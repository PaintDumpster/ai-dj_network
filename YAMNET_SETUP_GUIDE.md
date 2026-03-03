# YAMNet Classification Setup Guide

This guide walks you through setting up ONNX Runtime and YAMNet for audio classification in your ROS2 C++ node.

## Prerequisites

- Ubuntu 22.04 or compatible Linux distribution
- ROS2 (Humble or later)
- Python 3.8+ (for model conversion)
- Cmake 3.20+

---

## Step 1: Install ONNX Runtime

### Option A: Install Pre-built Library (Recommended)

```bash
# Download ONNX Runtime for Linux
cd ~/Downloads
wget https://github.com/microsoft/onnxruntime/releases/download/v1.24.2/onnxruntime-linux-x64-1.24.2.tgz

# Extract
tar -xzf onnxruntime-linux-x64-1.24.2.tgz

# Install to system (requires sudo)
sudo mkdir -p /opt/onnxruntime
sudo cp -r onnxruntime-linux-x64-1.24.2/* /opt/onnxruntime/

# Create symlinks for easier discovery
sudo ln -sf /opt/onnxruntime/include /usr/local/include/onnxruntime
sudo ln -sf /opt/onnxruntime/lib/libonnxruntime.so /usr/local/lib/libonnxruntime.so
sudo ln -sf /opt/onnxruntime/lib/libonnxruntime.so.1.24.2 /usr/local/lib/libonnxruntime.so.1.24.2

# Update library cache
sudo ldconfig
```

### Option B: Build from Source (Advanced)

```bash
git clone --recursive https://github.com/microsoft/onnxruntime.git
cd onnxruntime
./build.sh --config Release --build_shared_lib --parallel
sudo cmake --install build/Linux/Release
```

### Verify Installation

```bash
# Check if library is found
ldconfig -p | grep onnxruntime

# Should output something like:
# libonnxruntime.so.1.24.2 (libc6,x86-64) => /usr/local/lib/libonnxruntime.so.1.24.2
# libonnxruntime.so (libc6,x86-64) => /usr/local/lib/libonnxruntime.so
```

---

## Step 2: Download and Convert YAMNet Model

### Install Python Dependencies

```bash
pip3 install tensorflow tf2onnx numpy
```

### Download YAMNet from TensorFlow Hub

```bash
cd <workspace>/models

# Create Python script to download and convert YAMNet
cat > convert_yamnet.py << 'EOF'
import tensorflow as tf
import tensorflow_hub as hub
import tf2onnx
import numpy as np

# Load YAMNet model from TensorFlow Hub
print("Loading YAMNet model from TensorFlow Hub...")
model = hub.load('https://tfhub.dev/google/yamnet/1')

# Create a wrapper for the model
class YAMNetWrapper(tf.Module):
    def __init__(self, yamnet_model):
        super(YAMNetWrapper, self).__init__()
        self.yamnet = yamnet_model
    
    @tf.function(input_signature=[tf.TensorSpec(shape=[None], dtype=tf.float32)])
    def __call__(self, waveform):
        scores, embeddings, spectrogram = self.yamnet(waveform)
        return scores

# Wrap the model
wrapped_model = YAMNetWrapper(model)

# Convert to ONNX
print("Converting to ONNX format...")
spec = (tf.TensorSpec((None,), tf.float32, name="input"),)
output_path = "yamnet.onnx"

model_proto, _ = tf2onnx.convert.from_function(
    wrapped_model,
    input_signature=spec,
    output_path=output_path
)

print(f"Model saved to {output_path}")

# Download class names
print("Downloading class names...")
import urllib.request
class_map_path = model.class_map_path().numpy()
class_names = list(model.class_names().numpy())

with open('yamnet_class_names.txt', 'w') as f:
    for name in class_names:
        f.write(name.decode('utf-8') + '\n')

print(f"Class names saved to yamnet_class_names.txt ({len(class_names)} classes)")
EOF

# Run the conversion script
python3 convert_yamnet.py
```

