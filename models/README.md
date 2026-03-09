# Models Directory

This directory contains machine learning models for audio classification.

## Required Files - Fine-Tuned Multi-Model Setup

This project uses **three fine-tuned YAMNet models**, each with unique class labels:

### Model 1 (Red) - Fine-tuned specialist
- **model1.onnx** - First fine-tuned model
- **model1_classes.txt** - Model 1 output class labels

### Model 2 (Green) - Fine-tuned specialist
- **model2.onnx** - Second fine-tuned model
- **model2_classes.txt** - Model 2 output class labels

### Model 3 (Blue) - Fine-tuned specialist
- **model3.onnx** - Third fine-tuned model
- **model3_classes.txt** - Model 3 output class labels

## Setup Instructions

Place your six files in this directory:
1. Copy your three `.onnx` model files → Rename to `model1.onnx`, `model2.onnx`, `model3.onnx`
2. Copy your three class label `.txt` files → Rename to `model1_classes.txt`, `model2_classes.txt`, `model3_classes.txt`

Ensure ONNX Runtime is installed (see main README.md for installation instructions).

## Model Information

### Fine-Tuned YAMNet Models
- **Base Architecture**: YAMNet (Google Research)
- **Training**: Fine-tuned on custom datasets
- **Purpose**: Specialized audio event classification
- **Input**: 15,600 audio samples (0.975s at 16kHz)
- **Output**: Model-specific class probabilities (varies per model)
- **Format**: ONNX

Each model has been trained on a unique dataset with its own set of output classes. The three models work in parallel to provide diverse audio classification perspectives, with results visualized in RGB colors on the LED matrix.

### Class Labels Format

Each `*_classes.txt` file should have one class name per line:
```
class_name_1
class_name_2
class_name_3
...
```

The line number corresponds to the output index from the model.

## Directory Structure

```
models/
├── README.md                    # This file
├── model1.onnx                  # Fine-tuned model 1 (Red channel)
├── model1_classes.txt           # Model 1 class labels
├── model2.onnx                  # Fine-tuned model 2 (Green channel)
├── model2_classes.txt           # Model 2 class labels
├── model3.onnx                  # Fine-tuned model 3 (Blue channel)
└── model3_classes.txt           # Model 3 class labels
```

## Verification

After placing your models, verify files exist:
```bash
cd models/
ls -lh
# Should show:
# model1.onnx (size varies)
# model1_classes.txt
# model2.onnx (size varies)
# model2_classes.txt
# model3.onnx (size varies)
# model3_classes.txt
```

## Usage

### Running Three Fine-Tuned Models

Each model instance needs its own ONNX file and class labels file:

```bash
# Terminal 1: Model 1 (Red - 255,0,0)
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=/home/salva/iaac/ai4all/rosnetwork/models/model1.onnx \
  -p class_names_path:=/home/salva/iaac/ai4all/rosnetwork/models/model1_classes.txt \
  -p model_name:=Model_1_Red \
  -p model_color_r:=255 -p model_color_g:=0 -p model_color_b:=0

# Terminal 2: Model 2 (Green - 0,255,0)
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=/home/salva/iaac/ai4all/rosnetwork/models/model2.onnx \
  -p class_names_path:=/home/salva/iaac/ai4all/rosnetwork/models/model2_classes.txt \
  -p model_name:=Model_2_Green \
  -p model_color_r:=0 -p model_color_g:=255 -p model_color_b:=0

# Terminal 3: Model 3 (Blue - 0,0,255)
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=/home/salva/iaac/ai4all/rosnetwork/models/model3.onnx \
  -p class_names_path:=/home/salva/iaac/ai4all/rosnetwork/models/model3_classes.txt \
  -p model_name:=Model_3_Blue \
  -p model_color_r:=0 -p model_color_g:=0 -p model_color_b:=255
```

### Using Launch File

Or use the launch file (update paths in `src/cpp_pkg/launch/three_yamnet_models.launch.py`):
```bash
ros2 launch cpp_pkg three_yamnet_models.launch.py
```
