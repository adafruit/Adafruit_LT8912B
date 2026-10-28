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

  Serial.println("Adafruit LT8912B P4 graphics test");

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

  // Use Adafruit GFX functions directly on the display, with RGB565 colors.
  // Draw once; repeated writes into an active single buffer can tear.
  uint16_t foreground = display.color565(80, 220, 255);
  uint16_t background = display.color565(8, 12, 24);
  uint16_t red = display.color565(255, 0, 0);
  uint16_t blue = display.color565(0, 0, 255);
  uint16_t yellow = display.color565(255, 255, 0);
  int16_t width = display.width();
  int16_t height = display.height();
  int16_t radius = height / 6;

  display.fillScreen(background);
  display.drawRoundRect(16, 16, width - 32, height - 32, 20, foreground);
  display.setTextColor(foreground);
  display.setTextSize(4);
  display.setCursor(48, 56);
  display.print("Adafruit LT8912B");
  display.setTextSize(2);
  display.setCursor(48, 112);
  display.print("ESP32-P4 DSI + Adafruit GFX");
  // Bauhaus shape colors: red square, blue circle, and yellow triangle.
  display.fillRect(width / 5 - radius, height * 3 / 5 - radius,
                   radius * 2, radius * 2, red);
  display.fillCircle(width / 2, height * 3 / 5, radius, blue);
  display.fillTriangle(width * 4 / 5 - radius, height * 3 / 5 + radius,
                       width * 4 / 5, height * 3 / 5 - radius,
                       width * 4 / 5 + radius, height * 3 / 5 + radius,
                       yellow);
  if (!display.show()) {
    halt("Could not display the framebuffer.");
  }
  Serial.print("Static GFX drawing displayed in DVI mode.\n\r");
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
