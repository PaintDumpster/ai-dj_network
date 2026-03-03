# Models Directory

This directory contains machine learning models for audio classification.

## Required Files

### YAMNet Model Files

1. **yamnet.onnx** - YAMNet model in ONNX format (~3-4 MB)
2. **yamnet_class_names.txt** - List of 521 AudioSet class names

## Setup Instructions

Follow the [YAMNET_SETUP_GUIDE.md](../YAMNET_SETUP_GUIDE.md) to:
1. Install ONNX Runtime
2. Download and convert the YAMNet model
3. Set up the classification node

## Quick Setup

```bash
cd <workspace>/models

# Install dependencies
pip3 install tensorflow tf2onnx tensorflow-hub numpy

# Download and convert YAMNet (see YAMNET_SETUP_GUIDE.md for the conversion script)
python3 convert_yamnet.py
```

## Model Information

### YAMNet
- **Source**: [TensorFlow Hub](https://tfhub.dev/google/yamnet/1)
- **Purpose**: Audio event classification
- **Input**: 15,600 audio samples (0.975s at 16kHz)
- **Output**: 521 class probabilities (AudioSet classes)
- **Format**: ONNX

### AudioSet Classes

YAMNet can classify 521 different sound categories including:
- Music (instruments, genres, musical concepts)
- Human sounds (speech, singing, laughter, etc.)
- Animal sounds (dogs, cats, birds, etc.)
- Environmental sounds (rain, wind, traffic, etc.)
- Mechanical sounds (engines, tools, alarms, etc.)
- And many more...

Full class list is in `yamnet_class_names.txt` after setup.

## Directory Structure

```
models/
├── README.md                    # This file
├── yamnet.onnx                  # YAMNet ONNX model (to be downloaded)
├── yamnet_class_names.txt       # AudioSet class names (to be downloaded)
└── convert_yamnet.py            # Conversion script (from setup guide)
```

## Verification

After setup, verify files exist:
```bash
ls -lh
# Should show:
# yamnet.onnx (3-4 MB)
# yamnet_class_names.txt (few KB)
```

## Usage

The `yamnet_classification` node automatically loads these files. Default paths:
- Model: `<workspace>/models/yamnet.onnx`
- Classes: `<workspace>/models/yamnet_class_names.txt`

Override with ROS parameters if needed:
```bash
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=/custom/path/yamnet.onnx \
  -p class_names_path:=/custom/path/yamnet_class_names.txt
```
