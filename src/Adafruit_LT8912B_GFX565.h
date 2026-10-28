// SPDX-FileCopyrightText: 2026 Adafruit Industries
// SPDX-License-Identifier: Apache-2.0
/** @file Adafruit_LT8912B_GFX565.h
 * @brief Adafruit GFX display using ESP32-P4 DSI and the LT8912B bridge.
 */
#pragma once

#include "Adafruit_LT8912B.h"
#ifdef ARDUINO_ARCH_ESP32
#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32P4
#include <Adafruit_GFX.h>

#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

/** Adafruit GFX display, separate from the portable I2C bridge driver.
 * Requires Arduino ESP32 3.3.11 or a compatible IDF 5.5 build and PSRAM.
 * The caller owns the bridge and must keep it alive until end().
 * GFX draws RGB565 colors into framebuffer 0 as BGR888; call show() to flush.
 */
class Adafruit_LT8912B_GFX565 : public Adafruit_GFX {
 public:
  Adafruit_LT8912B_GFX565();
  ~Adafruit_LT8912B_GFX565();
  /** Copying host resources is forbidden. @param other Unused source. */
  Adafruit_LT8912B_GFX565(const Adafruit_LT8912B_GFX565& other) = delete;
  /** Assignment is forbidden. @param other Unused source. @return No result. */
  Adafruit_LT8912B_GFX565& operator=(const Adafruit_LT8912B_GFX565& other) =
      delete;
  bool begin(Adafruit_LT8912B& bridge, const LT8912B_Timing& timing,
             uint8_t buffers = 1, uint32_t laneBitRateMbps = 960);
  void end();
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  static uint16_t color565(uint8_t red, uint8_t green, uint8_t blue);
  uint8_t* getFrameBuffer(uint8_t index = 0);
  bool show(uint8_t index = 0);
  esp_lcd_panel_handle_t getPanel();

 private:
  Adafruit_LT8912B* _bridge = nullptr;
  esp_ldo_channel_handle_t _power = nullptr;
  esp_lcd_dsi_bus_handle_t _bus = nullptr;
  esp_lcd_panel_handle_t _panel = nullptr;
  uint8_t* _pixels[3] = {};
  uint8_t _buffers = 0;
};
#endif
#endif
