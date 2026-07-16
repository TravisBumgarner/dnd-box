// calibrate.ino — orientation calibration for the DND cube.
//
// Walks you through each side: rotate so that face points UP, press Enter, and it
// records the averaged accelerometer vector. At the end it prints a config.h block.
// Normally run via `./upload.sh calibrate`, which drives this over serial and writes
// firmware/config.h for you. For raw debugging use `./upload.sh calibrate -m`, then
// press Enter once to start (the sketch waits for a host byte before printing).
#include <DndBox.h>

struct CalStep {
  const char* name;
  const char* ledMacro;  // text emitted into config.h for this side's LED
};

// Order defines the rows in config.h. TOP/BOTTOM have no LED.
static CalStep STEPS[] = {
  {"RED",    "PIN_LED_RED"},
  {"GREEN",  "PIN_LED_GREEN"},
  {"YELLOW", "PIN_LED_YELLOW"},
  {"BLUE",   "PIN_LED_BLUE"},
  {"TOP",    "-1"},
  {"BOTTOM", "-1"},
};
static const int N = sizeof(STEPS) / sizeof(STEPS[0]);
float vx[N], vy[N], vz[N];

void waitForEnter() {
  while (Serial.available()) Serial.read();          // drain
  for (;;) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') return;
    }
    delay(5);
  }
}

void setup() {
  Serial.begin(115200);
  // The XIAO C6's USB-Serial/JTAG reports "connected" the moment it boots, so
  // `while (!Serial)` returns instantly and the banner below prints before any host
  // is reading — it gets lost. Instead, wait for the host to send a byte first: the
  // calibrate driver sends one on connect; in `-m` mode, just press Enter to start.
  while (!Serial.available()) delay(10);
  while (Serial.available()) Serial.read();   // drain the hello
  delay(200);
  ledsInit();

  if (!adxlBegin()) {
    Serial.println("ADXL345 not found — check SDA/SCL/power. Halting.");
    for (;;) delay(1000);
  }

  Serial.println("\n=== DND cube calibration ===");
  Serial.println("For each side: rotate so that face points UP, hold still, press Enter.\n");

  for (int i = 0; i < N; i++) {
    Serial.printf("[%d/%d] %s side UP, then press Enter...\n", i + 1, N, STEPS[i].name);
    waitForEnter();
    adxlReadAvg(vx[i], vy[i], vz[i], 128);
    Serial.printf("      captured  x=%.4f  y=%.4f  z=%.4f\n\n", vx[i], vy[i], vz[i]);
  }

  Serial.println("=========================================================");
  Serial.println("Copy everything between the markers into firmware/config.h:");
  Serial.println("----- BEGIN config.h -----");
  Serial.println("#pragma once");
  Serial.println("#include <DndBox.h>");
  Serial.println();
  Serial.println("static const SideRef SIDES[] = {");
  for (int i = 0; i < N; i++) {
    Serial.printf("  { \"%s\", %.4ff, %.4ff, %.4ff, %s },\n",
                  STEPS[i].name, vx[i], vy[i], vz[i], STEPS[i].ledMacro);
  }
  Serial.println("};");
  Serial.println("static const int NUM_SIDES = sizeof(SIDES) / sizeof(SIDES[0]);");
  Serial.println("----- END config.h -----");
  Serial.println("\nThen run:  ./upload.sh");
}

void loop() {}
