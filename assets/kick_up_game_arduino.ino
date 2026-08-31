/*
  KICK UP GAME — Arduino Sketch
  ----------------------------------------------------------------------
  This code is meant to be uploaded ONCE and never touched again after
  mounting. All future tuning/changes happen on the ESP32-CAM side only.

  WHAT THIS DOES:
  - Listens for a single command over serial from the ESP32-CAM: "HIT"
  - When received, moves both servos to a new random position within
    your defined safe bounds

  WIRING:
  - Pan servo signal  -> Arduino Pin 9
  - Tilt servo signal -> Arduino Pin 10
  - Both servo red wires -> Arduino 5V
  - Both servo black/brown wires -> Arduino GND
  - ESP32-CAM TX -> Arduino Pin 0 (RX)
  - ESP32-CAM RX -> Arduino Pin 1 (TX)
  - Battery+ (through switch) -> Arduino VIN
  - Battery- -> Arduino GND

  IMPORTANT: Disconnect ESP32-CAM TX/RX wires from Arduino pins 0/1
  whenever you need to re-upload code to the Arduino itself (these pins
  are shared with USB serial and will interfere with uploads otherwise).
  Since this code is meant to be final/permanent, you should only need
  to do this once during initial setup.
*/

#include <Servo.h>

Servo panServo;
Servo tiltServo;

// ---------------------------------------------------------
// TUNABLE VARIABLES — set these once based on your gimbal's
// safe mechanical range, then this file should never need
// to change again
// ---------------------------------------------------------

const int PAN_PIN  = 9;
const int TILT_PIN = 10;

// Pan (left-right) angle bounds, in degrees (0-180 servo range)
int PAN_MIN = 10;     // leftmost safe angle
int PAN_MAX = 120;    // rightmost safe angle

// Tilt (up-down) angle bounds, in degrees (0-180 servo range)
int TILT_MIN = 65;    // topmost safe angle
int TILT_MAX = 115;   // bottommost safe angle

// Starting position (used briefly on power-up before first random move)
int PAN_START  = 65;
int TILT_START = 90;

int STARTUP_DELAY_MS = 1000;

// How long to wait after sending a "HIT" before accepting another one,
// to prevent the same physical hit from being processed multiple times
// if the ESP32-CAM happens to send the signal more than once in a row
int HIT_COOLDOWN_MS = 1000;
unsigned long lastHitTime = 0;

// ---------------------------------------------------------

void setup() {
  Serial.begin(115200);  // Must match ESP32-CAM's baud rate

  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);

  panServo.write(PAN_START);
  tiltServo.write(TILT_START);
  delay(STARTUP_DELAY_MS);

  randomSeed(analogRead(A0));

  moveToRandomPosition();
}

void loop() {
  if (Serial.available() > 0) {
    String message = Serial.readStringUntil('\n');
    message.trim();

    if (message == "HIT") {
      if (millis() - lastHitTime > HIT_COOLDOWN_MS) {
        moveToRandomPosition();
        lastHitTime = millis();
      }
    }
  }
}

void moveToRandomPosition() {
  int randomPan  = random(PAN_MIN, PAN_MAX + 1);
  int randomTilt = random(TILT_MIN, TILT_MAX + 1);

  panServo.write(randomPan);
  tiltServo.write(randomTilt);
}
