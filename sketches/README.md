# Sketches

Firmware for the two microcontrollers in the system. Both use the Arduino IDE.

---

## button_reader — Arduino Uno R3

Reads a 4×4 matrix keypad and emits serial tokens to the ROS `reader` node.

**Port:** `/dev/ttyACM0` @ **9600 baud**

### Wiring

```
Keypad pin → Arduino pin
Row 1      → D9
Row 2      → D8
Row 3      → D7
Row 4      → D6
Col 1      → D5
Col 2      → D4
Col 3      → D3
Col 4      → D2
```

### Serial protocol (output only)

| Key pressed | Token sent | Token on release |
|-------------|-----------|-----------------|
| 1 – 9 | `PRESS_1` … `PRESS_9` | `RELEASE_1` … `RELEASE_9` |
| 0 | `PRESS_10` | `RELEASE_10` |
| A | `NAV_A` | _(none)_ |
| B | `NAV_B` | _(none)_ |
| C | `NAV_C` | _(none)_ |
| D | `NAV_D` | _(none)_ |
| `*` | `SELECT` | _(none)_ |
| `#` | `BACK` | _(none)_ |

Sound buttons (1–10) emit both PRESS and RELEASE. Navigation keys emit only on press.

### Upload

```bash
# Board: Arduino Uno  Port: /dev/ttyACM0
arduino-cli lib install Keypad
arduino-cli compile --fqbn arduino:avr:uno sketches/button_reader
arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 sketches/button_reader
```

Or open `button_reader.ino` in Arduino IDE → select **Arduino Uno** → Upload.

---

## matrix_display — Raspberry Pi Pico (×2)

Two Pico boards drive the 74×75 WS2815 LED matrix. Each runs the same
logic; they differ only in pin assignments and which section of the matrix
they own.

**Port:** `/dev/ttyACM1` (Pico 1), `/dev/ttyACM2` (Pico 2) @ **115200 baud**

### LED matrix geometry

```
← time (75 columns = 30 s) →
┌──────────────────────────────────────────────────────────────────────────┐
│  Cluster 1   │  Cluster 2   │  Cluster 3   │  Cluster 4   │  Cluster 5   │  74 px tall
│  cols 0-14   │  cols 15-29  │  cols 30-44  │  cols 45-59  │  cols 60-74  │
│◄────────── Pico 1 (GP9, GP11, GP12) ──────►│◄── Pico 2 (GP11, GP12) ───►│
└──────────────────────────────────────────────────────────────────────────┘
```

Each cluster = 15 strips × 74 pixels = 1 110 LEDs. Strips use **serpentine wiring**
(odd-numbered strips run in reverse).

### Pin assignments

| Board | Pin | Cluster |
|-------|-----|---------|
| Pico 1 | GP9 | 1 |
| Pico 1 | GP11 | 2 |
| Pico 1 | GP12 | 3 |
| Pico 2 | GP11 | 4 |
| Pico 2 | GP12 | 5 |

> **Power note:** `FastLED.setBrightness(3)` is the hard ceiling in both sketches.
> Do not raise this value without first load-testing the power supply.

### Sketches

| File | Board | Clusters | Columns | Start delay |
|------|-------|----------|---------|-------------|
| `pico1_matrix.ino` | Pico 1 | 1-3 | 0-44 | 0 ms |
| `pico2_matrix.ino` | Pico 2 | 4-5 | 45-74 | 18 000 ms |
| `zone1_02.ino` | Pico 1 | 1-3 | — | standalone test |

### Serial protocol (input only)

All packets from the ROS writer node use the format:

```
SYNC[2]   0xAA 0x55
CMD[1]    command byte
LEN[2]    payload length (big-endian uint16)
PAYLOAD   LEN bytes
```

| CMD | Name | Payload | Action |
|-----|------|---------|--------|
| `0x01` | WAVEFORM | `interval_ms(2) start_delay_ms(2) n_cols(1) amplitudes[n_cols]` | Store waveform, begin timed reveal |
| `0x02` | COLORIZE | `R G B n_cols confidence[n_cols]` | Repaint with agent color blended by confidence |
| `0x03` | CLEAR | _(none)_ | Revert to white waveform |
| `0x04` | RESET | _(none)_ | Clear all LEDs |

**WAVEFORM reveal timing:** Both Picos receive the packet simultaneously.
Pico 1's `start_delay_ms = 0`; Pico 2's `start_delay_ms = 18 000`. Both
reveal at `interval_ms = 400 ms/column`, so the sweep fills the full 75-column
matrix left-to-right in exactly 30 seconds.

**COLORIZE color blend:** Each lit pixel uses
`color = lerp(WHITE → model_color, confidence / 255)`.
High confidence → agent color (red/green/blue). Low confidence → white.

### Upload

Install the **Raspberry Pi Pico** board package in Arduino IDE:

1. File → Preferences → Additional boards manager URLs:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Tools → Board → Boards Manager → search **RP2040** → install **Raspberry Pi Pico/RP2040**
3. Install the **FastLED** library: Sketch → Include Library → Manage Libraries → search **FastLED**

Upload:
```bash
# In Arduino IDE:
# Tools → Board → Raspberry Pi Pico
# Tools → Port → /dev/ttyACM1 (Pico 1) or /dev/ttyACM2 (Pico 2)
# Sketch → Upload
```

### Standalone test

`zone1_02.ino` cycles through red → green → blue → magenta → white using
hardcoded test amplitudes — no serial input needed. Upload it to Pico 1 to
verify hardware wiring before running the full system.

---

## Troubleshooting

**Permission denied on serial port**
```bash
sudo usermod -aG dialout $USER   # log out and back in
```

**Port not found**
```bash
ls /dev/ttyACM*   # Arduino on ACM0, Pico 1 on ACM1, Pico 2 on ACM2
lsusb             # confirm all three devices enumerated
```

**Upload fails — port busy**
```bash
# Stop any ROS node using the port first
pkill -f "ros2 launch"
```

**Pico not responding to serial commands**
```bash
# Open a serial monitor at 115200 and confirm the startup message:
# "Pico 1 matrix online — clusters 1-3, 45 cols"
# then send a minimal WAVEFORM packet to test:
python3 - <<'EOF'
import serial, struct
s = serial.Serial('/dev/ttyACM1', 115200, timeout=2)
amps = [37] * 45
body = struct.pack('>HHB', 400, 0, 45) + bytes(amps)
hdr  = bytes([0xAA, 0x55, 0x01]) + struct.pack('>H', len(body))
s.write(hdr + body)
print(s.readline())   # should print nothing; columns start lighting up
EOF
```
