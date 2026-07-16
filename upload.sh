#!/usr/bin/env bash
# Compile + upload a sketch to the Seeed XIAO ESP32-C6, then optionally monitor serial.
#
# Usage:
#   ./upload.sh                     # build+upload firmware (the cube app)
#   ./upload.sh -m                  # ... and open the serial monitor
#   ./upload.sh calibrate           # automated: calibrate -> write config.h -> flash firmware
#   ./upload.sh calibrate -m        # raw: build+upload the calibration tool + monitor (debug)
#   ./upload.sh adxl345_test -m     # build+upload an isolation test from component_tests/
#   ./upload.sh firmware /dev/cu.usbmodemXXX -m
#   PORT=/dev/cu.usbmodemXXX ./upload.sh -m
#
# The port is auto-detected from `arduino-cli board list` (first USB serial port)
# when you don't pass one or set $PORT — handy since the /dev name changes per plug-in.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# The esp32 core's build steps call `python`; our shim (-> python3) satisfies that.
export PATH="$SCRIPT_DIR/bin:$PATH"

FQBN="esp32:esp32:XIAO_ESP32C6"
SKETCH="firmware"
PORT="${PORT:-}"   # empty → auto-detect after arg parsing
MONITOR=0

# First USB serial port arduino-cli sees — the XIAO's /dev name changes per plug-in,
# so detect it instead of hard-coding. Fields in `board list` are 2+-space separated.
detect_port() {
  arduino-cli board list 2>/dev/null \
    | awk -F '  +' '/Serial Port \(USB\)/ { print $1; exit }'
}

# Resolve a sketch name to its directory: top-level `firmware`/`calibrate`, or any
# isolation test living under `component_tests/`.
sketch_dir() {
  if [ -d "$SCRIPT_DIR/$1" ]; then
    echo "$SCRIPT_DIR/$1"
  elif [ -d "$SCRIPT_DIR/component_tests/$1" ]; then
    echo "$SCRIPT_DIR/component_tests/$1"
  else
    return 1
  fi
}

for arg in "$@"; do
  case "$arg" in
    -m|--monitor)  MONITOR=1 ;;
    /dev/*)        PORT="$arg" ;;
    -*)            echo "Unknown option: $arg" >&2; exit 2 ;;
    *)
      if sketch_dir "$arg" >/dev/null; then
        SKETCH="$arg"
      else
        echo "Unknown sketch: $arg (not found at ./$arg or ./component_tests/$arg)" >&2
        exit 2
      fi
      ;;
  esac
done

if [ -z "$PORT" ]; then
  PORT="$(detect_port || true)"
  if [ -z "$PORT" ]; then
    echo "No USB serial port found. Plug in the XIAO, or pass one explicitly:" >&2
    echo "  ./upload.sh $SKETCH /dev/cu.usbmodemXXXX" >&2
    echo "  (run 'arduino-cli board list' to see available ports)" >&2
    exit 1
  fi
  echo ">> Auto-detected port: $PORT"
fi

# Compile + upload a sketch by name to $PORT.
flash() {
  local sketch="$1" dir
  dir="$(sketch_dir "$sketch")"
  echo ">> Compiling $sketch..."
  arduino-cli compile --fqbn "$FQBN" --libraries "$SCRIPT_DIR/libraries" "$dir"

  # A serial monitor (or our calibrate driver) left holding the port makes esptool
  # fail with "Resource busy". Close anything holding it before uploading.
  if command -v lsof >/dev/null 2>&1; then
    local busy
    busy="$(lsof -t "$PORT" 2>/dev/null || true)"
    if [ -n "$busy" ]; then
      echo ">> $PORT is busy (pids: $busy) — closing before upload..."
      # shellcheck disable=SC2086
      kill $busy 2>/dev/null || true
      sleep 1
    fi
  fi

  echo ">> Uploading $sketch to $PORT..."
  arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$dir"
}

# `./upload.sh calibrate` (no -m): fully automated calibration. Flash the calibrate
# sketch, walk the sides over serial, capture the emitted block straight into
# firmware/config.h, then flash the real firmware — no copy-paste, no second command.
# (`./upload.sh calibrate -m` still does the raw flash+monitor for debugging.)
if [ "$SKETCH" = "calibrate" ] && [ "$MONITOR" -eq 0 ]; then
  VENV_PY="$SCRIPT_DIR/.venv/bin/python"
  if [ ! -x "$VENV_PY" ]; then
    echo "Missing calibration venv. Create it once with:" >&2
    echo "  python3 -m venv .venv && .venv/bin/pip install pyserial" >&2
    exit 1
  fi
  flash calibrate
  echo ">> Starting calibration — rotate the cube as prompted and press Enter for each side."
  "$VENV_PY" "$SCRIPT_DIR/tools/calibrate_driver.py" "$PORT" "$SCRIPT_DIR/firmware/config.h"
  echo ">> Calibration captured into firmware/config.h — flashing firmware..."
  flash firmware
  echo ">> Done. Calibrated firmware is on the device."
  exit 0
fi

flash "$SKETCH"
echo ">> Upload complete."

if [ "$MONITOR" -eq 1 ]; then
  echo ">> Opening serial monitor at 115200 — press Ctrl-C to exit before re-uploading."
  sleep 2
  arduino-cli monitor -p "$PORT" -c baudrate=115200
fi
