import os
import re
import json
import threading
import anthropic
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

SYSTEM_PROMPT = (
    'You are a voice avatar. Convert audio classification results into a single '
    'warm, natural spoken English sentence. Never mention YAMNet, confidence scores, '
    'or any technical terms. If confidence is below 0.5, use uncertain language like '
    "'I think I hear...'. If confidence is above 0.5, use confident language like "
    "'I can hear...'. Respond with ONE sentence only, no punctuation beyond a "
    'period or exclamation mark.'
)
FALLBACK_MESSAGE = "I'm not sure what I heard just now."

# Matches lines like "1. Music: 0.8745"
_RESULT_RE = re.compile(r'(\d+)\.\s+(.+?):\s+([\d.]+)')
# Matches model name from header like "[surveillance] Classification Results:"
_MODEL_RE = re.compile(r'\[(\w+)\]')


def parse_results(text):
    """Return (model_name, top_label, top_confidence, top3_list) or None."""
    model_match = _MODEL_RE.search(text)
    model_name = model_match.group(1) if model_match else 'unknown'

    results = []
    for m in _RESULT_RE.finditer(text):
        results.append({'label': m.group(2).strip(), 'score': float(m.group(3))})

    if not results:
        return None

    top3 = results[:3]
    return model_name, top3[0]['label'], top3[0]['score'], top3


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

        model_name, label, confidence, top3 = parsed
        self.get_logger().info(f'[{model_name}] Top result: {label} ({confidence:.2f})')

        threading.Thread(
            target=self._call_claude_and_publish,
            args=(model_name, label, confidence, top3),
            daemon=True
        ).start()

    def _call_claude_and_publish(self, model_name, label, confidence, top3):
        """Call the Claude API and publish combined result (runs in a background thread)."""
        sentence = self._call_claude(label, confidence)

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

    def _call_claude(self, label, confidence):
        """Call the Anthropic Messages API. Returns the LLM text or the fallback."""
        if not self.client:
            return FALLBACK_MESSAGE

        model = os.environ.get('CLAUDE_MODEL', 'claude-haiku-4-5-20251001')
        try:
            msg = self.client.messages.create(
                model=model,
                max_tokens=80,
                system=SYSTEM_PROMPT,
                messages=[{
                    'role': 'user',
                    'content': f'Audio label: {label}. Confidence: {confidence:.2f}.',
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
