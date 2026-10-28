# Adafruit LT8912B

Arduino support for the LT8912B MIPI DSI to HDMI/DVI bridge. The bridge is
configured over I2C; video arrives through its separate MIPI DSI input.
Sending pixels over I2C is not supported by this chip.

The portable driver uses Wire and Adafruit BusIO. The `Adafruit_LT8912B_P4` helper creates
the DSI display and exposes its ESP-IDF panel handle for drawing and separate
video integration. Other processors may use the bridge driver with their own
DSI host implementation.

This is an initial **0.1.0 review snapshot**. The originating Metro P4 sketches
have produced working video on hardware; that does not establish hardware
validation of this library port. HDMI audio remains unresolved and is not
included in this version.

## Getting started

Install this folder as an Arduino library, together with **Adafruit BusIO**,
**Adafruit GFX Library**, and **Adafruit TestBed**. The P4 examples require an
ESP32-P4 board with PSRAM. They compile with Arduino-ESP32 3.3.11 and the
current 4.0.x source core. The helper uses
the legacy PHY clock selection tested on the original P4 silicon.

Select your ESP32-P4 board, or **ESP32P4 Dev Module** for a generic board,
and enable **PSRAM** in the Arduino board menu. Set `RESET_PIN`, `I2C_SDA`,
and `I2C_SCL` at the top of each P4 sketch to match your LT8912B wiring.
The reset pin defaults to the board's `PIN_DSI_RESET` when available;
otherwise GPIO 20 is an example that you must check for your board.
I2C defaults to the selected board's `SDA` and `SCL` pins. The updated
**Adafruit Metro ESP32-P4** definition supplies these defaults automatically.
That board definition is proposed in
[Arduino-ESP32 #12980](https://github.com/espressif/arduino-esp32/pull/12980).

- `bridge_status`: portable I2C connection and bridge status example.
- `p4_colorbars`: ESP32-P4 framebuffer output with a static test pattern.
- `p4_graphics`: ESP32-P4 output using an Adafruit GFX canvas.

The P4 examples use the previously tested 800 x 480 video timing.
Check the pin assignments and timing configuration
before using another board or monitor. Monitor acceptance of a timing or sync
polarity must be tested. The examples keep their fixed mode; applications can
optionally select the display's preferred timing with `readEDIDTiming()`.

Keep the Adafruit adapter's **DDC switch OFF**, with no QT cable bypassing it,
while running these fixed-mode examples. Connecting the monitor DDC bus to the control
bus can introduce conflicting I2C devices. The bridge control addresses are
0x48, 0x49, and 0x4A.

## Optional EDID timing

`Adafruit_LT8912B::readEDIDTiming(timing, &Wire)` reads the display at 0x50 and
updates `timing` only if the base block has a valid header/checksum and a
supported preferred detailed timing. `parseEDIDTiming(data, length, timing)`
decodes an EDID block that the application has already read, avoiding a second
I2C transfer. Both return `false` and leave the caller's fallback unchanged on
failure. Neither function configures the bridge or writes EEPROM contents.

```cpp
LT8912B_Timing timing; // Caller-selected fallback.
if (bridge.readEDIDTiming(timing)) {
  Serial.print("Using EDID preferred timing.\n\r");
} else {
  Serial.print("Using fallback timing.\n\r");
}
// After checking host limits and initializing bridge:
// display.begin(bridge, timing) configures both the P4 DSI host and bridge.
```

DDC must be connected for the read. If it shares the bridge's I2C bus, hold
the bridge in reset and check that no display device responds at 0x48-0x4A
before bridge configuration; use a separate DDC bus or isolate DDC if there is
a conflict. The optional `wire` argument can select that separate bus.

The decoder supports the first preferred detailed timing in EDID 1.3/1.4:
progressive digital video with separate H/V sync, no stereo or borders, and
sync widths supported by the bridge. It extracts resolution, pixel clock,
front/back porches, sync widths and polarities. It does not search extensions
or alternate modes. The caller must check framebuffer dimensions, memory and
DSI-host clock/bandwidth limits before applying the timing. A valid EDID does
not guarantee that every advertised mode works with the selected DSI host.

## Video playback

SD access and video decoding belong in a separate player. The existing
[Metro P4 SD video sketch](https://github.com/adafruit/MBAdafruitBoards/tree/master/Development/Video/LT8912B%20DSI%20DVI/Arduino/MetroP4DviVideo)
is a reference for that integration. Its player code is GPL-2.0-only and is
not copied into this Apache-2.0 library. That demonstration plays silent
MJPEG video; MP4 soundtrack decoding is not supplied here.

## Build coverage

The Adafruit CI workflow builds all examples on its `metro_esp32p4` and
generic `esp32p4` targets, and checks formatting and standard Doxygen
documentation. The Metro target uses
the source core while the new board definition awaits upstream inclusion.
It uses the upstream [Adafruit CI scripts](https://github.com/adafruit/ci-arduino).

Local checks on 2026-10-07: all three examples compiled for the new Metro P4
board definition on the Arduino-ESP32 4.0.x source core. `p4_colorbars` also
compiled with Arduino-ESP32 3.3.11. Formatting checks passed, and Doxygen
1.8.13 using the standard CI template passed with no warnings. The new library
has not been uploaded to the board; the paused audio test is unchanged.

## Credits and license

The bridge initialization and register programming are adapted from
[Espressif's esp_lcd_lt8912b driver](https://github.com/espressif/esp-bsp/tree/master/components/lcd/esp_lcd_lt8912b).
Copyright notices are retained, and this library uses the same
**Apache License, Version 2.0**. See [license.txt](license.txt) for the complete
license and [NOTICE](NOTICE) for source provenance and the changes made for
Arduino. Dependencies retain their own licenses.
