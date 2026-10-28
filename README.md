# Adafruit LT8912B

Arduino support for the LT8912B MIPI DSI to HDMI/DVI bridge chip. This allows microcontroller/microcomputers with DSI output like the ESP32-P4 to drive a DVI / HDMI display monitor. To set up the timing, the LT8912B bridge is first configured over I2C. Then the driver must emit video data through MIPI DSI input. 
Sending pixels over I2C is NOT supported by this chip, it's just for the config!

Examples show how to read the EDID timing from the display and then attempt to configure the bridge to match: note that the P4 has 2 lanes which means we max out at about 1024x768. Then you can use  `Adafruit_LT8912B_GFX565` which inherits from `Adafruit_GFX` and provides an ESP32-P4 DSI display with the
standard  `Adafruit_GFX` drawing, text, bitmap, and rotation functions. Only the P4 is supported right now. Note that GFX assumes 16-bit RGB565 colorspace format and for now we just drop 888 to 565!

GFX drawing targets framebuffer 0. `getFrameBuffer()` and `getPanel()` remain
available for direct 24-bit drawing and separate video integration.
Drawing repeatedly into an active single buffer can tear, double buffer for clean draws!

We tried to get HDMI audio working but not successful yet (PR's welcome!) We do not have the under-NDA LT8912B datasheet and prefer to keep this codebase NDA free :)

## EDID Conflicts with RTD266x

To read EDID, turn the Adafruit adapter's **DDC switch ON** before starting.
The examples hold the bridge in reset while reading, then check its control
addresses at 0x48, 0x49, and 0x4A for conflicts. 
DDC can stay connected when
those addresses are clear.

Some monitors use RTD266x chips which expose the programming interface over DDC, which can conflict with the LT8912B's programming address. In such cases, isolate a conflicting
monitor by turning the **DDC switch OFF** and you can connect to the DDC/EDID interface over the separate Stemma QT connection. Or, with DDC fully disconnected at startup, the examples use the fallback mode.

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

The decoder supports the first preferred detailed timing in EDID 1.3/1.4:
progressive digital video with separate H/V sync, no stereo or borders, and
sync widths supported by the bridge. It extracts resolution, pixel clock,
front/back porches, sync widths and polarities. It does not search extensions
or alternate modes. The caller must check framebuffer dimensions, memory and
DSI-host clock/bandwidth limits before applying the timing. A valid EDID does
not guarantee that every advertised mode works with the selected DSI host.

## Credits and license

The bridge initialization and register programming are adapted from
[Espressif's esp_lcd_lt8912b driver](https://github.com/espressif/esp-bsp/tree/master/components/lcd/esp_lcd_lt8912b).
Copyright notices are retained, and this library uses the same
**Apache License, Version 2.0**. See [license.txt](license.txt) for the complete
license and [NOTICE](NOTICE) for source provenance and the changes made for
Arduino. Dependencies retain their own licenses.
