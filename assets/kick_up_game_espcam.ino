/*
  KCIK UP GAME - ESPCAM Sketch
  ----------------------------------------------------------------------
  Locks onto the brightest position of the wall at startup and after 
  every hit. Once locked, it freezes the target zone until a HIT is detected.
*/

#include "esp_camera.h"

// ---------------------------------------------------------
// TARGET ZONE CONFIGURATION
// ---------------------------------------------------------
int ROI_CENTER_X = 80;  // Dynamically updated to the brightest spot
int ROI_CENTER_Y = 60;  // Dynamically updated to the brightest spot
int ROI_RADIUS = 35;    // Adjust this to match the visual size of your light circle

// ---------------------------------------------------------
// TUNABLE VARIABLES
// ---------------------------------------------------------
int PIXEL_CHANGE_THRESHOLD = 35; 
int MIN_CHANGED_PIXELS = 40;     
int RELEARN_DELAY_MS = 2500;     
int INITIAL_LEARN_DELAY_MS = 3000;
int BASELINE_FRAMES = 30;

// ---------------------------------------------------------
// INTERNAL STATE
// ---------------------------------------------------------
uint8_t *baseline = NULL;
bool baselineReady = false;
bool waitingToRelearn = false;
unsigned long relearnTime = 0;

#define FLASH_LED_PIN 4
const unsigned long TRIGGER_DURATION_MS = 250;
const unsigned long COOLDOWN_MS = 800;
unsigned long lastTriggerTime = 0;
bool flashActive = false;

const unsigned long PRINT_INTERVAL_MS = 60;
unsigned long lastPrintTime = 0;

// Camera pins for AI-Thinker module
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void learnBaseline() {
  Serial.println(">>> Finding the brightest spot on the wall...");

  int width = 160;
  int height = 120;
  int totalPixels = width * height;

  uint32_t *accumulator = (uint32_t *)calloc(totalPixels, sizeof(uint32_t));
  if (!accumulator) {
    Serial.println("ERROR: Out of memory!");
    return;
  }

  for (int f = 0; f < BASELINE_FRAMES; f++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { f--; continue; }

    for (int i = 0; i < totalPixels; i++) {
      accumulator[i] += fb->buf[i];
    }
    esp_camera_fb_return(fb);
    delay(33); 
  }

  if (baseline == NULL) {
    baseline = (uint8_t *)malloc(totalPixels);
  }

  long brightSumX = 0;
  long brightSumY = 0;
  int brightPixelCount = 0;
  
  // Average frames and actively track the brightest cluster of pixels
  for (int i = 0; i < totalPixels; i++) {
    baseline[i] = (uint8_t)(accumulator[i] / BASELINE_FRAMES);
    
    // Look for pixels brighter than 150 (out of 255)
    if (baseline[i] > 150) { 
      int x = i % width;
      int y = i / width;
      brightSumX += x;
      brightSumY += y;
      brightPixelCount++;
    }
  }

  free(accumulator);

  // If a bright spot is detected, shift the ROI center directly to it
  if (brightPixelCount > 10) {
    ROI_CENTER_X = brightSumX / brightPixelCount;
    ROI_CENTER_Y = brightSumY / brightPixelCount;
    Serial.printf(">>> LOCKED: ROI centered at brightest position: (%d, %d)\n", ROI_CENTER_X, ROI_CENTER_Y);
  } else {
    Serial.println(">>> WARNING: No bright spot detected. Defaulting to center (80,60)");
    ROI_CENTER_X = 80;
    ROI_CENTER_Y = 60;
  }

  baselineReady = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA; 
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { return; }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

  Serial.println("Warming up camera sensor...");
  delay(1500); 

  s->set_whitebal(s, 0);
  s->set_gain_ctrl(s, 0);
  s->set_exposure_ctrl(s, 0);
  s->set_agc_gain(s, 15);
  s->set_aec_value(s, 600);

  Serial.println("=========================================================");
  Serial.println("SOCCER DETECTOR — DYNAMIC TARGET BRIGHTNESS TRACKING");
  Serial.println("=========================================================");

  delay(INITIAL_LEARN_DELAY_MS);
  learnBaseline();
}

void loop() {
  unsigned long currentMillis = millis();

  if (waitingToRelearn && currentMillis >= relearnTime) {
    waitingToRelearn = false;
    learnBaseline();
  }

  // Serial controls
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    int val = Serial.parseInt();
    if (cmd == 'c' || cmd == 'C') PIXEL_CHANGE_THRESHOLD = val;
    if (cmd == 'n' || cmd == 'N') MIN_CHANGED_PIXELS = val;
    if (cmd == 'r' || cmd == 'R') RELEARN_DELAY_MS = val;
    if (cmd == 'b' || cmd == 'B') learnBaseline();
    while (Serial.available() > 0) { Serial.read(); }
  }

  if (waitingToRelearn || !baselineReady) {
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  int width = fb->width;
  int height = fb->height;
  uint8_t *pixels = fb->buf;

  int changedPixelCount = 0;

  // Scan the frame, limiting evaluation strictly to the current tracked ROI circle
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      
      int dx = x - ROI_CENTER_X;
      int dy = y - ROI_CENTER_Y;
      
      // Ignore pixels outside the dynamic light radius
      if ((dx * dx + dy * dy) > (ROI_RADIUS * ROI_RADIUS)) {
        continue; 
      }

      int idx = y * width + x;
      int diff = abs((int)pixels[idx] - (int)baseline[idx]);

      if (diff > PIXEL_CHANGE_THRESHOLD) {
        changedPixelCount++;
      }
    }
  }

  if (currentMillis - lastPrintTime >= PRINT_INTERVAL_MS) {
    Serial.printf("[WATCHING ZONE %d,%d] Changed px inside target: %d\n", 
                  ROI_CENTER_X, ROI_CENTER_Y, changedPixelCount);
    lastPrintTime = currentMillis;
  }

  if (changedPixelCount >= MIN_CHANGED_PIXELS && currentMillis - lastTriggerTime > COOLDOWN_MS) {
    Serial.println("HIT");
    lastTriggerTime = currentMillis;
    waitingToRelearn = true;
    relearnTime = currentMillis + RELEARN_DELAY_MS;
    Serial.printf(">>> HIT DETECTED! Re-centering on bright spot in %dms...\n", RELEARN_DELAY_MS);
  }

  esp_camera_fb_return(fb);
  vTaskDelay(pdMS_TO_TICKS(1));
}
