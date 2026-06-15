// pico1_matrix.ino — Raspberry Pi Pico, clusters 1-3 (columns 0-44)
// Receives binary serial commands from the ROS writer node and renders
// a waveform on a 74×45 section of the WS2815 LED matrix.
//
// Upload via Arduino IDE with the Raspberry Pi Pico board package installed.
// Select: Tools → Board → Raspberry Pi Pico
//
// Serial protocol (see Part 9 of the implementation plan):
//   All packets: SYNC(0xAA 0x55) CMD(1) LEN_H(1) LEN_L(1) PAYLOAD(LEN)
//   CMD 0x01 WAVEFORM  — interval_ms(2) start_delay_ms(2) n_cols(1) amplitudes(n_cols)
//   CMD 0x02 COLORIZE  — R G B n_cols confidence(n_cols)
//   CMD 0x03 CLEAR     — no payload, reverts to white waveform
//   CMD 0x04 RESET     — clears all LEDs

#include <FastLED.h>

// ─── Board-specific configuration ────────────────────────────────────────────
#define DATA_PIN_1    9   // GP9  — cluster 1
#define DATA_PIN_2   11   // GP11 — cluster 2
#define DATA_PIN_3   12   // GP12 — cluster 3
#define N_CLUSTERS    3
#define N_COLS       45   // 3 clusters × 15 strips/cluster

// ─── Shared hardware constants ────────────────────────────────────────────────
#define COLOR_ORDER         GRB
#define LED_TYPE            WS2815
#define BRIGHTNESS            3   // Hard power ceiling — do not raise without load testing
#define PIXELS_PER_STRIP     74
#define STRIPS_PER_CLUSTER   15
#define LEDS_PER_CLUSTER    (PIXELS_PER_STRIP * STRIPS_PER_CLUSTER)  // 1110

// ─── Serial protocol constants ────────────────────────────────────────────────
#define SYNC_0        0xAA
#define SYNC_1        0x55
#define CMD_WAVEFORM  0x01
#define CMD_COLORIZE  0x02
#define CMD_CLEAR     0x03
#define CMD_RESET     0x04
#define RX_BUF_SIZE   256

// ─── LED buffers (one per cluster) ───────────────────────────────────────────
CRGB leds[N_CLUSTERS][LEDS_PER_CLUSTER];

// ─── Waveform & color state ───────────────────────────────────────────────────
uint8_t  amplitudes[N_COLS];   // bar height per column (0–74)
uint8_t  conf[N_COLS];         // confidence per column (0–255)
CRGB     model_color = CRGB::White;
bool     waveform_ready = false;
bool     colorized      = false;

// ─── Reveal state machine ─────────────────────────────────────────────────────
enum State { IDLE, WAITING, REVEALING, WAVEFORM_VISIBLE, COLORIZED_STATE };
State    state           = IDLE;
uint32_t reveal_start_ms = 0;
uint16_t reveal_interval = 400;  // ms per column (overridden by packet)
int      revealed_col    = 0;
uint32_t last_col_time   = 0;

// ─── Serial receive state machine ────────────────────────────────────────────
enum RxState { RX_SYNC0, RX_SYNC1, RX_CMD, RX_LEN_H, RX_LEN_L, RX_DATA };
RxState  rx_state = RX_SYNC0;
uint8_t  rx_cmd;
uint16_t rx_len;
uint16_t rx_pos;
uint8_t  rx_buf[RX_BUF_SIZE];

// ─── Pixel helpers ────────────────────────────────────────────────────────────

// Map (global_col, pixel_in_strip) to the CRGB in the right cluster array.
// Handles serpentine wiring: odd strips run in the opposite direction.
static inline CRGB& led_at(int global_col, int pixel) {
    int cluster = global_col / STRIPS_PER_CLUSTER;
    int strip   = global_col % STRIPS_PER_CLUSTER;
    int local   = (strip % 2 == 1) ? (PIXELS_PER_STRIP - 1 - pixel) : pixel;
    return leds[cluster][strip * PIXELS_PER_STRIP + local];
}

// Return the color to use for a column (white or confidence-blended agent color).
static inline CRGB col_color(int col) {
    if (!colorized) return CRGB::White;
    uint8_t c = conf[col];
    // lerp(WHITE → model_color, c/255)
    return CRGB(
        (uint8_t)(255 + (int)(model_color.r - 255) * c / 255),
        (uint8_t)(255 + (int)(model_color.g - 255) * c / 255),
        (uint8_t)(255 + (int)(model_color.b - 255) * c / 255)
    );
}

// Render one column of the waveform (centered bar with the given amplitude).
void render_column(int col, CRGB color) {
    uint8_t amp     = amplitudes[col];
    int     padding = (PIXELS_PER_STRIP - (int)amp) / 2;
    for (int p = 0; p < PIXELS_PER_STRIP; p++) {
        bool lit = (p >= padding && p < padding + amp);
        led_at(col, p) = lit ? color : CRGB::Black;
    }
}

