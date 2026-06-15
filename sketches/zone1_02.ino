#include <FastLED.h>

// =========================================================================
// HARDWARE & TIMING CONFIGURATION
// =========================================================================
#define DATA_PIN_1 9       // Controls Cluster 1 (GP9 - Physical Pin 12)
#define DATA_PIN_2 11      // Controls Cluster 2 (GP11 - Physical Pin 15)
#define DATA_PIN_3 12      // Controls Cluster 3 (GP12 - Physical Pin 16)
#define COLOR_ORDER GRB    // Native byte order layout for WS2815 strips

// ADJUSTABLE SEQUENTIAL TIMING (In Milliseconds)
#define CLUSTER_DELAY_MS 300 // <-- Change this to make the transition faster or slower

// Matrix Dimensions (74 Pixels * 15 Strips per Cluster)
#define PIXELS_PER_STRIP 74
#define STRIPS_PER_CLUSTER 15
#define NUM_CLUSTERS 3     

#define NUM_PIXELS_PER_PIN (PIXELS_PER_STRIP * STRIPS_PER_CLUSTER) // 1110 pixels per line

// Allocate distinct hardware memory blocks for all three data lines
CRGB leds_cluster1[NUM_PIXELS_PER_PIN];
CRGB leds_cluster2[NUM_PIXELS_PER_PIN];
CRGB leds_cluster3[NUM_PIXELS_PER_PIN]; 

// ==========================================
// EXPLICIT SPECTROGRAM MATRIX
// ==========================================
const uint8_t test_waveform[NUM_CLUSTERS][STRIPS_PER_CLUSTER] = {
  // Cluster 1 (GP9): EQ curve climbing then falling
  {10, 15, 25, 35, 45, 55, 65, 74, 65, 55, 45, 35, 25, 15, 10}, 
  
  // Cluster 2 (GP11): EQ curve falling then climbing
  {74, 65, 55, 45, 35, 25, 15, 10, 15, 25, 35, 45, 55, 65, 74},

  // Cluster 3 (GP12): EQ curve dipping in the middle 
  {70, 50, 35, 20, 10, 5, 2, 0, 2, 5, 10, 20, 35, 50, 70}
};

void renderSpectrogramWithColor(CRGB current_color);

void setup() {
  FastLED.addLeds<WS2815, DATA_PIN_1, COLOR_ORDER>(leds_cluster1, NUM_PIXELS_PER_PIN);
  FastLED.addLeds<WS2815, DATA_PIN_2, COLOR_ORDER>(leds_cluster2, NUM_PIXELS_PER_PIN);
  FastLED.addLeds<WS2815, DATA_PIN_3, COLOR_ORDER>(leds_cluster3, NUM_PIXELS_PER_PIN); 

  FastLED.setBrightness(3); // Low power testing ceiling
  FastLED.clear();
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Pico Tri-Pin Sequential Cascade Bench Online.");
}

void loop() {
  Serial.println("Waveform Sequence: Red");
  renderSpectrogramWithColor(CRGB(255, 0, 0));
  delay(1000); // Pause on final full look before switching colors

  Serial.println("Waveform Sequence: Green");
  renderSpectrogramWithColor(CRGB(0, 255, 0));
  delay(1000);

  Serial.println("Waveform Sequence: Blue");
  renderSpectrogramWithColor(CRGB(0, 0, 255));
  delay(1000);

  Serial.println("Waveform Sequence: Magenta");
  renderSpectrogramWithColor(CRGB(255, 0, 100));
  delay(1000);

  Serial.println("Waveform Sequence: White");
  renderSpectrogramWithColor(CRGB(255, 255, 255));
  delay(1000);

  // SAFE SHUTDOWN INTERVAL
  FastLED.clear();
  FastLED.show();
  Serial.println("Dark Latch");
  delay(1000);
}

// ==========================================
// CORE MATRIX RENDERING FUNCTION
// ==========================================
void renderSpectrogramWithColor(CRGB current_color) {
  // 1. Wipe the frame buffers completely clean at the start of a color change.
  FastLED.clear(); 

  // Step through our 3 distinct hardware clusters
  for (int c = 0; c < NUM_CLUSTERS; c++) {
    
    // Select the pointer to the target cluster memory space
    CRGB* target_led_array = nullptr;
    if (c == 0) {
      target_led_array = leds_cluster1;
    } else if (c == 1) {
      target_led_array = leds_cluster2;
    } else {
      target_led_array = leds_cluster3;
    }

    // Step through the 15 physical strips inside the active cluster
    for (int s = 0; s < STRIPS_PER_CLUSTER; s++) {
      int target_height = test_waveform[c][s];
      
      if (target_height > PIXELS_PER_STRIP) target_height = PIXELS_PER_STRIP;
      if (target_height < 0) target_height = 0;
      
      int padding = (PIXELS_PER_STRIP - target_height) / 2;
      int start_pixel = padding;
      int end_pixel = padding + target_height;
      int strip_data_offset = s * PIXELS_PER_STRIP;

      for (int p = start_pixel; p < end_pixel; p++) {
        int local_pixel = p;
        if (s % 2 == 1) {
          local_pixel = (PIXELS_PER_STRIP - 1) - p;
        }
        int global_pixel_index = strip_data_offset + local_pixel;
        target_led_array[global_pixel_index] = current_color;
      }
    }

    // 2. NEW MECHANISM: Push data to hardware IMMEDIATELY after calculating this specific cluster
    FastLED.show();

    // 3. NEW MECHANISM: Hold the look on the strip before processing the next cluster channel
    // We omit the delay on the final cluster so the loop returns cleanly to the main timing blocks
    if (c < NUM_CLUSTERS - 1) {
      delay(CLUSTER_DELAY_MS);
    }
  }
}