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
         │ waveform.csv     │    writer    │─────┘
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

## ✅ Current Status

### Hardware & Arduino
- ✅ **Arduino Uno R3** configured with 4x4 Elegoo membrane keypad
- ✅ **Button sketch** (`sketches/button_reader/`) uploaded and tested
- ✅ **ESP32** connected and recognized at `/dev/ttyACM1`
- ⏳ **ESP32 LED sketch** - Not yet implemented
- 🔧 **Keypad layout**:
  ```
  1  2  3  a     →  Buttons 1-3 (sound triggers)
  4  5  6  b     →  Buttons 4-6 (sound triggers)
  7  8  9  c     →  Buttons 7-9 (sound triggers)
  *  0  #  d     →  Button 11 (*), Button 10 (0)
  ```

### ROS2 Nodes
- ✅ **reader** - Fully tested, publishes button presses to topics
  - `/arduino_data` for buttons 1-10
  - `/state_control` for button 11 (start/stop recording)
- ✅ **build_waveform** - Fully tested
  - Generates placeholder sine waves (220-880 Hz) for missing sound files
  - Publishes real-time LED matrix data (42×146)
  - Saves CSV waveforms to `/tmp/waveform_<timestamp>.csv` (661k samples, ~12 MB)
  - Recording controlled by button 11 or ROS2 services
- ✅ **writer** - Running and sending data
  - Receiving LED matrix data
  - Formatting and sending to ESP32 via serial
  - Compressed format: `MATRIX:42x146:row,col,R,G,B;...`
- ⏳ **yamnet_classification** - Not yet tested
- ⏳ **web_bridge** - Not yet tested

### Test Results
- ✅ Button presses detected and logged
- ✅ State control (button 11) toggles recording
- ✅ Waveform generation working with placeholder audio
- ✅ LED matrix data published and transmitted
- ⏳ LED display not visible yet (ESP32 sketch needed)
- ⏳ YAMNet classification pending

### Next Steps
1. **Create ESP32 sketch** for LED matrix control
2. **Add sound files** to `sounds/` directory (optional - placeholders work)
3. **Set up YAMNet models** for audio classification
4. **Test classification** and color overlays
5. **Implement web interface** for remote monitoring

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
├── sketches/                 # Arduino sketches
│   ├── button_reader/        # Arduino Uno keypad reader (TESTED ✅)
│   └── README.md             # Arduino setup instructions
├── models/                   # YAMNet ONNX models
├── sounds/                   # Button sound files (optional)
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

## ✅ Tested Configuration (March 2026)

Successfully tested with minimal setup - no sound files or YAMNet models required:

### Hardware
- Arduino Uno R3 (ATmega16U2) on `/dev/ttyACM0`
- Elegoo 4x4 membrane keypad connected to pins 2-9
- ESP32 (USB JTAG) on `/dev/ttyACM1`

### Arduino Setup
```bash
# Install Keypad library
arduino-cli lib install Keypad

# Compile and upload
cd sketches/button_reader
arduino-cli compile --fqbn arduino:avr:uno .
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:uno .
```

### ROS2 System Test
```bash
# Build
cd ~/iaac/ai4all/rosnetwork
colcon build --packages-select cpp_pkg
source install/setup.bash

# Terminal 1: Reader
ros2 run cpp_pkg reader

# Terminal 2: Waveform Builder
ros2 run cpp_pkg build_waveform --ros-args \
  -p sounds_folder:=/home/salva/iaac/ai4all/rosnetwork/sounds

# Terminal 3: Writer
ros2 run cpp_pkg writer
```

### Test Procedure
1. Press `*` button → Starts recording (30 seconds)
2. Press number buttons 0-9 → Generates placeholder sine waves
3. Wait 30s or press `*` again → Stops recording
4. Check output: `/tmp/waveform_<timestamp>.csv` (~12 MB, 661k samples)

### Test Results
- ✅ All buttons detected correctly
- ✅ Button 11 (*) toggles recording on/off
- ✅ Placeholder audio generated (220-880 Hz sine waves)
- ✅ Real-time LED matrix data published to `/led_matrix` topic
- ✅ CSV waveform saved successfully
- ✅ Writer sending formatted data to ESP32 serial port

### Topics Active During Test
```bash
$ ros2 topic list
/arduino_data         # Buttons 1-10
/state_control        # Button 11
/led_matrix          # 42×146 grayscale waveform
/led_paint_commands  # Color overlay commands
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

### Sound Files (Optional - Tested without!)
Place 10 WAV files in `sounds/`:
```
sounds/button_1.wav
sounds/button_2.wav
...
sounds/button_10.wav
```
**If missing, automatically generates placeholder sine waves:**
- Button 1: 220 Hz (A3)
- Button 2: 247 Hz (B3)
- Button 3: 262 Hz (C4 - Middle C)
- Button 4: 294 Hz (D4)
- Button 5: 330 Hz (E4)
- Button 6: 349 Hz (F4)
- Button 7: 392 Hz (G4)
- Button 8: 440 Hz (A4)
- Button 9: 494 Hz (B4)
- Button 10: 880 Hz (A5)

This allows **full system testing without any audio files!** ✅

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

### Serial Port Permission Denied
The most common issue during testing:
```bash
# Permanent fix (requires logout/login):
sudo usermod -a -G dialout $USER
# Then log out and back in

# Temporary fix (for current session):
sudo chmod 666 /dev/ttyACM0
sudo chmod 666 /dev/ttyACM1
```

### Both Arduino and ESP32 show as ttyACM (not ttyUSB)
This is correct for:
- Arduino Uno R3 (ATmega16U2 USB chip) → `/dev/ttyACM0`
- ESP32 with native USB JTAG → `/dev/ttyACM1`

Check with:
```bash
lsusb
# Should show:
# Arduino SA Uno R3 (CDC ACM)
# Espressif USB JTAG/serial debug unit

ls -l /dev/ttyACM*
```

### Button 11 not triggering recording
If button 11 presses don't start/stop recording, check that the reader is stripping carriage returns:
```bash
# Check topic data (should show "11", not "11\r"):
ros2 topic echo /state_control

# If you see "\r", rebuild with latest code
cd ~/iaac/ai4all/rosnetwork
colcon build --packages-select cpp_pkg
```

### No waveform being built
Make sure to press button 11 (*) to **start recording** before pressing sound buttons.

### Arduino not detected
```bash
# List serial ports
ls /dev/ttyUSB* /dev/ttyACM*

# Check which device it is
lsusb | grep -i arduino
```

### ONNX Runtime not found
```bash
# Check installation
ls /opt/onnxruntime/lib/libonnxruntime.so
sudo ldconfig

# If still not found, add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/onnxruntime/lib:$LD_LIBRARY_PATH
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

