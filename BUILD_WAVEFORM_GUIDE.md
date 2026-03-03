# Build Waveform Node - Usage Guide

## Overview
This ROS2 node receives button press data from Arduino (via the `arduino_data` topic) and builds a waveform by combining sounds associated with each button (1-10). The user has 30 seconds to press buttons and build their waveform live.

## Features
- **10 Button Support**: Handles button numbers 1-10 from Arduino
- **Sound Mapping**: Each button is linked to a sound file in the `sounds/` folder
- **Live Waveform Building**: Waveform is constructed in real-time as buttons are pressed
- **30-Second Recording**: Automatically stops after 30 seconds
- **Placeholder Sounds**: Generates sine waves if sound files are missing
- **CSV Export**: Saves final waveform to `/tmp/waveform_<timestamp>.csv`

## Setup

### 1. Place Sound Files
Add your .wav files to the `sounds/` folder:
```bash
<workspace>/sounds/
├── button_1.wav
├── button_2.wav
├── ...
└── button_10.wav
```

### 2. Build the Package
```bash
cd <workspace>
colcon build --packages-select cpp_pkg
source install/setup.bash
```

## Running the Node

### Terminal 1: Start the Arduino Reader
```bash
source install/setup.bash
ros2 run cpp_pkg reader --ros-args -p port:=/dev/ttyUSB0 -p baud_rate:=9600
```

### Terminal 2: Start the Waveform Builder
```bash
source install/setup.bash
ros2 run cpp_pkg build_waveform
```

**Optional Parameters:**
```bash
ros2 run cpp_pkg build_waveform --ros-args \
  -p sounds_folder:=/path/to/sounds \
  -p recording_duration:=30.0 \
  -p sample_rate:=44100
```

## How It Works

1. **Idle State**: Node waits for the first button press
2. **Recording Starts**: First button press triggers the 30-second recording period
3. **Button Processing**: 
   - Arduino sends button number (1-10)
   - Node looks up corresponding sound file
   - Sound waveform is added to the matrix at the current timestamp
4. **Recording Ends**: After 30 seconds, waveform is saved to CSV
5. **Ready for Next**: Node returns to idle, ready for next recording session

## Waveform Matrix Structure

The waveform is stored as a vector of (time, amplitude) points:
- **time**: Time in seconds from start of recording
- **amplitude**: Signal amplitude (-1.0 to 1.0)

## Output

After recording, the waveform is saved as a CSV file:
```csv
time,amplitude
0.000000,0.000000
0.000023,0.314159
...
```

## Arduino Expected Format

The Arduino should publish button numbers (1-10) as strings to the `arduino_data` topic:
```cpp
// Arduino example
Serial.println("1");  // Button 1 pressed
Serial.println("5");  // Button 5 pressed
```

## Placeholder Sound Generation

If a sound file is missing, the node generates a sine wave:
- Button 1 → 220 Hz (A3)
- Button 2 → 293 Hz
- ...
- Button 10 → 880 Hz (A5)
- Duration: 0.5 seconds with fade in/out envelope

## Future Enhancements

- [ ] Actual WAV file loading and decoding
- [ ] Sound mixing/overlapping support
- [ ] Real-time visualization
- [ ] Multiple simultaneous button press handling
- [ ] Adjustable sound duration and volume
- [ ] Export to audio file format (WAV, MP3)
