/*
 * AI DJ Button Reader - Arduino Uno
 * 
 * Reads a 4x4 matrix keypad and sends button presses over serial
 * - Buttons 0-9: Sound triggers (sent as 1-10)
 * - Button *: State control (sent as 11)
 * - Buttons a,b,c,d,#: Ignored
 * 
 * Hardware:
 * - Arduino Uno on /dev/ttyACM0
 * - 4x4 Matrix Keypad
 * - Row pins: 9, 8, 7, 6
 * - Column pins: 5, 4, 3, 2
 */

#include <Keypad.h>

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;

// Keypad layout
char keys[ROWS][COLS] = {
  {'1','2','3','a'},
  {'4','5','6','b'},
  {'7','8','9','c'},
  {'*','0','#','d'}
};

// Pin connections (adjust if needed)
byte rowPins[ROWS] = {9, 8, 7, 6};    // Connect to row pins of keypad
byte colPins[COLS] = {5, 4, 3, 2};    // Connect to column pins of keypad

// Create keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  // Initialize serial at 9600 baud (matches ROS2 reader node)
  Serial.begin(9600);
  
  // Wait for serial port to connect
  while (!Serial) {
    ; // Wait for serial port
  }
  
  Serial.println("Arduino Button Reader Ready");
  Serial.println("Layout: 1-9,0 = sounds, * = state control");
}

void loop() {
  // Read keypad
  char key = keypad.getKey();
  
  if (key) {
    // Map button to command
    if (key >= '1' && key <= '9') {
      // Buttons 1-9 (already numbered correctly)
      Serial.println(key);
    }
    else if (key == '0') {
      // Button 0 -> send as "10"
      Serial.println("10");
    }
    else if (key == '*') {
      // State control button -> send as "11"
      Serial.println("11");
    }
    // Ignore a, b, c, d, # buttons
    
    // Small delay to debounce
    delay(50);
  }
}
