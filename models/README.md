# Models Directory

This directory contains machine learning models for audio classification.

## Two-Stage Inference Architecture

The system uses **two-stage inference** with YAMNet embeddings:

### Stage 1: Base YAMNet Model
- **YamNet.onnx** (14MB) - Extracts 1024-dimensional embeddings from raw audio
- **Input**: Raw audio waveform [num_samples], float32, 16kHz sample rate
- **Output**: Embeddings [num_frames, 1024], ~1 frame per 0.48s of audio
- **Source**: Converted from TensorFlow Hub YAMNet SavedModel
- **Conversion**: Use `convert_yamnet_to_onnx.py` to regenerate

### Stage 2: Fine-Tuned Classification Heads

Three specialized classification heads trained on YAMNet embeddings:

#### Surveillance Model (Red)
- **surveillance_head.onnx** - Surveillance audio classification
- **surveillance_classes.txt** - Output class labels (9 classes)
- **Input**: Averaged embeddings [1, 1024]
- **Output**: Class probabilities

#### Natural Model (Green)
- **natural_head.onnx** - Natural soundscape classification
- **natural_classes.txt** - Output class labels (9 classes)
- **Input**: Averaged embeddings [1, 1024]
- **Output**: Class probabilities

#### Cultural Model (Blue)
- **cultural_head.onnx** - Cultural audio classification
- **cultural_classes.txt** - Output class labels (10 classes)
- **Input**: Averaged embeddings [1, 1024]
- **Output**: Class probabilities

## Setup Instructions

### Option 1: Use Pre-Converted Models (Quickstart)

If you have the ONNX files already:
1. Place `YamNet.onnx` in this directory (base model)
2. Place the three `*_head.onnx` classification heads
3. Ensure corresponding `*_classes.txt` files exist

### Option 2: Convert YAMNet from TensorFlow

If you need to regenerate the base YAMNet model:

```bash
cd models/
python3 -m venv .venv
source .venv/bin/activate
pip install tensorflow tf2onnx onnx

# Run conversion (downloads YAMNet SavedModel from Kaggle)
python convert_yamnet_to_onnx.py

# Cleanup (optional)
deactivate
rm -rf .venv yamnet_model.tar.gz yamnet_savedmodel/
```

This creates `YamNet.onnx` (~14MB) with embeddings output.

## Inference Pipeline

The C++ classification nodes perform two-stage inference:

```
Raw Audio Waveform (variable length)
         ↓
   YamNet.onnx
         ↓
Embeddings [num_frames, 1024]
         ↓
   Average across frames
         ↓
Averaged Embeddings [1, 1024]
         ↓
   *_head.onnx (classification)
         ↓
Class Probabilities
```

Each audio sample produces multiple embedding frames (one per ~0.48s), which are averaged before classification.

## Directory Structure

```
models/
├── README.md                      # This file
├── convert_yamnet_to_onnx.py      # YAMNet conversion script
├── YamNet.onnx                    # Base embedding model (14MB) [gitignored]
├── surveillance_head.onnx         # Surveillance classifier [gitignored]
├── surveillance_classes.txt       # Surveillance labels
├── natural_head.onnx              # Natural sounds classifier [gitignored]
├── natural_classes.txt            # Natural labels
├── cultural_head.onnx             # Cultural audio classifier [gitignored]
├── cultural_classes.txt           # Cultural labels
└── yamnet_all_classes.txt         # Base YAMNet classes (reference)
```

**Note**: `.onnx` model files are gitignored due to size. Regenerate using `convert_yamnet_to_onnx.py` or obtain from project maintainers.

## Verification

After setup, verify required files exist:
```bash
cd models/
ls -lh *.onnx *.txt

# Expected output:
# YamNet.onnx (~14MB)
# surveillance_head.onnx (~2.6MB)
# natural_head.onnx (~2.6MB)
# cultural_head.onnx (~2.6MB)
# surveillance_classes.txt
# natural_classes.txt
# cultural_classes.txt
```

## Usage

### Launch All Three Classification Nodes

```bash
# Automated launch (recommended)
ros2 launch cpp_pkg three_yamnet_models.launch.py

# Or manually (3 terminals):
# Terminal 1: Surveillance
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_name:=surveillance

# Terminal 2: Natural
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_name:=natural

# Terminal 3: Cultural
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_name:=cultural
```

Each node:
- Loads `YamNet.onnx` for embeddings extraction
- Loads corresponding `*_head.onnx` for classification
- Subscribes to waveform generation service
- Publishes to `classification_results_*` topic

## Model Information

### YamNet Base Model
- **Architecture**: MobileNetV1 with audio-specific modifications
- **Original Training**: AudioSet (2 million samples, 521 classes)
- **Embedding Size**: 1024 dimensions
- **Frame Rate**: ~2.08 frames/second (0.48s per frame)
- **License**: Apache 2.0

### Classification Heads
- **Training**: Fine-tuned on custom Barcelona district audio datasets
- **Architecture**: Lightweight fully-connected layers on top of frozen YAMNet embeddings
- **Input**: 1024-dim embeddings (averaged across time)
- **Output**: Softmax probabilities (9-10 classes per head)

### Class Labels Format

Each `*_classes.txt` file has one class name per line:
```
class_name_1
class_name_2
class_name_3
...
```

Line number corresponds to model output index (0-indexed).

## System Requirements

- **ONNX Runtime**: C++ library (system-wide installation required)
- **Sample Rate**: 16kHz (audio resampled automatically if different)
- **Memory**: ~50MB per classification node (models loaded in RAM)
- **CPU**: Multi-threaded inference supported

For ONNX Runtime installation, see main [README.md](../README.md).
  -p model_color_r:=0 -p model_color_g:=255 -p model_color_b:=0

# Terminal 3: Cultural Model (Blue - 0,0,255)
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=/home/salva/iaac/ai4all/rosnetwork/models/cultural_head.onnx \
  -p class_names_path:=/home/salva/iaac/ai4all/rosnetwork/models/cultural_classes.txt \
  -p model_name:=Cultural \
  -p model_color_r:=0 -p model_color_g:=0 -p model_color_b:=255
```

### Using Launch File

Or use the launch file (update paths in `src/cpp_pkg/launch/three_yamnet_models.launch.py`):
```bash
ros2 launch cpp_pkg three_yamnet_models.launch.py
```
