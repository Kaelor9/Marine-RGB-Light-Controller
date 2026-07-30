#pragma once
#include <Arduino.h>

// ============================================================
// Marine RGB Light Controller - hardware defaults
// Change only these values if your PCB pinout differs.
// ============================================================

constexpr uint8_t LED_DATA_PIN_1 = 4;

// Output 2 is reserved for a later independent-zone implementation.
// Set to a valid GPIO when that feature is added.
constexpr int8_t LED_DATA_PIN_2 = -1;

constexpr uint8_t ISO_INPUT_1_PIN = 38;

// Set to -1 if the second isolated input is not connected yet.
constexpr int8_t ISO_INPUT_2_PIN = -1;

constexpr uint8_t ISO_INPUT_ACTIVE_LEVEL = HIGH;

constexpr uint16_t DEFAULT_LED_COUNT = 27;
constexpr uint16_t MAX_LED_COUNT = 300;

constexpr char DEFAULT_DEVICE_NAME[] = "Marine RGB";
constexpr char DEFAULT_MDNS_NAME[] = "rgb";
constexpr char WIFI_SETUP_AP_NAME[] = "Marine RGB Setup";

constexpr uint16_t WIFI_SETUP_TIMEOUT_SECONDS = 300;
constexpr uint32_t INPUT_DEBOUNCE_MS = 45;
constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 900;
constexpr uint32_t UI_POLL_INTERVAL_MS = 700;

constexpr char FIRMWARE_VERSION[] = "0.2.0";
