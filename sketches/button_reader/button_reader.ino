/*
 * AI DJ Button Reader - Arduino Uno
 *
 * Sound buttons (1-9, 0→10): send PRESS_n on press, RELEASE_n on release.
 *   Holding a button will report as active; releasing cuts the sound.
 * Nav buttons  (A=up, B=left, C=right, D=down): send NAV_X once on press.
 * Control keys (*=SELECT, #=BACK): send SELECT / BACK once on press.
 *
 * Hardware:
 *   Arduino Uno on /dev/ttyACM0 @ 9600 baud
 *   4x4 Matrix Keypad
 *   Row pins: 9, 8, 7, 6 — Column pins: 5, 4, 3, 2
 */

#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','a'},
  {'4','5','6','b'},
  {'7','8','9','c'},
  {'*','0','#','d'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Return the button number string for sound keys (1-9 → "1"-"9", 0 → "10")
const char* soundIndex(char key) {
  switch (key) {
    case '1': return "1";
    case '2': return "2";
    case '3': return "3";
    case '4': return "4";
    case '5': return "5";
    case '6': return "6";
    case '7': return "7";
    case '8': return "8";
    case '9': return "9";
    case '0': return "10";
    default:  return nullptr;
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  // Allow multi-key detection
  keypad.setDebounceTime(50);
  keypad.setHoldTime(200);
  Serial.println("Arduino Button Reader Ready");
}

void loop() {
  if (!keypad.getKeys()) return;

  for (int i = 0; i < LIST_MAX; i++) {
    Key& k = keypad.key[i];
    if (!k.stateChanged) continue;

    char key = k.kchar;

    // ── Sound buttons (hold-sensitive) ──────────────────────────────
    const char* idx = soundIndex(key);
    if (idx != nullptr) {
      if (k.kstate == PRESSED || k.kstate == HOLD) {
        Serial.print("PRESS_");
        Serial.println(idx);
      } else if (k.kstate == RELEASED) {
        Serial.print("RELEASE_");
        Serial.println(idx);
      }
      continue;
    }

    // ── Nav buttons (one-shot on PRESSED) ───────────────────────────
    if (k.kstate != PRESSED) continue;

    switch (key) {
      case 'a': Serial.println("NAV_A"); break;   // up
      case 'b': Serial.println("NAV_B"); break;   // left
      case 'c': Serial.println("NAV_C"); break;   // right
      case 'd': Serial.println("NAV_D"); break;   // down
      case '*': Serial.println("SELECT"); break;
      case '#': Serial.println("BACK");  break;
    }
  }
}