### Alternative: Download Pre-converted Model

If the conversion script has issues, you can download a pre-converted ONNX model:

```bash
cd <workspace>/models

# Note: You may need to convert it yourself as YAMNet ONNX versions vary
# The conversion script above is the recommended approach
```

---

## Step 3: Verify Model Files

```bash
cd <workspace>/models
ls -lh

# You should see:
# yamnet.onnx (approx 3-4 MB)
# yamnet_class_names.txt (521 class names)
```

---

## Step 4: Build the ROS2 Package

```bash
cd <workspace>
colcon build --packages-select cpp_pkg

# If ONNX Runtime is found, you should see:
# -- ONNX Runtime found:
# --   Include: /opt/onnxruntime/include
# --   Library: /opt/onnxruntime/lib/libonnxruntime.so
# -- yamnet_classification node will be built
```

---

## Step 5: Run the YAMNet Classifier

### Terminal 1: Start the WaveformBuilder (if needed)
```bash
source install/setup.bash
ros2 run cpp_pkg build_waveform
```

### Terminal 2: Start YAMNet Classifier

```bash
source install/setup.bash
ros2 run cpp_pkg yamnet_classification
```

### Or classify a specific waveform file:

```bash
source install/setup.bash
ros2 run cpp_pkg yamnet_classification /tmp/waveform_12345.csv
```

---

## Configuration Parameters

You can customize the classifier behavior:

```bash
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=/path/to/yamnet.onnx \
  -p class_names_path:=/path/to/yamnet_class_names.txt \
  -p input_sample_rate:=44100 \
  -p yamnet_sample_rate:=16000
```

### Parameters:
- `model_path`: Path to YAMNet ONNX model (default: `<workspace>/models/yamnet.onnx`)
- `class_names_path`: Path to class names file (default: `<workspace>/models/yamnet_class_names.txt`)
- `input_sample_rate`: Sample rate of input waveform (default: 44100 Hz)
- `yamnet_sample_rate`: YAMNet expected sample rate (default: 16000 Hz)

---

## Troubleshooting

### ONNX Runtime not found during build

```bash
# Check if library is installed
ls /opt/onnxruntime/
ls /usr/local/lib/libonnxruntime*

# If not found, reinstall following Step 1
```

### Model fails to load

```bash
# Verify model file exists
ls -lh <workspace>/models/yamnet.onnx

# Check model with Python
python3 -c "import onnx; model = onnx.load('models/yamnet.onnx'); print('Model loaded successfully')"
```

### Inference errors

- Ensure waveform CSV format matches expected format (time, amplitude)
- Check that audio is properly normalized (-1.0 to 1.0)
- Verify sample rate parameters match your audio source

---

## YAMNet Model Details

- **Input**: 15,600 samples (0.975 seconds at 16 kHz)
- **Output**: 521 class probabilities
- **Classes**: AudioSet ontology (music, speech, environmental sounds, etc.)
- **Sample Rate**: 16 kHz

The node automatically handles:
- Resampling from 44.1 kHz to 16 kHz
- Padding/truncating to 0.975 seconds
- Normalization to [-1, 1] range

---

## Integration with build_waveform Node

The classifier can work with waveforms generated by the `build_waveform` node:

1. User presses buttons on Arduino (30 seconds)
2. `build_waveform` creates waveform CSV in `/tmp/`
3. `yamnet_classification` processes the waveform
4. Classification results published to `classification_results` topic

---

## Next Steps

- Implement automatic waveform file monitoring
- Add ROS service interface for on-demand classification
- Create visualization node for results
- Add support for real-time streaming classification
- Integrate with database for storing results

---

## References

- [YAMNet on TensorFlow Hub](https://tfhub.dev/google/yamnet/1)
- [ONNX Runtime Documentation](https://onnxruntime.ai/)
- [AudioSet Ontology](https://research.google.com/audioset/)
