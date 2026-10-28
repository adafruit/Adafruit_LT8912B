// SPDX-FileCopyrightText: 2026 Adafruit Industries
// SPDX-License-Identifier: Apache-2.0

#include <Adafruit_LT8912B_GFX565.h>
#include <Adafruit_TestBed.h>

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This example requires an ESP32-P4 board with PSRAM."
#endif

// Set the reset pin to match your ESP32-P4 board and LT8912B wiring.
#if defined(PIN_DSI_RESET)
const uint16_t RESET_PIN = PIN_DSI_RESET;
#else
const uint16_t RESET_PIN = 20; // Example GPIO; change to your bridge's reset pin.
#endif

const uint32_t DSI_LANE_BIT_RATE_MBPS = 960;

Adafruit_TestBed testbed;
Adafruit_LT8912B bridge;
Adafruit_LT8912B_GFX565 display;

LT8912B_Timing timing; // Fall back to 800 x 480 if EDID cannot be used.

void halt(const char *message);

void setup() {
  Serial.begin(115200);
  delay(250); // Allow serial startup without waiting for a USB connection.

  Serial.println("Adafruit LT8912B P4 color bars test");

  // Hold the bridge in reset while reading the monitor's EDID over DDC.
  // Turn the adapter DDC switch ON before starting to read the monitor.
  digitalWrite(RESET_PIN, LOW);
  pinMode(RESET_PIN, OUTPUT);
  delay(10);
  bool usingEDID = bridge.readEDIDTiming(timing);
  // RGB888 needs 24 bits per pixel over the helper's two DSI data lanes.
  if (usingEDID && timing.pixelClockMHz * 24 > 2 * DSI_LANE_BIT_RATE_MBPS) {
    Serial.print("EDID mode exceeds the configured DSI bandwidth.\n\r");
    timing = LT8912B_Timing();
    usingEDID = false;
  }
  if (usingEDID) {
    Serial.print("Using the monitor's preferred EDID timing.\n\r");
  } else {
    Serial.print("No usable EDID timing; using the 800 x 480 fallback.\n\r");
  }

  // With the bridge held in reset, its three addresses should be silent.
  // An EDID response at 0x50 is normal and does not conflict with the bridge.
  for (uint8_t address = LT8912B_I2C_MAIN; address <= LT8912B_I2C_AVI; address++) {
    if (testbed.scanI2CBus(address)) {
      Serial.print("I2C address conflict at 0x");
      Serial.print(address, HEX);
      Serial.print(". Check reset wiring or disconnect monitor DDC to continue.\n\r");
      while (testbed.scanI2CBus(address)) {
        delay(100);
      }
    }
  }

  if (!bridge.begin(RESET_PIN)) {
    halt("LT8912B not found.");
  }

  if (!display.begin(bridge, timing, 1, DSI_LANE_BIT_RATE_MBPS)) {
    if (!usingEDID) {
      halt("Display startup failed. Check PSRAM and the DSI connection.");
    }
    Serial.print("Could not start the EDID mode; trying 800 x 480.\n\r");
    timing = LT8912B_Timing();
    if (!bridge.reset()) {
      halt("Could not reset the bridge.");
    }
    if (!display.begin(bridge, timing, 1, DSI_LANE_BIT_RATE_MBPS)) {
      halt("Display startup failed. Check PSRAM and the DSI connection.");
    }
  }

  // Standard Adafruit GFX RGB565 colors: white, yellow, cyan, green,
  // magenta, red, blue, and black.
  const uint16_t bars[8] = {
    0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000
  };
  for (uint8_t bar = 0; bar < 8; bar++) {
    int16_t left = (uint32_t)bar * display.width() / 8;
    int16_t right = (uint32_t)(bar + 1) * display.width() / 8;
    display.fillRect(left, 0, right - left, display.height(), bars[bar]);
  }
  if (!display.show()) {
    halt("Could not display the framebuffer.");
  }
  Serial.println("Static color bars displayed in DVI mode.");
}

void loop() {
  delay(100);
}

void halt(const char *message) {
  Serial.println(message);
  while (true) {
    delay(10);
  }
}
