#!/usr/bin/env python3
"""
Test script: feed audio into the YAMNet → LLM pipeline.

Usage:
  python3 test_audio_input.py              # generates a 5s test tone
  python3 test_audio_input.py my_audio.wav # uses your own WAV file

The script writes /tmp/waveform_test.csv, which the yamnet_classification
nodes pick up when you call the classify services.
"""
import sys
import os
import numpy as np

OUTPUT_PATH = '/tmp/waveform_test.csv'
SAMPLE_RATE = 44100  # must match input_sample_rate in the launch file


def load_wav(path):
    from scipy.io import wavfile
    rate, data = wavfile.read(path)

    # Convert stereo to mono
    if data.ndim == 2:
        data = data.mean(axis=1)

    # Normalise to [-1, 1] float32
    data = data.astype(np.float32)
    if data.max() > 1.0:
        data /= np.iinfo(np.int16).max

    # Resample to SAMPLE_RATE if needed
    if rate != SAMPLE_RATE:
        from scipy.signal import resample
        num_samples = int(len(data) * SAMPLE_RATE / rate)
        data = resample(data, num_samples).astype(np.float32)
        print(f"Resampled from {rate} Hz to {SAMPLE_RATE} Hz")

    return data


def generate_test_tone(duration=5.0):
    """440 Hz sine wave — recognisable as a musical tone."""
    t = np.linspace(0, duration, int(SAMPLE_RATE * duration), endpoint=False)
    return (0.5 * np.sin(2 * np.pi * 440 * t)).astype(np.float32)


def save_csv(samples):
    with open(OUTPUT_PATH, 'w') as f:
        f.write('time,amplitude\n')
        for i, amp in enumerate(samples):
            f.write(f'{i},{amp}\n')
    print(f"Saved {len(samples)} samples to {OUTPUT_PATH}")


if __name__ == '__main__':
    if len(sys.argv) > 1:
        wav_path = sys.argv[1]
        if not os.path.exists(wav_path):
            print(f"File not found: {wav_path}")
            sys.exit(1)
        print(f"Loading {wav_path}...")
        samples = load_wav(wav_path)
    else:
        print("No WAV file provided — generating 5s test tone at 440 Hz...")
        samples = generate_test_tone()

    save_csv(samples)

    print("\nNow run (in separate terminals, after sourcing ROS):")
    print("  ros2 run py_pkg llm_node")
    print("  ros2 topic echo /avatar_speech")
    print("  ros2 launch cpp_pkg three_yamnet_models.launch.py")
    print("\nThen trigger classification:")
    print("  ros2 service call /classify_waveform_surveillance std_srvs/srv/Trigger '{}'")
    print("  ros2 service call /classify_waveform_natural std_srvs/srv/Trigger '{}'")
    print("  ros2 service call /classify_waveform_cultural std_srvs/srv/Trigger '{}'")
