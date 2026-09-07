#pragma once
#include <Arduino.h>

constexpr uint8_t LED_DATA_PIN_1 = 4;

constexpr int8_t DEFAULT_ISO_INPUT_1_PIN = 38;
constexpr int8_t DEFAULT_ISO_INPUT_2_PIN = 39;

constexpr uint8_t ISO_INPUT_ACTIVE_LEVEL = HIGH;

constexpr uint16_t DEFAULT_LED_COUNT = 300;
constexpr uint16_t MAX_LED_COUNT = 300;

constexpr char DEFAULT_DEVICE_NAME[] = "Prism";
constexpr char DEFAULT_MDNS_NAME[] = "prism";
constexpr char WIFI_SETUP_AP_NAME[] = "Prism Setup";

constexpr uint8_t MAX_DEVICE_NAME_LENGTH = 31;
// Kept well below the 63 character DNS label limit; long .local names are
// awkward to type and some clients truncate them anyway.
constexpr uint8_t MAX_HOSTNAME_LENGTH = 24;

constexpr uint16_t WIFI_SETUP_TIMEOUT_SECONDS = 300;
constexpr uint32_t INPUT_DEBOUNCE_MS = 45;
constexpr uint32_t INPUT_LONG_PRESS_MS = 550;
constexpr uint32_t INPUT_DIM_STEP_MS = 90;
constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 900;

// Single source of truth for the version. The web interface in WebUi.h is
// compiled into this firmware, so it reports this value instead of carrying a
// version string of its own. Keep site/manifest.json in sync when releasing.
constexpr char FIRMWARE_VERSION[] = "0.5.6";
