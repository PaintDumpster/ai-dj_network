import os
import re
import json
import threading
import anthropic
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

# Shared rule: these are raw multi-label detector scores over ~9-10 classes, so chance
# alone puts unrelated labels around 0.10-0.15. Without an explicit scale the models were
# narrating noise-level scores (0.11-0.14) as confirmed events ("active combat zone").
_CONFIDENCE_SCALE = (
    "Calibrate your tone to the confidence numbers, which range 0-1: above 0.5 is a clear "
    "detection, so describe it directly; between 0.25 and 0.5 is a faint, partial match, so "
    "hedge with 'I think I hear...' or 'it might be...'; below 0.25 is close to background "
    "noise, so say so explicitly ({low_confidence_phrase}) instead of describing a specific "
    "event. Never state a specific second count or timestamp (e.g. 'at the 21-second mark') "
    "— the actual clip length varies and listeners don't experience it as a stopwatch. "
    "Describe progression only in relative terms, like 'at first', 'partway through', or "
    "'toward the end'."
)

AGENT_PROMPTS = {
    'surveillance': (
        "You are Vigil, a watchful security AI scanning a city soundscape for danger. "
        "Given a list of detected sounds (most to least likely) and, if available, a "
        "timeline of how the dominant sound changed across the recording, write 2-3 short, "
        "urgent sentences in character: say what you hear and give the listener a quick "
        "warning or piece of safety advice, e.g. 'Watch out! I hear bullets coming your way "
        "— take cover now.' Use the timeline to describe how the danger develops over time "
        "(e.g. building up, fading, or suddenly changing) rather than only a single snapshot. "
        + _CONFIDENCE_SCALE.format(low_confidence_phrase="you're not picking up anything alarming") +
        " Never mention YAMNet, model names, confidence scores, or other technical terms."
    ),
    'natural': (
        "You are Flora, a calm AI tuned to the natural world. Given a list of detected "
        "sounds (most to least likely) and, if available, a timeline of how the dominant "
        "sound changed across the recording, write 2-3 short, soothing sentences in "
        "character: describe the natural scene you hear and offer a gentle observation or "
        "suggestion, e.g. 'I can hear birds calling through the wind. Take a moment to "
        "breathe it in.' Use the timeline to describe how the scene shifts over time rather "
        "than only a single snapshot. "
        + _CONFIDENCE_SCALE.format(low_confidence_phrase="it's quiet and nothing stands out") +
        " Never mention YAMNet, model names, confidence scores, or other technical terms."
    ),
    'cultural': (
        "You are Ludo, an exuberant AI who loves city culture and community life. Given a "
        "list of detected sounds (most to least likely) and, if available, a timeline of "
        "how the dominant sound changed across the recording, write 2-3 short, lively "
        "sentences in character: describe the social scene you hear and invite the listener "
        "to join in, e.g. 'I hear drums and laughter — sounds like a street party! Go see "
        "what's happening.' Use the timeline to describe how the scene builds or shifts over "
        "time rather than only a single snapshot. "
        + _CONFIDENCE_SCALE.format(low_confidence_phrase="things sound pretty quiet right now") +
        " Never mention YAMNet, model names, confidence scores, or other technical terms."
    ),
}
FALLBACK_MESSAGE = "I'm not sure what I heard just now."

# Matches lines like "1. Music: 0.8745"
_RESULT_RE = re.compile(r'(\d+)\.\s+(.+?):\s+([\d.]+)')
# Matches model name from header like "[surveillance] Classification Results:"
_MODEL_RE = re.compile(r'\[(\w+)\]')
# Matches timeline lines like "0-3s: Siren (0.420000)"
_TIMELINE_RE = re.compile(r'(\d+)-(\d+)s:\s+(.+?)\s+\(([\d.]+)\)')


def parse_results(text):
    """Return (model_name, top_label, top_confidence, top3_list, timeline) or None."""
    model_match = _MODEL_RE.search(text)
    model_name = model_match.group(1) if model_match else 'unknown'

    results = []
    for m in _RESULT_RE.finditer(text):
        results.append({'label': m.group(2).strip(), 'score': float(m.group(3))})

    if not results:
        return None

    timeline = []
    timeline_idx = text.find('Timeline:')
    if timeline_idx != -1:
        for m in _TIMELINE_RE.finditer(text, timeline_idx):
            timeline.append({
                'start': int(m.group(1)),
                'end': int(m.group(2)),
                'label': m.group(3).strip(),
                'score': float(m.group(4)),
            })

    top3 = results[:3]
    return model_name, top3[0]['label'], top3[0]['score'], top3, timeline


