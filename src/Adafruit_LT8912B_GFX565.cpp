// SPDX-FileCopyrightText: 2026 Adafruit Industries
// SPDX-License-Identifier: Apache-2.0
/** @file Adafruit_LT8912B_GFX565.cpp
 * @brief Adafruit GFX drawing through ESP32-P4 DSI and the LT8912B.
 */
#include "Adafruit_LT8912B_GFX565.h"
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32P4)
#include <math.h>

#include "esp_lcd_panel_ops.h"

/** Create an uninitialized P4 DSI host. */
Adafruit_LT8912B_GFX565::Adafruit_LT8912B_GFX565() : Adafruit_GFX(0, 0) {}

/** Release display resources. Keep the bridge alive until this completes. */
Adafruit_LT8912B_GFX565::~Adafruit_LT8912B_GFX565() {
  end();
}

/** Start two-lane RGB888 DSI and configure the bridge for video-only DVI.
 * @param bridge Bridge whose begin() has already succeeded.
 * @param timing Matching DSI and bridge timing; pixel clock is in MHz.
 * @param buffers Number of panel-owned framebuffers, from 1 through 3.
 * @param laneBitRateMbps DSI rate per lane in megabits per second.
 * @return True on success. Failure releases partially allocated host resources.
 * @note Uses P4 internal PHY LDO channel 3 at 2.5 V. begin() is single-owner;
 * call end() before changing timing. This does not configure the I2C pins.
 */
bool Adafruit_LT8912B_GFX565::begin(Adafruit_LT8912B& bridge,
                                    const LT8912B_Timing& timing,
                                    uint8_t buffers, uint32_t laneBitRateMbps) {
  uint32_t hTotal = (uint32_t)timing.width + timing.hFrontPorch + timing.hSync +
                    timing.hBackPorch;
  uint32_t vTotal = (uint32_t)timing.height + timing.vFrontPorch +
                    timing.vSync + timing.vBackPorch;
  if (_bridge || buffers < 1 || buffers > 3) {
    return false;
  }
  if (!timing.width || !timing.height || !laneBitRateMbps) {
    return false;
  }
  // Adafruit GFX coordinates and dimensions are signed 16-bit values.
  if (timing.width > INT16_MAX || timing.height > INT16_MAX) {
    return false;
  }
  if (!isfinite(timing.pixelClockMHz) || timing.pixelClockMHz <= 0) {
    return false;
  }
  if (!timing.hSync || timing.hSync > 255) {
    return false;
  }
  if (!timing.vSync || timing.vSync > 255) {
    return false;
  }
  if (hTotal > 65535 || vTotal > 65535) {
    return false;
  }
  if (timing.vic > 127 || timing.aspectRatio > 2) {
    return false;
  }
  _bridge = &bridge;
  esp_ldo_channel_config_t power = {};
  power.chan_id = 3;
  power.voltage_mv = 2500;
  if (esp_ldo_acquire_channel(&power, &_power) != ESP_OK) {
    end();
    return false;
  }
  esp_lcd_dsi_bus_config_t bus = {};
  bus.bus_id = 0;
  bus.num_data_lanes = 2;
  bus.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY;
  bus.lane_bit_rate_mbps = laneBitRateMbps;
  if (esp_lcd_new_dsi_bus(&bus, &_bus) != ESP_OK) {
    end();
    return false;
  }
  esp_lcd_dpi_panel_config_t dpi = {};
  dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_PLL_F160M;
  dpi.dpi_clock_freq_mhz = timing.pixelClockMHz;
  dpi.in_color_format = LCD_COLOR_FMT_RGB888;
  dpi.out_color_format = LCD_COLOR_FMT_RGB888;
  dpi.num_fbs = buffers;
  dpi.video_timing.h_size = timing.width;
  dpi.video_timing.v_size = timing.height;
  dpi.video_timing.hsync_back_porch = timing.hBackPorch;
  dpi.video_timing.hsync_pulse_width = timing.hSync;
  dpi.video_timing.hsync_front_porch = timing.hFrontPorch;
  dpi.video_timing.vsync_back_porch = timing.vBackPorch;
  dpi.video_timing.vsync_pulse_width = timing.vSync;
  dpi.video_timing.vsync_front_porch = timing.vFrontPorch;
  dpi.flags.disable_lp = true;
  if (esp_lcd_new_panel_dpi(_bus, &dpi, &_panel) != ESP_OK) {
    end();
    return false;
  }
  // Match the vendor sequence: program the bridge, then start DPI scanout.
  if (!bridge.configure(timing, 2) || esp_lcd_panel_init(_panel) != ESP_OK) {
    end();
    return false;
  }
  esp_err_t result;
  if (buffers == 1) {
    result = esp_lcd_dpi_panel_get_frame_buffer(_panel, 1, (void**)&_pixels[0]);
  } else if (buffers == 2) {
    result = esp_lcd_dpi_panel_get_frame_buffer(_panel, 2, (void**)&_pixels[0],
                                                (void**)&_pixels[1]);
  } else {
    result = esp_lcd_dpi_panel_get_frame_buffer(_panel, 3, (void**)&_pixels[0],
                                                (void**)&_pixels[1],
                                                (void**)&_pixels[2]);
  }
  if (result != ESP_OK) {
    end();
    return false;
  }
  WIDTH = timing.width;
  HEIGHT = timing.height;
  setRotation(rotation);
  _buffers = buffers;
  return true;
}