// Re-render all already-revealed columns and push to hardware.
void render_all_revealed() {
    for (int c = 0; c < revealed_col; c++)
        render_column(c, col_color(c));
    FastLED.show();
}

// ─── Packet dispatch ──────────────────────────────────────────────────────────
void dispatch(uint8_t cmd, const uint8_t* data, uint16_t len) {
    switch (cmd) {

        case CMD_WAVEFORM: {
            if (len < 5) return;
            reveal_interval = ((uint16_t)data[0] << 8) | data[1];
            uint16_t delay  = ((uint16_t)data[2] << 8) | data[3];
            uint8_t  n      = data[4];
            if (n > N_COLS) n = N_COLS;
            for (uint8_t i = 0; i < n; i++)
                amplitudes[i] = (5 + i < len) ? data[5 + i] : 0;
            waveform_ready = true;
            colorized      = false;
            revealed_col   = 0;
            reveal_start_ms = millis() + (uint32_t)delay;
            state = (delay > 0) ? WAITING : REVEALING;
            last_col_time = millis();
            FastLED.clear();
            FastLED.show();
            Serial.print("WAVEFORM received, cols="); Serial.print(n);
            Serial.print(", delay="); Serial.println(delay);
            break;
        }

        case CMD_COLORIZE: {
            if (len < 4 || !waveform_ready) return;
            model_color = CRGB(data[0], data[1], data[2]);
            uint8_t n   = data[3];
            if (n > N_COLS) n = N_COLS;
            for (uint8_t i = 0; i < n; i++)
                conf[i] = (4 + i < len) ? data[4 + i] : 0;
            colorized = true;
            render_all_revealed();
            if (state == WAVEFORM_VISIBLE) state = COLORIZED_STATE;
            Serial.println("COLORIZE applied");
            break;
        }

        case CMD_CLEAR: {
            if (!waveform_ready) return;
            colorized = false;
            render_all_revealed();
            if (state == COLORIZED_STATE) state = WAVEFORM_VISIBLE;
            Serial.println("CLEAR — back to white");
            break;
        }

        case CMD_RESET: {
            waveform_ready = false;
            colorized      = false;
            revealed_col   = 0;
            state = IDLE;
            FastLED.clear();
            FastLED.show();
            Serial.println("RESET");
            break;
        }
    }
}

// ─── Serial receive ───────────────────────────────────────────────────────────
void receive_serial() {
    while (Serial.available()) {
        uint8_t b = (uint8_t)Serial.read();
        switch (rx_state) {
            case RX_SYNC0: rx_state = (b == SYNC_0) ? RX_SYNC1 : RX_SYNC0; break;
            case RX_SYNC1: rx_state = (b == SYNC_1) ? RX_CMD   : RX_SYNC0; break;
            case RX_CMD:   rx_cmd = b; rx_state = RX_LEN_H; break;
            case RX_LEN_H: rx_len = (uint16_t)b << 8; rx_state = RX_LEN_L; break;
            case RX_LEN_L:
                rx_len |= b;
                if (rx_len == 0) {
                    dispatch(rx_cmd, rx_buf, 0);
                    rx_state = RX_SYNC0;
                } else {
                    rx_pos = 0;
                    rx_state = RX_DATA;
                }
                break;
            case RX_DATA:
                if (rx_pos < RX_BUF_SIZE) rx_buf[rx_pos] = b;
                rx_pos++;
                if (rx_pos >= rx_len) {
                    dispatch(rx_cmd, rx_buf, rx_len);
                    rx_state = RX_SYNC0;
                }
                break;
        }
    }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    FastLED.addLeds<LED_TYPE, DATA_PIN_1, COLOR_ORDER>(leds[0], LEDS_PER_CLUSTER);
    FastLED.addLeds<LED_TYPE, DATA_PIN_2, COLOR_ORDER>(leds[1], LEDS_PER_CLUSTER);
    FastLED.addLeds<LED_TYPE, DATA_PIN_3, COLOR_ORDER>(leds[2], LEDS_PER_CLUSTER);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    Serial.begin(115200);
    Serial.println("Pico 1 matrix online — clusters 1-3, 45 cols");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    receive_serial();

    uint32_t now = millis();

    switch (state) {
        case WAITING:
            if (now >= reveal_start_ms) {
                state         = REVEALING;
                last_col_time = now;
            }
            break;

        case REVEALING:
            if (now - last_col_time >= (uint32_t)reveal_interval) {
                if (revealed_col < N_COLS) {
                    render_column(revealed_col, col_color(revealed_col));
                    FastLED.show();
                    last_col_time = now;
                    revealed_col++;
                }
                if (revealed_col >= N_COLS) {
                    state = colorized ? COLORIZED_STATE : WAVEFORM_VISIBLE;
                    Serial.println("Reveal complete");
                }
            }
            break;

        default:
            break;
    }
}
