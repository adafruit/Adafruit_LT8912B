// SPDX-FileCopyrightText: 2026 Adafruit Industries
// SPDX-License-Identifier: Apache-2.0

#include <Adafruit_LT8912B_P4.h>
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
#define MONITOR_EDID_ADDRESS 0x50
const uint32_t DSI_LANE_BIT_RATE_MBPS = 960;
Adafruit_TestBed testbed;
Adafruit_LT8912B bridge;
Adafruit_LT8912B_P4 display;
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

  // Disconnect DDC before bridge writes: monitor devices can share its addresses.
  if (testbed.scanI2CBus(MONITOR_EDID_ADDRESS)) {
    Serial.print("Turn the adapter DDC switch OFF and remove any QT bypass to continue.\n\r");
    while (testbed.scanI2CBus(MONITOR_EDID_ADDRESS)) {
      delay(100);
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

  // Panel framebuffer bytes are blue, green, red, in that order.
  const uint8_t bars[8][3] = {
    {255, 255, 255}, {0, 255, 255}, {255, 255, 0}, {0, 255, 0},
    {255, 0, 255},   {0, 0, 255},   {255, 0, 0},   {0, 0, 0}
  };
  uint8_t *pixels = display.getFrameBuffer();
  if (!pixels) {
    halt("No framebuffer available.");
  }
  for (uint16_t y = 0; y < timing.height; y++) {
    for (uint16_t x = 0; x < timing.width; x++) {
      uint8_t bar = (uint32_t)x * 8 / timing.width;
      *pixels++ = bars[bar][0];
      *pixels++ = bars[bar][1];
      *pixels++ = bars[bar][2];
    }
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
