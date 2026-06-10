#!/usr/bin/env python3
"""
Convert YamNet SavedModel to ONNX format
Extracts embeddings (1024-dim) from raw audio waveform input
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'  # Disable GPU
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'  # Reduce TF logging

import tensorflow as tf
import tf2onnx
import numpy as np

print("Loading YamNet SavedModel...")
model_path = "yamnet_savedmodel"
model = tf.saved_model.load(model_path)

print("\nAvailable signatures:")
for sig_name in model.signatures.keys():
    print(f"  - {sig_name}")

# Get the serving signature
infer = model.signatures['serving_default']

print("\nInput signature:")
for key, val in infer.structured_input_signature[1].items():
    print(f"  {key}: {val}")

print("\nOutput signature:")
for key, val in infer.structured_outputs.items():
    print(f"  {key}: {val}")

# Create a wrapper that outputs embeddings instead of class scores
@tf.function(input_signature=[tf.TensorSpec(shape=[None], dtype=tf.float32, name='audio')])
def yamnet_embeddings(waveform):
    """Extract 1024-dim embeddings from audio waveform"""
    scores, embeddings, spectrogram = model(waveform)
    return embeddings

# Test with sample input
print("\nTesting model with sample input...")
sample_audio = tf.random.normal([15600])  # 0.975s at 16kHz
embeddings = yamnet_embeddings(sample_audio)
print(f"Embeddings shape: {embeddings.shape}")
print(f"Embeddings dtype: {embeddings.dtype}")

# Convert to ONNX
print("\nConverting to ONNX...")
spec = (tf.TensorSpec((None,), tf.float32, name='audio'),)
output_path = "yamnet_embeddings.onnx"

model_proto, _ = tf2onnx.convert.from_function(
    yamnet_embeddings,
    input_signature=spec,
    opset=13,
    output_path=output_path
)

print(f"\n✓ Model converted successfully!")
print(f"✓ Saved to: {output_path}")
print(f"\nModel accepts:")
print(f"  Input: audio waveform, shape [num_samples], dtype float32")
print(f"  Output: embeddings, shape [num_frames, 1024], dtype float32")
print(f"\nNote: num_frames depends on audio length (~1 frame per 0.48s of audio)")
