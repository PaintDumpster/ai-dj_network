import os
import re
import json
import threading
import requests
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

GROQ_API_URL = 'https://api.groq.com/openai/v1/chat/completions'
GROQ_MODEL = 'llama-3.1-8b-instant'
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

        self.api_key = os.environ.get('GROQ_API_KEY')
        if not self.api_key:
            self.get_logger().error(
                'GROQ_API_KEY environment variable is not set. '
                'The node will publish the fallback message for every input.'
            )

        for topic in (
            'classification_results_surveillance',
            'classification_results_natural',
            'classification_results_cultural',
        ):
            self.create_subscription(String, topic, self._classification_callback, 10)

        self.avatar_pub = self.create_publisher(String, '/avatar_speech', 10)
        self.results_pub = self.create_publisher(String, '/model_results', 10)

        self.get_logger().info('LLM Node initialized')

    def _classification_callback(self, msg):
        """Receive a classification result, parse it, call Groq in a thread."""
        parsed = parse_results(msg.data)
        if parsed is None:
            self.get_logger().warn('Could not parse classification result message')
            return

        model_name, label, confidence, top3 = parsed
        self.get_logger().info(f'[{model_name}] Top result: {label} ({confidence:.2f})')

        threading.Thread(
            target=self._call_groq_and_publish,
            args=(model_name, label, confidence, top3),
            daemon=True
        ).start()

    def _call_groq_and_publish(self, model_name, label, confidence, top3):
        """Call the Groq API and publish combined result (runs in a background thread)."""
        sentence = self._call_groq(label, confidence)

        # Publish plain sentence to /avatar_speech (existing behaviour)
        out = String()
        out.data = sentence
        self.avatar_pub.publish(out)

        # Publish combined JSON to /model_results
        combined = String()
        combined.data = json.dumps({
            'model': model_name,
            'top3': top3,
            'sentence': sentence,
        })
        self.results_pub.publish(combined)

        self.get_logger().info(f'[{model_name}] {sentence}')

    def _call_groq(self, label, confidence):
        """Make the HTTP request to Groq. Returns the LLM text or the fallback."""
        if not self.api_key:
            return FALLBACK_MESSAGE

        body = {
            'model': GROQ_MODEL,
            'messages': [
                {'role': 'system', 'content': SYSTEM_PROMPT},
                {'role': 'user', 'content': f'Audio label: {label}. Confidence: {confidence:.2f}.'},
            ],
            'max_tokens': 80,
            'temperature': 0.7,
        }

        try:
            resp = requests.post(
                GROQ_API_URL,
                headers={
                    'Authorization': f'Bearer {self.api_key}',
                    'Content-Type': 'application/json',
                },
                json=body,
                timeout=5,
            )
            if not resp.ok:
                self.get_logger().error(f'Groq API error {resp.status_code}: {resp.text}')
            resp.raise_for_status()
            return resp.json()['choices'][0]['message']['content'].strip()
        except requests.exceptions.Timeout:
            self.get_logger().warn('Groq API request timed out')
        except requests.exceptions.RequestException as e:
            self.get_logger().error(f'Groq API request failed: {e}')
        except (KeyError, IndexError, ValueError) as e:
            self.get_logger().error(f'Unexpected Groq API response format: {e}')

        return FALLBACK_MESSAGE


def main():
    rclpy.init()
    node = LLMNode()
    rclpy.spin(node)
    rclpy.shutdown()


if __name__ == '__main__':
    main()
