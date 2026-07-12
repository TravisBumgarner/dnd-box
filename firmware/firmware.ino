// firmware.ino — DND status cube (Seeed XIAO ESP32-C6).
//
// Reads the ADXL345 to decide which face points UP, then lights that side's LED.
// TOP / BOTTOM sides have no LED (off). After sitting on an "off" side for
// SLEEP_TIMEOUT_MS, the cube deep-sleeps and wakes on movement (ADXL345 INT → D0).
#include <DndBox.h>
#include "config.h"

// ───────── Tuning ─────────
static const uint32_t SLEEP_TIMEOUT_MS = 30000;  // idle-on-off-side before sleep
static const float    MATCH_MIN_CONF   = 0.90f;  // dot-product needed to accept a side
static const uint8_t  DEBOUNCE         = 4;       // stable reads before switching side
static const uint16_t LOOP_DELAY_MS    = 80;

int      currentSide = -1;   // side currently displayed
int      candidate   = -1;   // side we're debouncing toward
uint8_t  stable      = 0;
uint32_t offSince    = 0;    // millis() when we landed on an off side (0 = not off)

static bool isOffSide(int i) { return i >= 0 && SIDES[i].ledPin < 0; }

void showSide(int i) {
  ledSet(i >= 0 ? SIDES[i].ledPin : -1);
  Serial.printf("side = %s\n", i >= 0 ? SIDES[i].name : "?");
}

// Flash all LEDs briefly so a hardware/wiring fault is obvious.
void errorBlink() {
  for (;;) {
    for (int p : DND_LEDS) digitalWrite(p, HIGH);
    delay(150);
    ledsAllOff();
    delay(150);
    Serial.println("ADXL345 not found — check SDA/SCL/power.");
  }
}

void setup() {
  Serial.begin(115200);
  ledsInit();
  if (!adxlBegin()) errorBlink();
  Serial.println(wokeFromMotion() ? "woke from motion" : "power-on / reset");
}

void loop() {
  float x, y, z, conf;
  adxlReadAvg(x, y, z, 16);
  int b = bestSide(SIDES, NUM_SIDES, x, y, z, &conf);
  if (conf < MATCH_MIN_CONF) b = currentSide;   // ambiguous tilt → hold current

  if (b == candidate) { if (stable < 255) stable++; }
  else { candidate = b; stable = 0; }

  if (stable >= DEBOUNCE && b != currentSide) {
    currentSide = b;
    showSide(currentSide);
  }

  // Sleep after being idle on an off side (charging keeps working — motion wakes it).
  if (isOffSide(currentSide)) {
    if (offSince == 0) offSince = millis();
    else if (millis() - offSince > SLEEP_TIMEOUT_MS) {
      Serial.println("idle on off side → deep sleep");
      Serial.flush();
      deviceSleep();  // returns only after a reset on the next wake
    }
  } else {
    offSince = 0;
  }

  delay(LOOP_DELAY_MS);
}
