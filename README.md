<p align="center">
    <img src="docs/images/Hero.PNG" width="100%">
</p>

<h1 align="center">
Marine RGB Light Controller
</h1>

<p align="center">
Professional ESP32-S3 based RGB LED controller for marine and automotive installations.
</p>

<p align="center">

![ESP32-S3](https://img.shields.io/badge/ESP32-S3-blue)

![Input](https://img.shields.io/badge/Input-12--24V-success)

![USB-C](https://img.shields.io/badge/USB-USB--C-informational)

![License](https://img.shields.io/badge/License-MIT-yellow)

</p>

---

# Overview

The Marine RGB Light Controller is a custom ESP32-S3 development board designed for driving addressable RGB LED strips in demanding environments.

Unlike a standard ESP32 board, it integrates a complete protected power supply, optically isolated digital inputs, 5V level shifted LED outputs and expansion interfaces on a single compact PCB.

Designed primarily for marine installations, the controller is equally suited for automotive, off-road and industrial lighting applications.

---

# Highlights

- ⚡ 12–24V DC input
- 🛡 Reverse polarity & surge protection
- 🔌 USB-C programming
- 💡 Two independent WS281x/FastLED outputs
- 🔀 5V logic level shifting
- 🔒 Two optically isolated digital inputs
- 📡 Wi-Fi & Bluetooth
- 🔧 GPIO & I²C expansion
- ⚓ Designed for marine environments

---

# Hardware Overview

<p align="center">
<img src="docs/images/board-top.png" width="900">
</p>

The controller integrates all major circuitry required for reliable LED control:

- Protected power input
- High efficiency buck converter
- Dedicated 3.3V regulator
- ESP32-S3-WROOM-1
- USB-C programming interface
- 5V level shifter
- High-speed optocouplers
- Expansion header

---

# Pinout

<p align="center">
<img src="docs/images/PINOUT 2.png" width="900">
</p>

| Interface | Description |
|------------|-------------|
| VIN | 12–24V DC Input |
| AUX Power | LED Supply |
| DATA 1 | WS281x Output |
| DATA 2 | WS281x Output |
| ISO Input 1 | Optically Isolated Input |
| ISO Input 2 | Optically Isolated Input |
| SDA / SCL | I²C Expansion |
| IO10 / IO11 | GPIO Expansion |
| 3V3 / 5V | Auxiliary Power |

---

# Gallery

<p align="center">

<img src="docs/images/board-perspective.png" width="48%">

<img src="docs/images/board-bottom.png" width="48%">

</p>

---

# Technical Specifications

| Feature | Specification |
|---------|---------------|
| MCU | ESP32-S3-WROOM-1-N8R8 |
| Input Voltage | 12–24V DC |
| LED Outputs | 2 × WS281x |
| Logic Level | 5V |
| Optocoupled Inputs | 2 |
| USB | USB-C |
| Wireless | Wi-Fi + Bluetooth |
| Expansion | GPIO + I²C |

---

# Repository Structure

```
docs/
    images/
    hardware.md
    software.md

firmware/

hardware/

README.md
```

---

# Documentation

Additional documentation can be found in the **docs** directory.

- Hardware Design
- Power Supply
- LED Outputs
- Optocoupled Inputs
- Software
- Getting Started

---

# Project Status

| Hardware | Firmware | Documentation |
|----------|----------|---------------|
| ✅ Stable | 🚧 In Development | 🚧 In Progress |

---

# Future Development

- Web interface
- OTA firmware updates
- MQTT support
- Home Assistant integration
- Additional LED effects

---

# License

Released under the MIT License.

---

<p align="center">

Designed and developed by **Watcher-33**

</p>
