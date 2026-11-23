// SPDX-FileCopyrightText: 2026 Adafruit Industries
// SPDX-License-Identifier: Apache-2.0

// HDMI audio test: color bars plus a steady 440 Hz tone on the monitor's
// speakers.
//
// Wiring, Metro ESP32-P4 to the LT8912B adapter's I2S header:
//   A0 -> BCK (BCLK), A1 -> LRC (LRCLK), A2 -> DIN
// Power, ground and I2C come over the DSI cable. Turn the adapter's DDC
// switch OFF. Use an HDMI monitor with audio (DVI inputs carry no sound).

#include <Adafruit_LT8912B_GFX565.h>
#include <ESP_I2S.h>

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This example requires an ESP32-P4 board with PSRAM."
#endif

#if defined(PIN_DSI_RESET)
const uint16_t RESET_PIN = PIN_DSI_RESET;
#else
const uint16_t RESET_PIN = 20; // Example GPIO, set to your reset pin.
#endif

const int8_t PIN_BCLK = A0;
const int8_t PIN_LRCLK = A1;
const int8_t PIN_DIN = A2;

const uint32_t SAMPLE_RATE = 48000; // The bridge is set up for 48 kHz.
const float TONE_HZ = 440;
const int32_t TONE_PEAK = 4000 << 16; // 16-bit peak in the top half.
const float TWO_PI_F = 2 * M_PI;

Adafruit_LT8912B bridge;
Adafruit_LT8912B_GFX565 display;
LT8912B_Timing timing;
I2SClass i2s;

void halt(const char* message);

void setup() {
  Serial.begin(115200);
  delay(250); // Allow serial startup without waiting for a USB connection.

  Serial.println("Adafruit LT8912B HDMI audio tone test");

  // CEA-861 1280 x 720 at 60 Hz (VIC 4). Fixed timing, because the DDC
  // switch stays off for audio, so the monitor's EDID can't be read.
  timing.width = 1280;
  timing.height = 720;
  timing.hFrontPorch = 110;
  timing.hSync = 40;
  timing.hBackPorch = 220;
  timing.vFrontPorch = 5;
  timing.vSync = 5;
  timing.vBackPorch = 20;
  timing.pixelClockMHz = 74.25;
  timing.hSyncPositive = true;
  timing.vSyncPositive = true;
  timing.vic = 4;
  timing.aspectRatio = 2; // 16:9

  if (!bridge.begin(RESET_PIN)) {
    halt("LT8912B not found. Check the DSI cable and the DDC switch.");
  }
  if (!display.begin(bridge, timing)) {
    halt("Display startup failed. Check PSRAM and the DSI connection.");
  }

  // Standard Adafruit GFX RGB565 colors: white, yellow, cyan, green,
  // magenta, red, blue, and black.
  const uint16_t bars[8] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0,
                            0xF81F, 0xF800, 0x001F, 0x0000};
  for (uint8_t bar = 0; bar < 8; bar++) {
    int16_t left = bar * display.width() / 8;
    int16_t right = (bar + 1) * display.width() / 8;
    display.fillRect(left, 0, right - left, display.height(), bars[bar]);
  }
  if (!display.show()) {
    halt("Could not display the framebuffer.");
  }

  if (!bridge.enableAudio(true)) {
    halt("Could not enable HDMI audio.");
  }

  // The LT8912B needs 32-bit I2S slots. ESP_I2S uses the data width as the
  // slot width, so send 32-bit samples.
  i2s.setPins(PIN_BCLK, PIN_LRCLK, PIN_DIN);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_STEREO)) {
    halt("I2S startup failed.");
  }
  Serial.println("Color bars and a 440 Hz tone should now be playing.");
}

void loop() {
  const size_t frames = 240; // 5 ms at 48 kHz.
  static int32_t buffer[frames * 2];
  static float phase = 0;
  const float step = TWO_PI_F * TONE_HZ / SAMPLE_RATE;

  for (size_t i = 0; i < frames; i++) {
    int32_t sample = TONE_PEAK * sinf(phase);
    buffer[2 * i] = sample;
    buffer[2 * i + 1] = sample;
    phase += step;
    if (phase > TWO_PI_F) {
      phase -= TWO_PI_F;
    }
  }
  i2s.write((uint8_t*)buffer, sizeof(buffer));
}

void halt(const char* message) {
  Serial.println(message);
  while (true) {
    delay(10);
  }
}
