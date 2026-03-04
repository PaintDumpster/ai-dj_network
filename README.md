# AI DJ - ROS2 Audio Classification & LED Visualization System

An interactive audio system that uses Arduino button inputs to build live waveforms, classifies them using multiple YAMNet models, and displays results on an RGB LED matrix in real-time.

## 🎯 Project Overview

This ROS2-based system integrates:
- **Hardware Interface**: Two controller boards
  - **Button Input Board** (Arduino Uno @ /dev/ttyACM0): 10 buttons for live audio composition + state control
  - **LED Control Board** (ESP32 @ /dev/ttyACM1): RGB LED matrix driver (42×146 pixels)
- **Waveform Generation**: Real-time audio waveform building from button presses
- **Multi-Model Classification**: Parallel YAMNet models for audio event detection
- **LED Visualization**: 42×146 RGB matrix displaying waveforms with classification highlights
- **Web Interface**: Real-time visualization and control via web browser

## 🏗️ System Architecture

```
┌──────────────────┐                    ┌──────────────────┐
│  Arduino Uno     │                    │      ESP32       │
│   Button Input   │                    │   LED Matrix     │
│   /dev/ttyACM0   │                    │   /dev/ttyACM1   │
│  (10 + 1 button) │                    │   (42×146 RGB)   │
└────────┬─────────┘                    └────────▲─────────┘
         │ Serial                                │ Serial
         ▼                                       │
┌──────────────┐                                 │
│    reader    │ → arduino_data topic            │
└──────┬───────┘                                 │
       ▼                                         │
┌──────────────────┐     ┌─────────────────────┐ │
│ build_waveform   │────→│   led_matrix topic  │ │
│  - Builds audio  │     │   (42×146 matrix)   │ │
│  - Saves CSV     │     └──────────┬──────────┘ │
└────────┬─────────┘                ▼            │
         │                  ┌──────────────┐     │
         │ waveform.csv     │    writer    │   ──┘
         ▼                  │  LED sender  │
┌────────────────────────┐  └──────▲───────┘
│ yamnet_classification  │         │
│   - Model 1 (Red)      │         │
│   - Model 2 (Green)    │─────────┘
│   - Model 3 (Blue)     │  led_paint_commands
└────────────────────────┘
         │
         ▼
┌──────────────────┐     ┌──────────────┐
│ classification_  │────→│  web_bridge  │
│ results topics   │     │   + webapp   │
└──────────────────┘     └──────────────┘
```

### Hardware Configuration

**Board 1 - Button Input** (Arduino Uno R3 → `/dev/ttyACM0` @ 9600 baud)
- Buttons 1-10: Audio composition triggers
- Button 11: State control (start/stop recording)
- Sends button numbers over serial
- USB Chip: ATmega16U2 (CDC ACM)

**Board 2 - LED Matrix** (ESP32 → `/dev/ttyACM1` @ 115200 baud)
- Controls 42×146 RGB LED matrix (6,132 pixels)
- Receives matrix updates and paint commands
- Protocol: Compressed format (position + RGB values)
- USB: Native USB JTAG/serial debug unit

## 📦 Package Structure

```
rosnetwork/
├── src/
│   ├── cpp_pkg/              # C++ nodes
│   │   ├── src/
│   │   │   ├── reader.cpp                 # Arduino serial reader
│   │   │   ├── build_waveform.cpp         # Waveform builder + LED matrix
│   │   │   ├── yamnet_classification.cpp  # YAMNet inference + painting
│   │   │   ├── writer.cpp                 # LED matrix writer
│   │   │   └── serial_port.cpp            # Serial utilities
│   │   └── launch/
│   │       └── three_yamnet_models.launch.py
│   └── py_pkg/               # Python nodes
│       └── py_pkg/
│           └── web_bridge.py             # Web interface bridge
├── models/                   # YAMNet ONNX models
├── sounds/                   # Button sound files
├── webapp/                   # Web visualization
└── setup_onnxruntime.sh     # ONNX Runtime installer

```

## 🚀 Quick Start

### 1. Prerequisites

```bash
# ROS2 Humble or later
sudo apt install ros-humble-desktop

# Build tools
sudo apt install python3-colcon-common-extensions

# Python dependencies (for model conversion and web interface)
pip3 install tensorflow tf2onnx tensorflow-hub fastapi uvicorn websockets
```

### 2. Install ONNX Runtime

```bash
cd ~/Downloads
wget https://github.com/microsoft/onnxruntime/releases/download/v1.24.2/onnxruntime-linux-x64-1.24.2.tgz
tar -xzf onnxruntime-linux-x64-1.24.2.tgz

sudo mkdir -p /opt/onnxruntime
sudo cp -r onnxruntime-linux-x64-1.24.2/* /opt/onnxruntime/
sudo ln -sf /opt/onnxruntime/lib/libonnxruntime.so* /usr/local/lib/
sudo ldconfig
```

Verify installation:
```bash
ldconfig -p | grep onnxruntime
# Should show: libonnxruntime.so.1.24.2 => /usr/local/lib/...
```

