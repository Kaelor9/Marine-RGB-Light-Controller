# Prism Firmware — v0.4.3

Firmware for the Marine RGB Light Controller hardware.

## Input configuration

The isolated input pins are fixed by the PCB:

- Input 1 = GPIO38
- Input 2 = GPIO39

Users do not enter GPIO numbers in the app. Under **Settings → Advanced settings**, both inputs can be enabled or disabled and assigned an action. Input enable changes take effect immediately.

Available actions include cycle preset colors, toggle power / hold to dim, next or previous effect, brightness up or down, warm white, and red, green or blue scenes.

## Light interface

- Embedded 6 × 60° perceptual hue wheel
- Hue-wheel selector remains fully inside the wheel
- Brightness slider integrated into the selected-color tile
- Static color, Rainbow Flow, Color Fade, Disco and Sparkle
- Smooth static transitions at approximately 60 FPS
- Warm-white compensation limited to the calibrated warm-white area

## Configuration notes

Changing LED count, color order or mDNS hostname requires a controller restart. Input enable settings apply immediately.

## Other included features

- Wi-Fi provisioning
- mDNS
- PWA name: Prism
- Embedded app logo
- OTA update page at `/update`
