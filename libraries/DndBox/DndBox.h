// DndBox.h — shared hardware layer for the DND status cube (Seeed XIAO ESP32-C6).
//
// Header-only so both the `firmware` and `calibrate` sketches can share one copy
// of the pin map, ADXL345 driver, orientation matching, LED, and deep-sleep helpers.
// Compile with: arduino-cli compile --libraries ./libraries ...
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "esp_sleep.h"

// ───────────────────────── Pin map (Seeed XIAO ESP32-C6) ─────────────────────────
// LEDs: GPIO ─► resistor (~330Ω) ─► LED anode, cathode ─► GND  (active HIGH).
// Red and blue are wired to each other's pads (D7↔D10), so the macros are mapped to
// the pins the LEDs are physically on — swap the leads and swap these back if you rewire.
#define PIN_LED_RED     D10       // GPIO18  (red LED wired to the D10 pad)
#define PIN_LED_GREEN   D8        // GPIO19
#define PIN_LED_YELLOW  D9        // GPIO20
#define PIN_LED_BLUE    D7        // GPIO17  (blue LED wired to the D7 pad)
// ADXL345 over I²C (XIAO default SDA/SCL). INT1 must be an LP GPIO (0–7) so it
// can wake the C6 from deep sleep.
#define PIN_SDA         D4        // GPIO22
#define PIN_SCL         D5        // GPIO23
#define PIN_ACCEL_INT   D0        // GPIO0
#define ACCEL_INT_GPIO  0         // raw GPIO number for the deep-sleep wake mask

// ───────────────────────── ADXL345 registers ─────────────────────────
#define ADXL345_ADDR              0x53   // SDO tied to GND
#define ADXL345_REG_DEVID         0x00   // reads 0xE5
#define ADXL345_REG_THRESH_ACT    0x24   // 62.5 mg/LSB
#define ADXL345_REG_ACT_INACT_CTL 0x27
#define ADXL345_REG_BW_RATE       0x2C
#define ADXL345_REG_POWER_CTL     0x2D
#define ADXL345_REG_INT_ENABLE    0x2E
#define ADXL345_REG_INT_MAP       0x2F
#define ADXL345_REG_INT_SOURCE    0x30
#define ADXL345_REG_DATA_FORMAT   0x31
#define ADXL345_REG_DATAX0        0x32

// Activity-wake sensitivity: 62.5 mg per LSB. 4 ≈ 0.25 g of movement to wake.
#ifndef ACT_THRESH
#define ACT_THRESH 4
#endif

// A calibrated reference: the averaged accelerometer reading (in g) when `name`'s
// face points UP, plus the LED to light for that side (-1 = no LED / off).
struct SideRef {
  const char* name;
  float x, y, z;
  int   ledPin;
};

// ───────────────────────── ADXL345 driver ─────────────────────────
static inline void adxlWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static inline uint8_t adxlRead(uint8_t reg) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)ADXL345_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Start the ADXL345 in ±2g full-resolution measurement mode. Returns false if the
// device ID doesn't match (bad wiring / wrong address).
static inline bool adxlBegin() {
  Wire.begin(PIN_SDA, PIN_SCL);
  if (adxlRead(ADXL345_REG_DEVID) != 0xE5) return false;
  adxlWrite(ADXL345_REG_DATA_FORMAT, 0x08);  // FULL_RES, ±2g
  adxlWrite(ADXL345_REG_BW_RATE, 0x0A);      // 100 Hz, normal power
  adxlWrite(ADXL345_REG_POWER_CTL, 0x08);    // measure
  return true;
}

// Read one acceleration sample in g (full-res = 256 LSB/g).
static inline void adxlReadOnce(float& x, float& y, float& z) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(ADXL345_REG_DATAX0);
  Wire.endTransmission(false);
  Wire.requestFrom((int)ADXL345_ADDR, 6);
  int16_t xi = Wire.read() | (Wire.read() << 8);
  int16_t yi = Wire.read() | (Wire.read() << 8);
  int16_t zi = Wire.read() | (Wire.read() << 8);
  x = xi / 256.0f;
  y = yi / 256.0f;
  z = zi / 256.0f;
}

// Averaged read — smooths noise for stable orientation detection / calibration.
static inline void adxlReadAvg(float& x, float& y, float& z, int n = 32) {
  float sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < n; i++) {
    float ax, ay, az;
    adxlReadOnce(ax, ay, az);
    sx += ax; sy += ay; sz += az;
    delay(3);
  }
  x = sx / n; y = sy / n; z = sz / n;
}

// ───────────────────────── Orientation matching ─────────────────────────
// Return the index of the side whose reference vector best matches the live
// reading (smallest angle = largest normalised dot product). `conf` (optional)
// receives that dot product in [-1, 1]; ~1.0 means a confident match.
static inline int bestSide(const SideRef* sides, int n, float x, float y, float z,
                           float* conf = nullptr) {
  float m = sqrtf(x * x + y * y + z * z);
  if (m < 1e-3f) m = 1;
  x /= m; y /= m; z /= m;
  int best = -1;
  float bestDot = -2;
  for (int i = 0; i < n; i++) {
    float rm = sqrtf(sides[i].x * sides[i].x + sides[i].y * sides[i].y + sides[i].z * sides[i].z);
    if (rm < 1e-3f) rm = 1;
    float d = (x * sides[i].x + y * sides[i].y + z * sides[i].z) / rm;
    if (d > bestDot) { bestDot = d; best = i; }
  }
  if (conf) *conf = bestDot;
  return best;
}

// ───────────────────────── LEDs ─────────────────────────
static const int DND_LEDS[] = {PIN_LED_RED, PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_BLUE};

static inline void ledsInit() {
  for (int p : DND_LEDS) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
}
static inline void ledsAllOff() {
  for (int p : DND_LEDS) digitalWrite(p, LOW);
}
// Light exactly one LED (pin<0 → all off).
static inline void ledSet(int pin) {
  ledsAllOff();
  if (pin >= 0) digitalWrite(pin, HIGH);
}

// ───────────────────────── Deep sleep / wake ─────────────────────────
// Configure the ADXL345 to raise INT1 on movement, then deep-sleep the C6 until
// that pin goes HIGH. The MCU resets on wake, so setup() runs fresh afterwards.
static inline void deviceSleep() {
  adxlWrite(ADXL345_REG_POWER_CTL, 0x00);       // standby to reconfigure
  adxlWrite(ADXL345_REG_THRESH_ACT, ACT_THRESH);
  adxlWrite(ADXL345_REG_ACT_INACT_CTL, 0xF0);   // activity AC-coupled, X/Y/Z
  adxlWrite(ADXL345_REG_INT_MAP, 0x00);         // activity → INT1
  adxlWrite(ADXL345_REG_INT_ENABLE, 0x10);      // enable activity interrupt
  adxlWrite(ADXL345_REG_BW_RATE, 0x1A);         // low-power, 100 Hz
  adxlWrite(ADXL345_REG_POWER_CTL, 0x08);       // measure
  adxlRead(ADXL345_REG_INT_SOURCE);             // clear any pending interrupt

  pinMode(PIN_ACCEL_INT, INPUT);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << ACCEL_INT_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();  // does not return
}

static inline bool wokeFromMotion() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
}