### 3. Set Up YAMNet Models

```bash
cd ~/iaac/ai4all/rosnetwork/models

# Create conversion script
cat > convert_yamnet.py << 'EOF'
import tensorflow as tf
import tensorflow_hub as hub
import tf2onnx

print("Loading YAMNet from TensorFlow Hub...")
model = hub.load('https://tfhub.dev/google/yamnet/1')

class YAMNetWrapper(tf.Module):
    def __init__(self, yamnet_model):
        super(YAMNetWrapper, self).__init__()
        self.yamnet = yamnet_model
    
    @tf.function(input_signature=[tf.TensorSpec(shape=[None], dtype=tf.float32)])
    def __call__(self, waveform):
        scores, embeddings, spectrogram = self.yamnet(waveform)
        return scores

wrapped_model = YAMNetWrapper(model)
spec = (tf.TensorSpec((None,), tf.float32, name="input"),)

print("Converting to ONNX...")
model_proto, _ = tf2onnx.convert.from_function(
    wrapped_model, input_signature=spec, output_path="yamnet.onnx"
)

# Save class names
class_names = list(model.class_names().numpy())
with open('yamnet_class_names.txt', 'w') as f:
    for name in class_names:
        f.write(name.decode('utf-8') + '\n')

print(f"✓ Model saved: yamnet.onnx")
print(f"✓ Classes saved: yamnet_class_names.txt ({len(class_names)} classes)")
EOF

python3 convert_yamnet.py
```

### 4. Build and Run

```bash
cd ~/iaac/ai4all/rosnetwork

# Build
colcon build --packages-select cpp_pkg py_pkg
source install/setup.bash

# Run complete system (separate terminals)

# Terminal 1: Button Input Reader (Arduino Uno)
ros2 run cpp_pkg reader --ros-args -p port:=/dev/ttyACM0 -p baud_rate:=9600

# Terminal 2: Waveform builder
ros2 run cpp_pkg build_waveform

# Terminal 3: LED Matrix Writer (ESP32)
ros2 run cpp_pkg writer --ros-args -p serial_port:=/dev/ttyACM1 -p baud_rate:=115200

# Terminal 4-6: YAMNet classifiers (RGB colors)
ros2 run cpp_pkg yamnet_classification --ros-args -p model_color_r:=255 -p model_color_g:=0 -p model_color_b:=0
ros2 run cpp_pkg yamnet_classification --ros-args -p model_color_r:=0 -p model_color_g:=255 -p model_color_b:=0
ros2 run cpp_pkg yamnet_classification --ros-args -p model_color_r:=0 -p model_color_g:=0 -p model_color_b:=255

# Terminal 7: Web interface (optional)
ros2 run py_pkg web_bridge
```

Or use the launch file for multiple models:
```bash
ros2 launch cpp_pkg three_yamnet_models.launch.py
```

## 🎵 Node Descriptions

### reader (Button Input - Arduino Uno)
Reads button press data from Arduino Uno button board via serial port.
- **Serial**: `/dev/ttyACM0` @ 9600 baud
- **Publishes**: 
  - `arduino_data` (std_msgs/String) - Button numbers 1-10
  - `state_control` (std_msgs/String) - Button 11 for state control
- **Parameters**: `port` (/dev/ttyACM0), `baud_rate` (9600)

### build_waveform
Builds audio waveforms from button presses and generates LED matrix data.
- **Subscribes**: `arduino_data`, `state_control`
- **Publishes**: `led_matrix` (std_msgs/UInt8MultiArray) - 42×146 grayscale waveform
- **Features**:
  - 30-second recording window
  - Real-time waveform calculation
  - CSV export to `/tmp/waveform_<timestamp>.csv`
  - LED matrix updates every 100ms
- **Parameters**:
  - `sounds_folder`: Path to button sound files
  - `recording_duration`: Recording time (default: 30.0s)
  - `sample_rate`: Audio sample rate (default: 44100)
  - `matrix_update_rate`: LED refresh rate (default: 0.1s)

### yamnet_classification
Performs audio classification using YAMNet and highlights relevant time segments.
- **Input**: Waveform CSV file
- **Publishes**: 
  - `classification_results` - Top-5 predictions with scores
  - `led_paint_commands` - Time segment coloring commands
- **Features**:
  - ONNX Runtime inference
  - Frame-by-frame analysis
  - Time-segment extraction for top classes
  - Model-specific RGB color assignment
- **Parameters**:
  - `model_path`: ONNX model file
  - `class_names_path`: Class labels file
  - `model_name`: Display name
  - `model_color_r/g/b`: RGB color (0-255)
  - `input_sample_rate`: Source sample rate (44100)
  - `yamnet_sample_rate`: Model sample rate (16000)

### writer (LED Matrix - ESP32)
Sends RGB matrix data to ESP32 LED control board via serial port.
- **Serial**: `/dev/ttyACM1` @ 115200 baud
- **Subscribes**:
  - `led_matrix` - Base waveform data (42×146 grayscale)
  - `led_paint_commands` - Classification highlights (RGB overlays)
