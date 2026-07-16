#!/usr/bin/env bash
# One-time environment bootstrap for the Seeed XIAO ESP32-C6.
# Installs arduino-cli and the Espressif esp32 core. Safe to re-run.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESP_URL="https://espressif.github.io/arduino-esp32/package_esp32_index.json"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo ">> Installing arduino-cli via Homebrew..."
  brew install arduino-cli
else
  echo ">> arduino-cli already installed: $(arduino-cli version)"
fi

if ! arduino-cli config dump >/dev/null 2>&1; then
  echo ">> Initializing arduino-cli config..."
  arduino-cli config init
fi

echo ">> Adding Espressif board index..."
arduino-cli config add board_manager.additional_urls "$ESP_URL" 2>/dev/null || true

echo ">> Updating core index..."
arduino-cli core update-index

echo ">> Installing esp32:esp32 core (this downloads the toolchain, be patient)..."
arduino-cli core install esp32:esp32

# Python venv for the calibration driver (`./upload.sh calibrate`), which needs pyserial.
if [ ! -x "$SCRIPT_DIR/.venv/bin/python" ]; then
  echo ">> Creating .venv with pyserial (for ./upload.sh calibrate)..."
  python3 -m venv "$SCRIPT_DIR/.venv"
  "$SCRIPT_DIR/.venv/bin/pip" install --quiet --upgrade pip pyserial
else
  echo ">> Calibration venv already present."
fi

echo ">> Done. Connected boards:"
arduino-cli board list
