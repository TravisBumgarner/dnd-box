// adxl345_test.ino — isolated ADXL345 read test (Seeed XIAO ESP32-C6).
//
// Self-contained: no DndBox.h / config.h dependency. Talks to the ADXL345 directly
// over I2C so you can confirm the sensor and wiring work on their own, apart from
// the rest of the cube firmware.
//
// Expected: on boot, "ADXL345 found (DEVID 0xE5)" then a stream of x/y/z readings
// in g. At rest, one axis should read ~±1.00 g (gravity) and the others ~0.00 g.
// Rotate the board and watch which axis swaps.
//
// Upload with arduino-cli, e.g.:
//   arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 component_tests/adxl345_test
//   arduino-cli upload  --fqbn esp32:esp32:XIAO_ESP32C6 -p /dev/tty.usbmodem* component_tests/adxl345_test
//   arduino-cli monitor -p /dev/tty.usbmodem* -c baudrate=115200

#include <Wire.h>

// ── Pin map (matches DndBox.h / XIAO ESP32-C6) ──
#define PIN_SDA  D4   // GPIO22
#define PIN_SCL  D5   // GPIO23

// ── ADXL345 registers ──
#define ADXL345_ADDR            0x53   // SDO tied to GND
#define ADXL345_REG_DEVID       0x00   // reads 0xE5
#define ADXL345_REG_BW_RATE     0x2C
#define ADXL345_REG_POWER_CTL   0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0      0x32

static void adxlWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t adxlRead(uint8_t reg) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)ADXL345_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Read one sample in g (full-res = 256 LSB/g).
static void adxlReadOnce(float& x, float& y, float& z) {
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

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* wait for USB serial */ }

  Wire.begin(PIN_SDA, PIN_SCL);

  uint8_t id = adxlRead(ADXL345_REG_DEVID);
  if (id != 0xE5) {
    Serial.printf("ADXL345 NOT found — DEVID read 0x%02X (expected 0xE5).\n", id);
    Serial.println("Check SDA/SCL wiring, 3V3 power, and that SDO is tied to GND (addr 0x53).");
    return;  // fall through to loop, which will just report the failure
  }
  Serial.println("ADXL345 found (DEVID 0xE5).");

  adxlWrite(ADXL345_REG_DATA_FORMAT, 0x08);  // FULL_RES, ±2g
  adxlWrite(ADXL345_REG_BW_RATE, 0x0A);      // 100 Hz, normal power
  adxlWrite(ADXL345_REG_POWER_CTL, 0x08);    // measure
}

void loop() {
  if (adxlRead(ADXL345_REG_DEVID) != 0xE5) {
    Serial.println("ADXL345 unreachable — check wiring/power.");
    delay(1000);
    return;
  }
  float x, y, z;
  adxlReadOnce(x, y, z);
  float mag = sqrtf(x * x + y * y + z * z);
  Serial.printf("x=%+6.3f  y=%+6.3f  z=%+6.3f  |g|=%5.3f\n", x, y, z, mag);
  delay(200);
}