- **Publishes**: Compressed matrix data to Arduino LED board
- **Features**:
  - RGB matrix composition (42×146×3 = 6,132 pixels)
  - Paint overlay management and re-application
  - Compressed data format (position + RGB values)
  - 10 Hz update rate (configurable)
- **Parameters**:
  - `serial_port`: LED board port (default: /dev/ttyACM1)
  - `baud_rate`: Communication speed (default: 115200)
  - `update_rate`: Matrix refresh interval (default: 0.1s)
  - Paint overlay management
  - Serial output formatting (ready for Arduino)
- **Parameters**:
  - `serial_port`: Arduino port (default: /dev/ttyACM0)
  - `update_rate`: Transmission rate (default: 0.1s)

### web_bridge (Python)
FastAPI WebSocket bridge for real-time web visualization.
- **Subscribes**: All classification results
- **Serves**: Web interface at `http://localhost:8000`

## 🎨 LED Matrix Visualization

### Matrix Dimensions
- **Width**: 146 columns (time axis, 30 seconds)
- **Height**: 42 rows (amplitude axis, -1.0 to +1.0)
- **Format**: RGB (3 bytes per pixel)

### Color Scheme
- **White (255,255,255)**: Base waveform
- **Red (255,0,0)**: Model 1 classification segments
- **Green (0,255,0)**: Model 2 classification segments
- **Blue (0,0,255)**: Model 3 classification segments

### Paint Command Format
```
PAINT,<start_time>,<end_time>,<R>,<G>,<B>
Example: PAINT,2.5,5.0,255,0,0
```

## 🔧 Configuration

### Sound Files
Place 10 WAV files in `sounds/`:
```
sounds/button_1.wav
sounds/button_2.wav
...
sounds/button_10.wav
```
If missing, generates placeholder sine waves (220-880 Hz).

### Multiple Model Setup
To run different models with different colors:
```bash
# Modify launch file or run manually with parameters
ros2 run cpp_pkg yamnet_classification --ros-args \
  -p model_path:=./models/yamnet_model1.onnx \
  -p model_name:=Music_Specialist \
  -p model_color_r:=255 \
  -p model_color_g:=100 \
  -p model_color_b:=0
```

## 🎓 YAMNet Audio Classes

YAMNet recognizes **521 AudioSet classes**:
- **Music**: Instruments, genres, musical concepts
- **Speech**: Conversation, narration, singing
- **Animals**: Dogs, cats, birds, insects
- **Nature**: Rain, wind, water, thunder
- **Human**: Laughter, crying, footsteps
- **Mechanical**: Engines, alarms, tools, vehicles

Full list in `models/yamnet_class_names.txt` after setup.

## 📊 Data Flow Example

1. **User presses Button 3** → Arduino sends "3"
2. **reader** publishes to `arduino_data` topic
3. **build_waveform** loads `sounds/button_3.wav`, adds to waveform matrix
4. **build_waveform** publishes updated LED matrix every 100ms
5. **writer** receives matrix, displays on LEDs (white)
6. After 30s, **build_waveform** saves `/tmp/waveform_1234.csv`
7. **yamnet_classification** processes waveform, detects "Piano" at 5.2-8.1s
8. **yamnet_classification** sends paint command: `PAINT,5.2,8.1,255,0,0`
9. **writer** overlays red color on LED columns 25-39
10. **web_bridge** displays results in browser

## 🛠️ Troubleshooting

### ONNX Runtime not found
```bash
# Check installation
ls /opt/onnxruntime/lib/libonnxruntime.so
sudo ldconfig

# If still not found, add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/onnxruntime/lib:$LD_LIBRARY_PATH
```

### Arduino not detected
```bash
# List serial ports
ls /dev/ttyUSB* /dev/ttyACM*

# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Build errors
```bash
# Clean build
cd ~/iaac/ai4all/rosnetwork
rm -rf build install log
colcon build --packages-select cpp_pkg --event-handlers console_direct+
```

### Model conversion fails
```bash
# Ensure TensorFlow dependencies
pip3 install --upgrade tensorflow tf2onnx tensorflow-hub

# Check Python version (3.8+ required)
python3 --version
```

## 📝 Development Notes

### Adding New Nodes
1. Create `.cpp` file in `src/cpp_pkg/src/`
2. Update `CMakeLists.txt` to add executable
3. Build: `colcon build --packages-select cpp_pkg`

### Testing Individual Nodes
```bash
# Test waveform builder without Arduino
ros2 topic pub /arduino_data std_msgs/String "data: '3'" --once

# Monitor LED matrix output
ros2 topic echo /led_matrix

# Check paint commands
ros2 topic echo /led_paint_commands
```

## 📄 License

[Add your license information]

## 🙏 Acknowledgments

- **YAMNet**: Google Research (AudioSet)
- **ONNX Runtime**: Microsoft
- **ROS2**: Open Robotics

