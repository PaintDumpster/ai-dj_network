import os
import re
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
_TOP_RESULT_RE = re.compile(r'1\.\s+(.+?):\s+([\d.]+)')


def parse_top_result(text):
    """Return (label, confidence) from a classification_results message, or None."""
    match = _TOP_RESULT_RE.search(text)
    if not match:
        return None
    return match.group(1).strip(), float(match.group(2))


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

        self.publisher = self.create_publisher(String, '/avatar_speech', 10)

        self.get_logger().info('LLM Node initialized')

    def _classification_callback(self, msg):
        """Receive a classification result, parse it, call Groq in a thread."""
        result = parse_top_result(msg.data)
        if result is None:
            self.get_logger().warn('Could not parse classification result message')
            return

        label, confidence = result
        self.get_logger().info(f'Top result: {label} ({confidence:.2f})')

        threading.Thread(
            target=self._call_groq_and_publish,
            args=(label, confidence),
            daemon=True
        ).start()

    def _call_groq_and_publish(self, label, confidence):
        """Call the Groq API and publish the result (runs in a background thread)."""
        response_text = self._call_groq(label, confidence)
        out = String()
        out.data = response_text
        self.publisher.publish(out)
        self.get_logger().info(f'Published to /avatar_speech: {response_text}')

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
