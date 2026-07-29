<p align="center">
    <img src="docs/images/Hero.png" width="100%">
</p>

<h1 align="center">Marine RGB Light Controller</h1>

<p align="center">
Professional ESP32-S3 controller for addressable RGB LED installations in marine, automotive and industrial environments.
</p>

<p align="center">

![ESP32-S3](https://img.shields.io/badge/ESP32-S3-blue)
![Input](https://img.shields.io/badge/Input-12--24V-success)
![USB-C](https://img.shields.io/badge/USB-USB--C-informational)
![License](https://img.shields.io/badge/License-All%20Rights%20Reserved-red)

</p>

---

# About

The **Marine RGB Light Controller** is a custom-designed ESP32-S3 controller built for reliable operation in demanding electrical environments.

Unlike generic ESP32 development boards, it combines protected power electronics, optically isolated inputs, dual level-shifted LED outputs and dedicated expansion interfaces into a single compact PCB.

Originally developed for marine lighting systems, the controller is equally suitable for automotive, off-road, RV and industrial LED installations.

---

# Key Features

- ⚡ Protected 12–24V DC controller input
- 🔋 Direct VIN pass-through LED power output
- 🌈 Two independent 5V level-shifted LED data outputs
- 🔒 Two high-speed optically isolated digital inputs
- 📡 Wi-Fi & Bluetooth LE
- 🔌 USB-C programming
- 🔧 GPIO & I²C expansion
- 🛡 TVS, fuse and reverse polarity protection
- ⚓ Designed for harsh electrical environments

---

# Hardware

<p align="center">
<img src="docs/images/board-top.png" width="900">
</p>

The PCB integrates everything required for a professional LED controller:

- ESP32-S3-WROOM-1-N8R8
- High-efficiency buck converter
- Low-noise 3.3V LDO
- USB-C programming interface
- Dual 5V level shifters
- High-speed optocouplers
- Protected controller power supply
- GPIO & I²C expansion
- Dedicated LED power distribution

---

# Technical Specifications

| Feature | Specification |
|---------|---------------|
| MCU | ESP32-S3-WROOM-1-N8R8 |
| Input Voltage | 12–24V DC |
| Controller Protection | Fuse + Reverse Polarity + TVS |
| LED Supply | Direct VIN Pass-through |
| LED Outputs | 2 × 5V Logic (Level Shifted) |
| Digital Inputs | 2 × Optically Isolated |
| Wireless | Wi-Fi 2.4GHz + Bluetooth LE |
| USB | USB-C |
| Expansion | GPIO + I²C |

---

# Pinout

<p align="center">
<img src="docs/images/PINOUT 2.png" width="900">
</p>

| Connection | Function |
|------------|----------|
| VIN | 12–24V DC Input |
| AUX Power | Direct VIN Pass-through |
| DATA 1 | 5V LED Data Output |
| DATA 2 | 5V LED Data Output |
| ISO IN1 | Optically Isolated Input |
| ISO IN2 | Optically Isolated Input |
| SDA / SCL | I²C Expansion |
| IO8 / IO9 | GPIO Expansion |
| IO10 / IO11 | GPIO Expansion |
| 3V3 | 3.3V Output |
| 5V | 5V Output |
| GND | Ground |

---

# Gallery

<p align="center">

<img src="docs/images/board-perspective.png" width="48%">

<img src="docs/images/board-bottom.png" width="48%">

</p>

---

# Software

The firmware is built around the ESP32-S3 platform and is designed to provide a modern and responsive user experience.

Current and planned features include:

- Modern Web UI
- RGB Color Picker
- Brightness Control
- LED Effects
- Wi-Fi Configuration
- mDNS Support
- OTA Firmware Updates
- FastLED Support
- Configuration Storage

---

# Repository Structure

```text
.
├── docs/
│   ├── images/
│   ├── hardware.md
│   └── software.md
│
├── firmware/
│
├── hardware/
│
└── README.md
```

---

# Documentation

Detailed documentation is available inside the **docs** folder.

- Hardware Design
- Power Supply
- LED Outputs
- Optically Isolated Inputs
- Pinout
- Firmware
- Getting Started

---

# Important Information

> [!IMPORTANT]
>
> The **LED power output is a direct pass-through of the input supply voltage.**
>
> - LED VOUT = VIN
> - The LED supply is **not reverse polarity protected**
> - The LED supply is **not fused**
> - Only the controller electronics are protected by the onboard fuse, TVS diode and reverse polarity protection
> - Add external protection where required by your installation

---

# Project Status

| Hardware | Firmware | Documentation |
|----------|----------|---------------|
| ✅ Revision 1 Complete | 🚧 In Development | 🚧 In Progress |

---

# Roadmap

- [x] Hardware Revision 1
- [x] Protected Power Supply
- [x] USB-C Programming
- [x] Optically Isolated Inputs
- [x] Dual LED Outputs
- [ ] Complete Web Interface
- [ ] OTA Updates
- [ ] MQTT Support
- [ ] Home Assistant Integration
- [ ] Additional LED Effects
- [ ] Configuration Backup & Restore

---

# License

This project is proprietary.

Copyright © 2026 Anders Merrild. All rights reserved.

No permission is granted to copy, modify, distribute, manufacture,
publish, sublicense, or use any part of this project without prior
written permission from the copyright holder.

See the [LICENSE](LICENSE) file for full terms.
---

<p align="center">

Designed and developed by <strong>Watcher-33</strong>

</p>
