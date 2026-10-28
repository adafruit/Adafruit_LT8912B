// SPDX-FileCopyrightText: 2026 Adafruit Industries
// SPDX-License-Identifier: Apache-2.0
/** @file Adafruit_LT8912B_EDID.cpp
 * @brief Optional EDID preferred-timing reader and decoder.
 */
#include <string.h>

#include "Adafruit_LT8912B.h"

/** Read the preferred timing from the display's EDID EEPROM at 0x50.
 * @param timing Receives the decoded timing only on success.
 * @param wire I2C bus connected to display DDC; may differ from the bridge bus.
 * @return True if a complete base block contains a supported preferred timing.
 * @note Does not require begin(), configure the bridge, or write EEPROM data.
 * When DDC shares the bridge bus, first check for conflicting devices at
 * 0x48-0x4A while holding the bridge in reset before any bridge configuration.
 * Failure leaves timing unchanged so the caller can keep its fallback mode.
 */
bool Adafruit_LT8912B::readEDIDTiming(LT8912B_Timing& timing, TwoWire* wire) {
  if (!wire) {
    return false;
  }
  Adafruit_I2CDevice device(LT8912B_EDID_ADDRESS, wire);
  if (!device.begin()) {
    return false;
  }
  uint8_t data[LT8912B_EDID_BLOCK_SIZE];
  for (uint8_t offset = 0; offset < sizeof(data); offset += 32) {
    if (!device.write_then_read(&offset, 1, data + offset, 32)) {
      return false;
    }
  }
  return parseEDIDTiming(data, sizeof(data), timing);
}

/** Decode the preferred detailed timing from an already-read EDID base block.
 * @param edid Buffer containing at least one complete 128-byte base block.
 * @param length Number of readable bytes in edid; extensions are ignored.
 * @param timing Receives the decoded timing only on success; otherwise
 * unchanged.
 * @return True for valid EDID 1.3/1.4 with a preferred progressive digital
 * separate-sync mode, no stereo or borders, and representable bridge sync
 * widths.
 * @note Validates the header and checksum. Does not search extension blocks or
 * other descriptors, or check the DSI host's clock, bandwidth, or memory
 * limits. The caller must pass the resulting timing to both the DSI host and
 * bridge; Adafruit_LT8912B_GFX565::begin() does this. VIC and aspect ratio are
 * unspecified.
 */
bool Adafruit_LT8912B::parseEDIDTiming(const uint8_t* edid, size_t length,
                                       LT8912B_Timing& timing) {
  const uint8_t header[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
  if (!edid || length < LT8912B_EDID_BLOCK_SIZE ||
      memcmp(edid, header, sizeof(header)) != 0) {
    return false;
  }
  uint8_t checksum = 0;
  for (size_t index = 0; index < LT8912B_EDID_BLOCK_SIZE; index++) {
    checksum += edid[index];
  }
  if (checksum != 0 || edid[LT8912B_EDID_VERSION] != 1 ||
      edid[LT8912B_EDID_REVISION] < 3 || edid[LT8912B_EDID_REVISION] > 4) {
    return false;
  }
  // EDID packs flags and timing fields across byte boundaries. These masks
  // decode that wire format, not read/modify/write of device registers.
  if (!(edid[LT8912B_EDID_INPUT] & LT8912B_EDID_DIGITAL) ||
      !(edid[LT8912B_EDID_FEATURES] & LT8912B_EDID_PREFERRED)) {
    return false;
  }
  const uint8_t* dtd = edid + LT8912B_EDID_DTD;
  uint16_t clock10kHz = dtd[0] | ((uint16_t)dtd[1] << 8);
  uint8_t flags = dtd[17];
  if (!clock10kHz || (flags & LT8912B_EDID_UNSUPPORTED) ||
      (flags & LT8912B_EDID_SYNC_TYPE) != LT8912B_EDID_SYNC_TYPE ||
      dtd[15] != 0 || dtd[16] != 0) {
    return false;
  }

  LT8912B_Timing preferred;
  preferred.pixelClockMHz = clock10kHz / 100.0;
  preferred.width = dtd[2] | ((uint16_t)(dtd[4] & 0xF0) << 4);
  uint16_t hBlank = dtd[3] | ((uint16_t)(dtd[4] & 0x0F) << 8);
  preferred.height = dtd[5] | ((uint16_t)(dtd[7] & 0xF0) << 4);
  uint16_t vBlank = dtd[6] | ((uint16_t)(dtd[7] & 0x0F) << 8);
  preferred.hFrontPorch = dtd[8] | ((uint16_t)(dtd[11] & 0xC0) << 2);
  preferred.hSync = dtd[9] | ((uint16_t)(dtd[11] & 0x30) << 4);
  preferred.vFrontPorch = (dtd[10] >> 4) | ((dtd[11] & 0x0C) << 2);
  preferred.vSync = (dtd[10] & 0x0F) | ((dtd[11] & 0x03) << 4);
  // Check blanking before subtraction, so malformed data cannot wrap around.
  if (!preferred.width || !preferred.height || !preferred.hSync ||
      preferred.hSync > 255 || !preferred.vSync || preferred.vSync > 255 ||
      hBlank < preferred.hFrontPorch + preferred.hSync ||
      vBlank < preferred.vFrontPorch + preferred.vSync) {
    return false;
  }
  preferred.hBackPorch = hBlank - preferred.hFrontPorch - preferred.hSync;
  preferred.vBackPorch = vBlank - preferred.vFrontPorch - preferred.vSync;
  preferred.hSyncPositive = (flags & LT8912B_EDID_HSYNC_POS) != 0;
  preferred.vSyncPositive = (flags & LT8912B_EDID_VSYNC_POS) != 0;
  timing = preferred;
  return true;
}
