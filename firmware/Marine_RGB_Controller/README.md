# Prism Firmware — v0.3.2

Firmware for the Marine RGB Light Controller hardware.

## Input configuration

The isolated input pins are fixed by the PCB:

```cpp
Input 1 = GPIO38
Input 2 = GPIO39
```

Users do not enter GPIO numbers in the app.

Under **Settings → Advanced settings**, each input can be:

- enabled or disabled
- assigned an action
- configured without changing firmware code

Available actions include:

- Cycle preset colors
- Toggle power / hold to dim
- Next effect
- Previous effect
- Brightness up
- Brightness down
- Warm white
- Red, green or blue scene

## Other included features

- Canvas-based hue wheel
- Brightness control
- Static color, Rainbow Flow, Color Fade, Disco and Sparkle
- Configurable smooth transitions
- Warm-white compensation
- Wi-Fi provisioning
- mDNS
- PWA name: Prism
- Embedded app logo
- OTA update page at `/update`
