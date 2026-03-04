# Arduino Sketches

## Button Reader (Arduino Uno)

### Hardware Setup

**Components:**
- Arduino Uno
- 4x4 Matrix Keypad

**Wiring:**
```
Keypad → Arduino Uno
Row 1  → Pin 9
Row 2  → Pin 8
Row 3  → Pin 7
Row 4  → Pin 6
Col 1  → Pin 5
Col 2  → Pin 4
Col 3  → Pin 3
Col 4  → Pin 2
```

**Keypad Layout:**
```
1  2  3  a
4  5  6  b
7  8  9  c
*  0  #  d
```

**Button Mapping:**
- `1-9` → Sends "1" to "9" (sound buttons)
- `0` → Sends "10" (sound button)
- `*` → Sends "11" (state control)
- `a,b,c,d,#` → Ignored

### Software Setup

1. **Install Arduino IDE:**
   ```bash
   # Ubuntu/Debian
   sudo apt install arduino
   
   # Or download from: https://www.arduino.cc/en/software
   ```

2. **Install Keypad Library:**
   - Open Arduino IDE
   - Go to: Sketch → Include Library → Manage Libraries
   - Search for "Keypad" by Mark Stanley and Alexander Brevig
   - Click Install

3. **Upload Sketch:**
   ```bash
   # Open the sketch
   arduino sketches/button_reader/button_reader.ino
   
   # In Arduino IDE:
   # 1. Tools → Board → Arduino Uno
   # 2. Tools → Port → /dev/ttyACM0
   # 3. Click Upload button (→)
   ```

4. **Test Serial Output:**
   ```bash
   # With Arduino IDE Serial Monitor (Ctrl+Shift+M)
   # Set baud rate to 9600
   # Press buttons and see numbers appear
   
   # Or use command line:
   sudo apt install screen
   screen /dev/ttyACM0 9600
   # Press Ctrl+A then K to exit
   ```

### Troubleshooting

**Permission Denied:**
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

**Port Not Found:**
```bash
# Check which ports are connected
ls -l /dev/ttyACM*
lsusb

# Expected:
# Arduino Uno R3 (CDC ACM) → /dev/ttyACM0
# ESP32 (USB JTAG/serial) → /dev/ttyACM1
```

**Upload Fails:**
```bash
# Make sure no other program is using the port
# Close Serial Monitor or any ROS nodes first
pkill -f "ros2 run cpp_pkg reader"
```
