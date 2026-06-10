#!/usr/bin/env python3
"""
Convert YamNet from TensorFlow Hub to ONNX format.
Outputs YamNet.onnx (embeddings only, 1024-dim) in the same directory as this script.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

import tensorflow as tf
import tensorflow_hub as hub
import tf2onnx
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
OUTPUT_PATH = os.path.join(SCRIPT_DIR, 'YamNet.onnx')

print("Downloading YamNet from TensorFlow Hub (requires internet)...")
yamnet = hub.load('https://tfhub.dev/google/yamnet/1')
print("Download complete.")


@tf.function(input_signature=[tf.TensorSpec(shape=[None], dtype=tf.float32, name='audio')])
def yamnet_embeddings(waveform):
    """Extract 1024-dim embeddings from a raw audio waveform at 16kHz."""
    _, embeddings, _ = yamnet(waveform)
    return embeddings


print("\nTesting model with sample input...")
sample = tf.random.normal([16000])
out = yamnet_embeddings(sample)
print(f"Embeddings shape: {out.shape}")

print("\nConverting to ONNX...")
spec = (tf.TensorSpec((None,), tf.float32, name='audio'),)
model_proto, _ = tf2onnx.convert.from_function(
    yamnet_embeddings,
    input_signature=spec,
    opset=13,
    output_path=OUTPUT_PATH,
)

print(f"\nSaved to: {OUTPUT_PATH}")
print("Input : audio waveform [num_samples] float32 at 16kHz")
print("Output: embeddings [num_frames, 1024] float32 (~1 frame per 0.48s)")