/** Stop output and release only this object's DSI, panel, and PHY resources.
 * The caller's Wire bus and bridge object remain available.
 */
void Adafruit_LT8912B_GFX565::end() {
  if (_bridge) {
    _bridge->enableOutput(false);
  }
  if (_panel) {
    esp_lcd_panel_del(_panel);
    _panel = nullptr;
  }
  if (_bus) {
    esp_lcd_del_dsi_bus(_bus);
    _bus = nullptr;
  }
  if (_power) {
    esp_ldo_release_channel(_power);
    _power = nullptr;
  }
  for (uint8_t index = 0; index < 3; index++) {
    _pixels[index] = nullptr;
  }
  _buffers = 0;
  _bridge = nullptr;
  WIDTH = 0;
  HEIGHT = 0;
  setRotation(rotation);
}

/** Draw one RGB565 pixel into framebuffer 0, honoring GFX rotation and
 * clipping.
 * @param x Horizontal coordinate in the current rotation.
 * @param y Vertical coordinate in the current rotation.
 * @param color RGB565 color, as used by Adafruit GFX.
 * @note Call show() after drawing. Drawing before begin() or after end() is
 * ignored.
 */
void Adafruit_LT8912B_GFX565::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (!_pixels[0] || x < 0 || y < 0 || x >= _width || y >= _height) {
    return;
  }
  int16_t originalX = x;
  switch (rotation) {
    case 1:
      x = WIDTH - 1 - y;
      y = originalX;
      break;
    case 2:
      x = WIDTH - 1 - x;
      y = HEIGHT - 1 - y;
      break;
    case 3:
      x = y;
      y = HEIGHT - 1 - originalX;
      break;
  }

  // Unpack GFX's fixed RGB565 pixel format, then expand each channel to 8 bits.
  uint8_t red = (color >> 11) & 0x1F;
  uint8_t green = (color >> 5) & 0x3F;
  uint8_t blue = color & 0x1F;
  uint8_t* pixel = _pixels[0] + ((size_t)y * WIDTH + x) * 3;
  pixel[0] = blue * 255 / 31;
  pixel[1] = green * 255 / 63;
  pixel[2] = red * 255 / 31;
}

/** Pack 8-bit RGB components into the standard Adafruit GFX RGB565 format.
 * @param red Red component, 0 to 255.
 * @param green Green component, 0 to 255.
 * @param blue Blue component, 0 to 255.
 * @return Packed 16-bit RGB565 color.
 */
uint16_t Adafruit_LT8912B_GFX565::color565(uint8_t red, uint8_t green,
                                           uint8_t blue) {
  // RGB565 stores the high 5 red, 6 green, and 5 blue bits in one 16-bit value.
  return ((uint16_t)(red & 0xF8) << 8) | ((uint16_t)(green & 0xFC) << 3) |
         (blue >> 3);
}

/** Get a panel-owned framebuffer, valid until end().
 * @param index Framebuffer index, less than the begin() buffer count.
 * @return BGR byte triplets in row order, or nullptr if unavailable.
 * @note With multiple buffers, the caller must coordinate scanout completion
 * before reusing a displayed buffer; show() does not wait for vertical blank.
 */
uint8_t* Adafruit_LT8912B_GFX565::getFrameBuffer(uint8_t index) {
  if (index >= _buffers) {
    return nullptr;
  }
  return _pixels[index];
}

/** Flush CPU writes and submit a panel-owned framebuffer to the DPI driver.
 * @param index Framebuffer index, less than the begin() buffer count.
 * @return True when the IDF driver accepts the buffer, not when scanout ends.
 */
bool Adafruit_LT8912B_GFX565::show(uint8_t index) {
  uint8_t* pixels = getFrameBuffer(index);
  if (!pixels) {
    return false;
  }
  return esp_lcd_panel_draw_bitmap(_panel, 0, 0, WIDTH, HEIGHT, pixels) ==
         ESP_OK;
}

/** Access the borrowed ESP-IDF panel for renderers and frame callbacks.
 * @return Panel handle, or nullptr before begin() or after end(). Do not delete
 * the handle; this object owns it. Stop external renderers before end().
 */
esp_lcd_panel_handle_t Adafruit_LT8912B_GFX565::getPanel() {
  return _panel;
}
#endif
