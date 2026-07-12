// config.h — orientation calibration for the DND cube.
//
// PLACEHOLDER values (ideal axis-aligned directions). Replace this whole block with
// the output of the calibration tool:  ./upload.sh calibrate -m  (then follow the
// prompts and paste the emitted block here, then ./upload.sh).
//
// Each row is the accelerometer reading (in g) when that side faces UP.
#pragma once
#include <DndBox.h>

static const SideRef SIDES[] = {
  { "RED", -0.0283f, 1.0579f, -0.1523f, PIN_LED_RED },
  { "GREEN", -0.9839f, -0.1177f, -0.0120f, PIN_LED_GREEN },
  { "YELLOW", 1.0538f, 0.0962f, -0.2155f, PIN_LED_YELLOW },
  { "BLUE", 0.1940f, -1.0020f, 0.0107f, PIN_LED_BLUE },
  { "TOP", 0.1970f, 0.1151f, 0.9261f, -1 },
  { "BOTTOM", -0.0616f, -0.0959f, -1.0526f, -1 },
};
static const int NUM_SIDES = sizeof(SIDES) / sizeof(SIDES[0]);
