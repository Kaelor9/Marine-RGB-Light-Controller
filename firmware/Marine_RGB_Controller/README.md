# Marine RGB Controller Firmware

This folder contains a replacement firmware project for the Marine RGB Light Controller.

## Implemented in this version

- Responsive browser interface
- Hue wheel and quick colors
- Brightness and smooth fade control
- Static Color
- Rainbow Flow
- Full-strip Color Fade
- Disco
- Sparkle
- Live browser synchronization without page reload
- Persistent configuration using ESP32 Preferences
- Wi-Fi provisioning through WiFiManager
- mDNS
- Browser-based OTA firmware upload at `/update`
- Configurable LED count, color order and maximum brightness
- Configurable actions for isolated inputs
- Wi-Fi reset and factory reset

## Required libraries

Install these through Arduino Library Manager:

- FastLED
- WiFiManager

The other libraries are supplied by the ESP32 Arduino core:

- WiFi
- WebServer
- ESPmDNS
- Preferences
- HTTPUpdateServer

## Board target

The repository workflow currently targets an ESP32-S3 board. Confirm that its FQBN enables:

- 8 MB flash
- OPI PSRAM
- USB CDC as required by your PCB

## Hardware defaults

Edit `Config.h` if the PCB pinout differs.

Current defaults:

- LED output 1: GPIO4
- Isolated input 1: GPIO38
- LED count: 27
- LED color order: BRG
- Isolated input 2: disabled until its GPIO is confirmed
- LED output 2: reserved for a later independent-zone implementation

## Important note about output 2

This release intentionally treats the controller as one LED zone. The second hardware output should not be enabled by guessing its pin. Once the exact GPIO and desired behavior are confirmed, it can be implemented as:

- mirrored output
- independent zone
- split strip
- synchronized effect output

## Web addresses

After Wi-Fi setup:

- Main interface: `http://rgb.local`
- OTA firmware upload: `http://rgb.local/update`

Default OTA page credentials:

- Username: `admin`
- Password: `marine-rgb`

Change those credentials before distributing the firmware commercially.

## Files

```text
Marine_RGB_Controller/
├── Marine_RGB_Controller.ino
├── Config.h
├── WebUi.h
└── README.md
```

## First test procedure

1. Back up the existing firmware.
2. Confirm GPIO4 and GPIO38 against the schematic.
3. Compile for the ESP32-S3 target.
4. Flash through USB.
5. Connect to `Marine RGB Setup`.
6. Open `http://rgb.local`.
7. Test static color with a low brightness limit.
8. Test every effect.
9. Test isolated input 1.
10. Confirm color order before increasing brightness.