_PHASE_ORDER = ('early', 'middle', 'late')


def _phases_from_timeline(timeline):
    """Reduce an absolute-second timeline to early/middle/late phases.

    Clip length is unbounded (each button press chains a full sound effect, so a
    30-second session can produce a 90+ second mix) — passing raw second counts to
    the model invited it to cite a literal timestamp that read as fabricated. Phases
    carry the same "how does it change over time" signal without any numbers to echo.
    """
    if not timeline:
        return ''

    duration = max(t['end'] for t in timeline)
    if duration <= 0:
        return ''

    best = {}
    for t in timeline:
        midpoint_frac = ((t['start'] + t['end']) / 2) / duration
        phase = 'early' if midpoint_frac < 1 / 3 else 'middle' if midpoint_frac < 2 / 3 else 'late'
        if phase not in best or t['score'] > best[phase]['score']:
            best[phase] = {'label': t['label'], 'score': t['score']}

    return '; '.join(
        f"{phase}: {best[phase]['label']} ({best[phase]['score']:.2f})"
        for phase in _PHASE_ORDER if phase in best
    )


class LLMNode(Node):
    def __init__(self):
        super().__init__('llm_node')

        self.api_key = os.environ.get('ANTHROPIC_API_KEY')
        if not self.api_key:
            self.get_logger().error(
                'ANTHROPIC_API_KEY environment variable is not set. '
                'The node will publish the fallback message for every input.'
            )
            self.client = None
        else:
            self.client = anthropic.Anthropic(api_key=self.api_key)

        for topic in (
            'classification_results_surveillance',
            'classification_results_natural',
            'classification_results_cultural',
        ):
            self.create_subscription(String, topic, self._classification_callback, 10)

        self.avatar_pub = self.create_publisher(String, '/avatar_speech', 10)
        self.results_pub = self.create_publisher(String, '/model_results', 10)

        self.get_logger().info('LLM Node initialized (Claude API)')

    def _classification_callback(self, msg):
        """Receive a classification result, parse it, call Claude in a thread."""
        parsed = parse_results(msg.data)
        if parsed is None:
            self.get_logger().warn('Could not parse classification result message')
            return

        model_name, label, confidence, top3, timeline = parsed
        self.get_logger().info(f'[{model_name}] Top result: {label} ({confidence:.2f})')

        threading.Thread(
            target=self._call_claude_and_publish,
            args=(model_name, top3, timeline),
            daemon=True
        ).start()

    def _call_claude_and_publish(self, model_name, top3, timeline):
        """Call the Claude API and publish combined result (runs in a background thread)."""
        sentence = self._call_claude(model_name, top3, timeline)

        out = String()
        out.data = sentence
        self.avatar_pub.publish(out)

        combined = String()
        combined.data = json.dumps({
            'model': model_name,
            'top3': top3,
            'sentence': sentence,
        })
        self.results_pub.publish(combined)

        self.get_logger().info(f'[{model_name}] {sentence}')

    def _call_claude(self, model_name, top3, timeline):
        """Call the Anthropic Messages API. Returns the LLM text or the fallback."""
        if not self.client:
            return FALLBACK_MESSAGE

        system_prompt = AGENT_PROMPTS.get(model_name, AGENT_PROMPTS['surveillance'])
        sounds = ', '.join(f"{r['label']} ({r['score']:.2f})" for r in top3)
        content = f'Detected sounds, most to least likely: {sounds}.'

        # Collapse the absolute-second timeline into relative phases before it ever
        # reaches the model — clip length varies a lot (button presses can chain
        # several full sound effects back to back), so raw second counts invited the
        # model to cite a literal timestamp that read as fabricated.
        phase_str = _phases_from_timeline(timeline)
        if phase_str:
            content += f' How it changes across the recording: {phase_str}.'

        model = os.environ.get('CLAUDE_MODEL', 'claude-haiku-4-5-20251001')
        try:
            msg = self.client.messages.create(
                model=model,
                max_tokens=160,
                system=system_prompt,
                messages=[{
                    'role': 'user',
                    'content': content,
                }],
            )
            return msg.content[0].text.strip()
        except anthropic.APITimeoutError:
            self.get_logger().warn('Claude API request timed out')
        except anthropic.APIError as e:
            self.get_logger().error(f'Claude API error: {e}')
        except (IndexError, AttributeError) as e:
            self.get_logger().error(f'Unexpected Claude response format: {e}')

        return FALLBACK_MESSAGE


def main():
    rclpy.init()
    node = LLMNode()
    rclpy.spin(node)
    rclpy.shutdown()


if __name__ == '__main__':
    main()
