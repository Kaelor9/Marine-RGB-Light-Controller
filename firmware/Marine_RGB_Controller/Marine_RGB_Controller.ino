#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <FastLED.h>
#include <WiFiManager.h>
#include <HTTPUpdateServer.h>

#include "Config.h"
#include "WebUi.h"
#include "AppLogo.h"

enum class Effect : uint8_t { Static, Rainbow, ColorFade, Disco, Sparkle, Off };

// Only single-wire (clockless) chipsets are listed here. Clock-based parts such
// as APA102/SK9822 need a second output line and RGBW parts need a fourth
// channel, so neither can be driven correctly by the current Prism hardware.
enum class LedChipset : uint8_t {
  WS2811 = 0,
  WS2812,
  WS2812B,
  WS2813,
  WS2815,
  SK6812,
  APA104,
  UCS1903,
  UCS1903B
};

enum class ColorOrder : uint8_t { RGB = 0, RBG, GRB, GBR, BRG, BGR };

// Both defaults deliberately match firmware 0.4.x, so an existing installation
// behaves exactly the same after updating even though it has no stored value.
constexpr LedChipset DEFAULT_LED_CHIPSET = LedChipset::WS2811;
constexpr ColorOrder DEFAULT_COLOR_ORDER = ColorOrder::BRG;

enum class InputAction : uint8_t {
  CycleColors,
  TogglePower,
  NextEffect,
  PreviousEffect,
  BrightnessUp,
  BrightnessDown,
  WarmWhite,
  RedScene,
  GreenScene,
  BlueScene
};

struct RuntimeState {
  uint8_t r = 255;
  uint8_t g = 128;
  uint8_t b = 40;
  uint8_t brightness = 70;
  uint8_t speed = 50;
  uint8_t intensity = 65;
  bool power = true;
  Effect effect = Effect::Static;
};

struct Settings {
  String deviceName = DEFAULT_DEVICE_NAME;
  String mdnsName = DEFAULT_MDNS_NAME;
  uint16_t ledCount = DEFAULT_LED_COUNT;
  uint8_t maxBrightness = 100;
  ColorOrder colorOrder = DEFAULT_COLOR_ORDER;
  LedChipset chipset = DEFAULT_LED_CHIPSET;
  bool restoreState = true;
  bool smoothTransitions = true;
  bool warmCompensation = true;
  uint16_t defaultFade = 800;

  bool input1Enabled = true;
  bool input2Enabled = true;

  InputAction input1Action = InputAction::CycleColors;
  InputAction input2Action = InputAction::NextEffect;
};

struct DebouncedInput {
  int8_t pin = -1;
  uint8_t lastRaw = LOW;
  uint8_t stable = LOW;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;
  uint32_t lastRepeatAt = 0;
  bool initialized = false;
  bool longPressActive = false;
  bool dimDirectionUp = true;
  bool ignoreUntilRelease = false;
};

CRGB leds[MAX_LED_COUNT];
uint16_t activeLedCount = DEFAULT_LED_COUNT;
WebServer server(80);
HTTPUpdateServer httpUpdater;
Preferences prefs;
RuntimeState state;
Settings settings;
DebouncedInput input1;
DebouncedInput input2;

bool stateDirty = false;
bool outputDirty = true;
uint32_t stateDirtyAt = 0;
uint32_t stateVersion = 1;

CRGB displayedColor = CRGB::Black;
CRGB transitionFrom = CRGB::Black;
CRGB transitionTo = CRGB::Black;
uint32_t transitionStartedAt = 0;
uint16_t transitionDuration = 0;
bool transitionActive = false;

// The web UI sends brightness targets live while the slider is moving. The
// physical output follows those targets through a short local smoothing stage,
// so network timing cannot turn a continuous finger gesture into visible steps.
// Q8 keeps sub-byte precision without using floating point in the render loop.
uint16_t displayedBrightnessQ8 = 0;
bool brightnessSmoothingInitialized = false;
uint32_t lastBrightnessSmoothingAt = 0;

uint8_t rainbowOffset = 0;
uint8_t fadeHue = 0;
uint32_t lastEffectFrame = 0;
uint32_t lastDiscoChange = 0;
CRGB discoColor = CRGB::Red;

const CRGB PRESET_COLORS[] = {
  CRGB(255, 0, 0),
  CRGB(0, 255, 0),
  CRGB(0, 0, 255),
  CRGB(255, 190, 0),
  CRGB(170, 0, 255),
  CRGB(0, 220, 190),
  CRGB(255, 128, 40),
  CRGB::Black
};
constexpr size_t PRESET_COLOR_COUNT = sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0]);
size_t presetIndex = 0;

String jsonEscape(const String& value) {
  String result;
  result.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '\\') result += "\\\\";
    else if (c == '"') result += "\\\"";
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else result += c;
  }
  return result;
}

uint8_t clampPercent(int value) {
  return static_cast<uint8_t>(constrain(value, 1, 100));
}

const char* effectName(Effect effect) {
  switch (effect) {
    case Effect::Static: return "static";
    case Effect::Rainbow: return "rainbow";
    case Effect::ColorFade: return "fade";
    case Effect::Disco: return "disco";
    case Effect::Sparkle: return "sparkle";
    case Effect::Off: return "off";
  }
  return "static";
}

// Returns false when the value is not a known effect, so the caller can keep
// the current one instead of silently falling back to Static.
bool parseEffect(const String& value, Effect& out) {
  for (uint8_t i = 0; i <= static_cast<uint8_t>(Effect::Off); ++i) {
    if (value == effectName(static_cast<Effect>(i))) {
      out = static_cast<Effect>(i);
      return true;
    }
  }
  return false;
}

const char* ledChipsetName(LedChipset chipset) {
  switch (chipset) {
    case LedChipset::WS2811: return "WS2811";
    case LedChipset::WS2812: return "WS2812";
    case LedChipset::WS2812B: return "WS2812B";
    case LedChipset::WS2813: return "WS2813";
    case LedChipset::WS2815: return "WS2815";
    case LedChipset::SK6812: return "SK6812";
    case LedChipset::APA104: return "APA104";
    case LedChipset::UCS1903: return "UCS1903";
    case LedChipset::UCS1903B: return "UCS1903B";
  }
  return "WS2811";
}

bool parseLedChipset(const String& value, LedChipset& out) {
  String candidate(value);
  candidate.trim();
  candidate.toUpperCase();
  for (uint8_t i = 0; i <= static_cast<uint8_t>(LedChipset::UCS1903B); ++i) {
    if (candidate == ledChipsetName(static_cast<LedChipset>(i))) {
      out = static_cast<LedChipset>(i);
      return true;
    }
  }
  return false;
}

const char* colorOrderName(ColorOrder order) {
  switch (order) {
    case ColorOrder::RGB: return "RGB";
    case ColorOrder::RBG: return "RBG";
    case ColorOrder::GRB: return "GRB";
    case ColorOrder::GBR: return "GBR";
    case ColorOrder::BRG: return "BRG";
    case ColorOrder::BGR: return "BGR";
  }
  return "BRG";
}

bool parseColorOrder(const String& value, ColorOrder& out) {
  String candidate(value);
  candidate.trim();
  candidate.toUpperCase();
  for (uint8_t i = 0; i <= static_cast<uint8_t>(ColorOrder::BGR); ++i) {
    if (candidate == colorOrderName(static_cast<ColorOrder>(i))) {
      out = static_cast<ColorOrder>(i);
      return true;
    }
  }
  return false;
}

// Reduces any input to a single RFC 1123 style hostname label: lowercase
// letters, digits and inner hyphens only. Separators collapse into one hyphen
// and the name never starts or ends with one. Falls back to the default when
// nothing usable is left, so mDNS can never be started with an invalid name.
String sanitizeHostname(const String& value) {
  String result;
  result.reserve(value.length() + 1);
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (allowed) {
      result += c;
    } else if (result.length() > 0 && result[result.length() - 1] != '-') {
      result += '-';
    }
    if (result.length() >= MAX_HOSTNAME_LENGTH) break;
  }
  while (result.length() > 0 && result[result.length() - 1] == '-') {
    result.remove(result.length() - 1);
  }
  if (result.length() == 0) return String(DEFAULT_MDNS_NAME);
  return result;
}

// Strips control characters and enforces the length limit without cutting a
// multi-byte UTF-8 sequence in half, which would corrupt /api/state.
String sanitizeDeviceName(const String& value) {
  String result;
  result.reserve(value.length() + 1);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c < 0x20 || c == 0x7F) continue;
    const bool continuation = (c & 0xC0) == 0x80;
    if (result.length() >= MAX_DEVICE_NAME_LENGTH && !continuation) break;
    result += static_cast<char>(c);
  }
  result.trim();
  if (result.length() == 0) return String(DEFAULT_DEVICE_NAME);
  return result;
}

const char* inputActionName(InputAction action) {
  switch (action) {
    case InputAction::CycleColors: return "colors";
    case InputAction::TogglePower: return "power";
    case InputAction::NextEffect: return "effects";
    case InputAction::PreviousEffect: return "previous-effect";
    case InputAction::BrightnessUp: return "brightness-up";
    case InputAction::BrightnessDown: return "brightness-down";
    case InputAction::WarmWhite: return "warm-white";
    case InputAction::RedScene: return "red";
    case InputAction::GreenScene: return "green";
    case InputAction::BlueScene: return "blue";
  }
  return "colors";
}

bool parseInputAction(const String& value, InputAction& out) {
  for (uint8_t i = 0; i <= static_cast<uint8_t>(InputAction::BlueScene); ++i) {
    if (value == inputActionName(static_cast<InputAction>(i))) {
      out = static_cast<InputAction>(i);
      return true;
    }
  }
  return false;
}

void markStateChanged() {
  ++stateVersion;
  stateDirty = true;
  outputDirty = true;
  stateDirtyAt = millis();
}

void startStaticTransition(const CRGB& target) {
  transitionFrom = displayedColor;
  transitionTo = target;
  transitionStartedAt = millis();
  transitionDuration = settings.defaultFade;
  transitionActive = settings.smoothTransitions && transitionDuration > 0;
  if (!transitionActive) displayedColor = target;
}

void setStaticColor(uint8_t r, uint8_t g, uint8_t b) {
  state.r = r;
  state.g = g;
  state.b = b;
  state.effect = Effect::Static;
  state.power = true;
  startStaticTransition(CRGB(r, g, b));
  markStateChanged();
}

void setBrightness(int value) {
  state.brightness = clampPercent(value);
  state.power = true;
  if (state.effect == Effect::Off) state.effect = Effect::Static;
  markStateChanged();
}

void loadPreferences() {
  prefs.begin("marine-rgb", true);

  // Stored values are re-validated here as well. Preferences written by an
  // older firmware, a manual NVS edit or a partially failed write must never be
  // able to put the controller into an invalid state.
  settings.deviceName = sanitizeDeviceName(prefs.getString("device", DEFAULT_DEVICE_NAME));
  settings.mdnsName = sanitizeHostname(prefs.getString("mdns", DEFAULT_MDNS_NAME));
  settings.ledCount = constrain(prefs.getUShort("leds", DEFAULT_LED_COUNT), 1, MAX_LED_COUNT);
  settings.maxBrightness = constrain(prefs.getUChar("maxb", 100), 1, 100);

  settings.colorOrder = DEFAULT_COLOR_ORDER;
  parseColorOrder(prefs.getString("order", colorOrderName(DEFAULT_COLOR_ORDER)), settings.colorOrder);

  settings.chipset = DEFAULT_LED_CHIPSET;
  parseLedChipset(prefs.getString("chipset", ledChipsetName(DEFAULT_LED_CHIPSET)), settings.chipset);
  settings.restoreState = prefs.getBool("restore", true);
  settings.smoothTransitions = prefs.getBool("smooth", true);
  settings.warmCompensation = prefs.getBool("warmcomp", true);
  settings.defaultFade = constrain(prefs.getUShort("fade", 800), 0, 5000);

  settings.input1Enabled = prefs.getBool("in1en", true);
  settings.input2Enabled = prefs.getBool("in2en", true);

  const uint8_t input1Raw = prefs.getUChar("in1", static_cast<uint8_t>(InputAction::CycleColors));
  const uint8_t input2Raw = prefs.getUChar("in2", static_cast<uint8_t>(InputAction::NextEffect));
  settings.input1Action = input1Raw <= static_cast<uint8_t>(InputAction::BlueScene)
    ? static_cast<InputAction>(input1Raw)
    : InputAction::CycleColors;
  settings.input2Action = input2Raw <= static_cast<uint8_t>(InputAction::BlueScene)
    ? static_cast<InputAction>(input2Raw)
    : InputAction::NextEffect;

  if (settings.restoreState) {
    state.r = prefs.getUChar("r", 255);
    state.g = prefs.getUChar("g", 128);
    state.b = prefs.getUChar("b", 40);
    state.brightness = constrain(prefs.getUChar("bright", 70), 1, 100);
    state.speed = constrain(prefs.getUChar("speed", 50), 1, 100);
    state.intensity = constrain(prefs.getUChar("intens", 65), 1, 100);
    state.power = prefs.getBool("power", true);
    const uint8_t effectRaw = prefs.getUChar("effect", static_cast<uint8_t>(Effect::Static));
    state.effect = effectRaw <= static_cast<uint8_t>(Effect::Off)
      ? static_cast<Effect>(effectRaw)
      : Effect::Static;
  }

  prefs.end();
}

void savePreferences() {
  prefs.begin("marine-rgb", false);
  prefs.putString("device", settings.deviceName);
  prefs.putString("mdns", settings.mdnsName);
  prefs.putUShort("leds", settings.ledCount);
  prefs.putUChar("maxb", settings.maxBrightness);
  prefs.putString("order", colorOrderName(settings.colorOrder));
  prefs.putString("chipset", ledChipsetName(settings.chipset));
  prefs.putBool("restore", settings.restoreState);
  prefs.putBool("smooth", settings.smoothTransitions);
  prefs.putBool("warmcomp", settings.warmCompensation);
  prefs.putUShort("fade", settings.defaultFade);
  prefs.putBool("in1en", settings.input1Enabled);
  prefs.putBool("in2en", settings.input2Enabled);
  prefs.putUChar("in1", static_cast<uint8_t>(settings.input1Action));
  prefs.putUChar("in2", static_cast<uint8_t>(settings.input2Action));

  if (settings.restoreState) {
    prefs.putUChar("r", state.r);
    prefs.putUChar("g", state.g);
    prefs.putUChar("b", state.b);
    prefs.putUChar("bright", state.brightness);
    prefs.putUChar("speed", state.speed);
    prefs.putUChar("intens", state.intensity);
    prefs.putBool("power", state.power);
    prefs.putUChar("effect", static_cast<uint8_t>(state.effect));
  }

  prefs.end();
  stateDirty = false;
}

void configureInput(DebouncedInput& input, bool enabled, int8_t pin) {
  input = DebouncedInput{};
  input.pin = enabled ? pin : -1;
  if (input.pin >= 0) pinMode(input.pin, INPUT);
}

void configureInputs() {
  configureInput(input1, settings.input1Enabled, DEFAULT_ISO_INPUT_1_PIN);
  configureInput(input2, settings.input2Enabled, DEFAULT_ISO_INPUT_2_PIN);
}

// FastLED needs both the chipset and the colour order as compile-time template
// arguments, so every supported combination has to exist in the binary. The
// macro keeps that expansion in one place instead of 54 hand-written lines.
#define PRISM_ADD_LEDS(CHIPSET)                                                                      \
  switch (settings.colorOrder) {                                                                     \
    case ColorOrder::RGB: FastLED.addLeds<CHIPSET, LED_DATA_PIN_1, RGB>(leds, activeLedCount); break; \
    case ColorOrder::RBG: FastLED.addLeds<CHIPSET, LED_DATA_PIN_1, RBG>(leds, activeLedCount); break; \
    case ColorOrder::GRB: FastLED.addLeds<CHIPSET, LED_DATA_PIN_1, GRB>(leds, activeLedCount); break; \
    case ColorOrder::GBR: FastLED.addLeds<CHIPSET, LED_DATA_PIN_1, GBR>(leds, activeLedCount); break; \
    case ColorOrder::BGR: FastLED.addLeds<CHIPSET, LED_DATA_PIN_1, BGR>(leds, activeLedCount); break; \
    case ColorOrder::BRG:                                                                            \
    default: FastLED.addLeds<CHIPSET, LED_DATA_PIN_1, BRG>(leds, activeLedCount); break;              \
  }

void configureLedController() {
  // Re-clamped instead of trusted: activeLedCount indexes a fixed size buffer.
  activeLedCount = constrain(settings.ledCount, 1, MAX_LED_COUNT);

  switch (settings.chipset) {
    case LedChipset::WS2812: PRISM_ADD_LEDS(WS2812); break;
    case LedChipset::WS2812B: PRISM_ADD_LEDS(WS2812B); break;
    case LedChipset::WS2813: PRISM_ADD_LEDS(WS2813); break;
    case LedChipset::WS2815: PRISM_ADD_LEDS(WS2815); break;
    case LedChipset::SK6812: PRISM_ADD_LEDS(SK6812); break;
    case LedChipset::APA104: PRISM_ADD_LEDS(APA104); break;
    case LedChipset::UCS1903: PRISM_ADD_LEDS(UCS1903); break;
    case LedChipset::UCS1903B: PRISM_ADD_LEDS(UCS1903B); break;
    case LedChipset::WS2811:
    default: PRISM_ADD_LEDS(WS2811); break;
  }

  FastLED.setDither(true);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.clear(true);
  displayedColor = CRGB(state.r, state.g, state.b);
}

uint16_t effectFrameInterval() {
  // Static colour fades should remain smooth regardless of the effect-speed slider.
  if (state.effect == Effect::Static) return 16;  // Approximately 60 FPS.
  return static_cast<uint16_t>(map(state.speed, 1, 100, 80, 10));
}

CRGB applyWarmCompensation(CRGB color) {
  if (!settings.warmCompensation || state.effect != Effect::Static) return color;

  // Keep the original conservative compensation window. It intentionally only
  // affects the established warm-white region, so ordinary red/orange/yellow
  // wheel colours are not re-balanced unexpectedly. Broader calibration should
  // be done against the physical LED strip rather than guessed in firmware.
  const bool warmWhiteSelected =
    state.r >= 200 &&
    state.g >= 90 && state.g <= 205 &&
    state.b <= 125 &&
    state.r > state.g &&
    state.g > state.b &&
    (state.r - state.g) >= 25 &&
    (state.g - state.b) >= 25;

  if (!warmWhiteSelected || state.brightness <= 20) return color;

  const uint8_t amount = static_cast<uint8_t>(
    map(state.brightness, 20, 100, 0, 255)
  );
  const uint8_t greenReduction = scale8(80, amount);
  const uint8_t blueReduction = scale8(135, amount);

  color.g = scale8_video(color.g, 255 - greenReduction);
  color.b = scale8_video(color.b, 255 - blueReduction);
  return color;
}

void renderStatic() {
  if (transitionActive) {
    const uint32_t elapsed = millis() - transitionStartedAt;
    if (elapsed >= transitionDuration) {
      displayedColor = transitionTo;
      transitionActive = false;
    } else {
      displayedColor = blend(transitionFrom, transitionTo, static_cast<uint8_t>((elapsed * 255UL) / transitionDuration));
    }
  } else {
    displayedColor = CRGB(state.r, state.g, state.b);
  }

  fill_solid(leds, activeLedCount, applyWarmCompensation(displayedColor));
}

void renderRainbow() {
  const uint8_t delta = max<uint8_t>(1, static_cast<uint8_t>(255 / activeLedCount));
  fill_rainbow(leds, activeLedCount, rainbowOffset, delta);
  rainbowOffset += map(state.speed, 1, 100, 1, 5);
}

void renderColorFade() {
  fill_solid(leds, activeLedCount, CHSV(fadeHue, 255, 255));
  fadeHue += map(state.speed, 1, 100, 1, 4);
}

void renderDisco() {
  const uint32_t interval = map(state.speed, 1, 100, 650, 90);
  if (millis() - lastDiscoChange >= interval) {
    lastDiscoChange = millis();
    discoColor = CHSV(random8(), 255, 255);
  }
  fill_solid(leds, activeLedCount, discoColor);
}

void renderSparkle() {
  CRGB base(state.r, state.g, state.b);
  base.nscale8_video(map(state.intensity, 1, 100, 35, 145));
  fill_solid(leds, activeLedCount, base);

  const uint8_t maxSparkles = activeLedCount >= 10 ? activeLedCount / 5 : 2;
  const uint8_t sparkles = map(state.intensity, 1, 100, 1, maxSparkles);
  for (uint8_t i = 0; i < sparkles; ++i) {
    if (random8() < map(state.speed, 1, 100, 35, 180)) {
      leds[random16(activeLedCount)] = blend(CRGB(state.r, state.g, state.b), CRGB::White, 190);
    }
  }
}

uint8_t targetOutputBrightness() {
  const uint8_t limitedPercent = static_cast<uint8_t>(
    (static_cast<uint16_t>(state.brightness) * settings.maxBrightness) / 100
  );
  return static_cast<uint8_t>(map(limitedPercent, 0, 100, 0, 255));
}

bool updateBrightnessSmoothing(uint32_t now) {
  const uint16_t targetQ8 = static_cast<uint16_t>(targetOutputBrightness()) << 8;

  if (!brightnessSmoothingInitialized) {
    displayedBrightnessQ8 = targetQ8;
    brightnessSmoothingInitialized = true;
    lastBrightnessSmoothingAt = now;
    return true;
  }

  const int32_t delta = static_cast<int32_t>(targetQ8) - displayedBrightnessQ8;
  if (delta == 0) {
    lastBrightnessSmoothingAt = now;
    return false;
  }

  // Time-based exponential tracking. A short ~22 ms time constant keeps the
  // output tied closely to a finger moving the live slider, while the 120 Hz
  // render cadence fills the gaps between HTTP updates instead of exposing
  // them as visible brightness steps. Q8 retains sub-byte precision.
  uint32_t dt = now - lastBrightnessSmoothingAt;
  if (dt == 0) dt = 1;
  if (dt > 40) dt = 40; // Do not jump after a temporary Wi-Fi/main-loop stall.
  lastBrightnessSmoothingAt = now;

  constexpr uint16_t SMOOTHING_TAU_MS = 22;
  const uint32_t denominator = SMOOTHING_TAU_MS + dt;
  int32_t step = static_cast<int32_t>((static_cast<int64_t>(delta) * dt) / denominator);

  // Always make progress, and snap the final sub-byte once it is visually
  // indistinguishable. This prevents a long numerical tail around the target.
  if (step == 0) step = delta > 0 ? 1 : -1;
  if (abs(delta) <= 96 || abs(step) >= abs(delta)) {
    displayedBrightnessQ8 = targetQ8;
  } else {
    displayedBrightnessQ8 = static_cast<uint16_t>(
      static_cast<int32_t>(displayedBrightnessQ8) + step
    );
  }
  return true;
}

void updateLeds() {
  const uint32_t now = millis();

  // Animated effects and active colour transitions need a continuous frame
  // cadence. A settled static colour does not: addressable LEDs hold their last
  // frame themselves. Avoiding redundant FastLED.show() calls leaves much more
  // time for Wi-Fi/WebServer work, especially with the 300 LED default.
  const bool animatedEffect =
    state.power && state.effect != Effect::Static && state.effect != Effect::Off;
  const uint16_t brightnessTargetQ8 = static_cast<uint16_t>(targetOutputBrightness()) << 8;
  const bool brightnessSmoothingActive =
    !brightnessSmoothingInitialized || displayedBrightnessQ8 != brightnessTargetQ8;
  const bool continuousFrames = animatedEffect || transitionActive || brightnessSmoothingActive;

  if (!outputDirty && !continuousFrames) return;

  // State changes are rendered immediately. Brightness smoothing gets a
  // dedicated ~120 Hz cadence; effects keep their normal frame interval.
  const uint16_t frameInterval = brightnessSmoothingActive ? 8 : effectFrameInterval();
  if (!outputDirty && now - lastEffectFrame < frameInterval) return;
  lastEffectFrame = now;
  updateBrightnessSmoothing(now);

  if (!state.power || state.effect == Effect::Off) {
    transitionActive = false;
    displayedColor = CRGB::Black;
    fill_solid(leds, activeLedCount, CRGB::Black);
  } else {
    switch (state.effect) {
      case Effect::Static: renderStatic(); break;
      case Effect::Rainbow: renderRainbow(); break;
      case Effect::ColorFade: renderColorFade(); break;
      case Effect::Disco: renderDisco(); break;
      case Effect::Sparkle: renderSparkle(); break;
      case Effect::Off: fill_solid(leds, activeLedCount, CRGB::Black); break;
    }
  }

  FastLED.setBrightness(static_cast<uint8_t>((displayedBrightnessQ8 + 128) >> 8));
  FastLED.show();
  outputDirty = false;
}

void cyclePresetColors() {
  const CRGB color = PRESET_COLORS[presetIndex];
  presetIndex = (presetIndex + 1) % PRESET_COLOR_COUNT;
  if (color == CRGB::Black) {
    state.power = false;
    state.effect = Effect::Off;
    markStateChanged();
  } else {
    setStaticColor(color.r, color.g, color.b);
  }
}

void nextEffect(bool reverse = false) {
  constexpr int effectCount = 5; // Excludes Off from normal cycling.

  // Off sits outside the cycle, so its index must not be fed into the modulo -
  // that used to jump to Rainbow instead of the first or last effect.
  if (state.effect == Effect::Off) {
    state.effect = reverse ? Effect::Sparkle : Effect::Static;
    state.power = true;
    markStateChanged();
    return;
  }

  int current = static_cast<int>(state.effect);
  current = reverse ? (current - 1 + effectCount) % effectCount : (current + 1) % effectCount;
  state.effect = static_cast<Effect>(current);
  state.power = true;
  markStateChanged();
}

void executeShortAction(InputAction action) {
  switch (action) {
    case InputAction::CycleColors: cyclePresetColors(); break;
    case InputAction::TogglePower:
      state.power = !state.power;
      if (state.power && state.effect == Effect::Off) state.effect = Effect::Static;
      markStateChanged();
      break;
    case InputAction::NextEffect: nextEffect(false); break;
    case InputAction::PreviousEffect: nextEffect(true); break;
    case InputAction::BrightnessUp: setBrightness(state.brightness + 10); break;
    case InputAction::BrightnessDown: setBrightness(state.brightness - 10); break;
    case InputAction::WarmWhite: setStaticColor(255, 128, 40); break;
    case InputAction::RedScene: setStaticColor(255, 0, 0); break;
    case InputAction::GreenScene: setStaticColor(0, 255, 0); break;
    case InputAction::BlueScene: setStaticColor(0, 0, 255); break;
  }
}

void executeHoldStep(DebouncedInput& input, InputAction action) {
  if (action == InputAction::CycleColors) {
    cyclePresetColors();
    return;
  }
  if (action == InputAction::NextEffect) {
    nextEffect(false);
    return;
  }
  if (action == InputAction::PreviousEffect) {
    nextEffect(true);
    return;
  }

  bool increase = action != InputAction::BrightnessDown;
  if (action == InputAction::TogglePower ||
      action == InputAction::WarmWhite ||
      action == InputAction::RedScene ||
      action == InputAction::GreenScene ||
      action == InputAction::BlueScene) {
    increase = input.dimDirectionUp;
  }

  int next = state.brightness + (increase ? 2 : -2);
  if (next >= 100) {
    next = 100;
    input.dimDirectionUp = false;
  } else if (next <= 1) {
    next = 1;
    input.dimDirectionUp = true;
  }
  setBrightness(next);
}

void updateInput(DebouncedInput& input, InputAction action) {
  if (input.pin < 0) return;
  const uint32_t now = millis();
  const uint8_t raw = digitalRead(input.pin);

  if (!input.initialized) {
    input.lastRaw = raw;
    input.stable = raw;
    input.changedAt = now;
    input.pressedAt = now;
    input.lastRepeatAt = now;
    input.initialized = true;
    // An input that is already active when it is configured must not count as a
    // fresh press. pressedAt used to stay at 0, so the long-press timer fired
    // immediately and a latched switch made the light ramp on its own.
    input.ignoreUntilRelease = (raw == ISO_INPUT_ACTIVE_LEVEL);
    return;
  }

  if (raw != input.lastRaw) {
    input.lastRaw = raw;
    input.changedAt = now;
  }

  if (raw != input.stable && now - input.changedAt >= INPUT_DEBOUNCE_MS) {
    input.stable = raw;
    if (input.stable == ISO_INPUT_ACTIVE_LEVEL) {
      input.pressedAt = now;
      input.lastRepeatAt = now;
      input.longPressActive = false;
    } else {
      const bool suppressed = input.ignoreUntilRelease;
      input.ignoreUntilRelease = false;
      if (!suppressed && !input.longPressActive) executeShortAction(action);
    }
  }

  if (input.stable == ISO_INPUT_ACTIVE_LEVEL && !input.ignoreUntilRelease) {
    if (!input.longPressActive && now - input.pressedAt >= INPUT_LONG_PRESS_MS) {
      input.longPressActive = true;
      input.lastRepeatAt = 0;
    }

    if (input.longPressActive && (input.lastRepeatAt == 0 || now - input.lastRepeatAt >= INPUT_DIM_STEP_MS)) {
      input.lastRepeatAt = now;
      executeHoldStep(input, action);
    }
  }
}

String buildStateJson() {
  String json;
  json.reserve(1200);
  json += "{\"version\":" + String(stateVersion) + ",\"state\":{";
  json += "\"r\":" + String(state.r) + ",\"g\":" + String(state.g) + ",\"b\":" + String(state.b);
  json += ",\"brightness\":" + String(state.brightness);
  json += ",\"speed\":" + String(state.speed);
  json += ",\"intensity\":" + String(state.intensity);
  json += ",\"power\":" + String(state.power ? "true" : "false");
  json += ",\"effect\":\"" + String(effectName(state.effect)) + "\"},\"settings\":{";
  json += "\"deviceName\":\"" + jsonEscape(settings.deviceName) + "\"";
  json += ",\"mdnsName\":\"" + jsonEscape(settings.mdnsName) + "\"";
  json += ",\"ledCount\":" + String(settings.ledCount);
  json += ",\"maxBrightness\":" + String(settings.maxBrightness);
  json += ",\"colorOrder\":\"" + String(colorOrderName(settings.colorOrder)) + "\"";
  json += ",\"ledChipset\":\"" + String(ledChipsetName(settings.chipset)) + "\"";
  json += ",\"restoreState\":" + String(settings.restoreState ? "true" : "false");
  json += ",\"smoothTransitions\":" + String(settings.smoothTransitions ? "true" : "false");
  json += ",\"warmCompensation\":" + String(settings.warmCompensation ? "true" : "false");
  json += ",\"defaultFade\":" + String(settings.defaultFade);
  json += ",\"input1Enabled\":" + String(settings.input1Enabled ? "true" : "false");
  json += ",\"input2Enabled\":" + String(settings.input2Enabled ? "true" : "false");
  json += ",\"input1Action\":\"" + String(inputActionName(settings.input1Action)) + "\"";
  json += ",\"input2Action\":\"" + String(inputActionName(settings.input2Action)) + "\"";
  json += "},\"network\":{\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",\"rssi\":" + String(WiFi.RSSI()) + "}";
  json += ",\"firmware\":\"" + String(FIRMWARE_VERSION) + "\"}";
  return json;
}

void sendOk() {
  server.send(200, "text/plain", "OK");
}

// server.arg() returns an empty string for a present-but-empty parameter and
// String::toInt() turns anything unparsable into 0. Previously "ledCount=" or
// "maxBrightness=abc" was therefore accepted and silently clamped the setting
// to its minimum. Unparsable values now count as "not supplied".
bool argAsInt(const char* name, long& out) {
  if (!server.hasArg(name)) return false;
  String value = server.arg(name);
  value.trim();
  if (value.length() == 0 || value.length() > 10) return false;
  size_t i = (value[0] == '-' || value[0] == '+') ? 1 : 0;
  if (i >= value.length()) return false;
  for (; i < value.length(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  out = value.toInt();
  return true;
}

int argInt(const char* name, int fallback) {
  long parsed = 0;
  return argAsInt(name, parsed) ? static_cast<int>(parsed) : fallback;
}

bool argBool(const char* name, bool fallback) {
  long parsed = 0;
  if (argAsInt(name, parsed)) return parsed != 0;
  String value = server.arg(name);
  value.trim();
  value.toLowerCase();
  if (value == "true" || value == "on") return true;
  if (value == "false" || value == "off") return false;
  return fallback;
}

void setupRoutes() {
  // Keep the large embedded UI cacheable. A normal refresh can then validate a
  // tiny ETag instead of downloading the full ~300 kB page again. The ETag is
  // tied to the firmware version, so a firmware update automatically invalidates
  // the cached UI. WebServer only exposes request headers that are collected.
  const char* headerKeys[] = {"If-None-Match"};
  server.collectHeaders(headerKeys, 1);

  server.on("/", HTTP_GET, []() {
    const String etag = String("\"") + FIRMWARE_VERSION + "\"";
    server.sendHeader("ETag", etag);
    server.sendHeader("Cache-Control", "private, no-cache");
    if (server.hasHeader("If-None-Match") && server.header("If-None-Match") == etag) {
      server.send(304, "text/plain", "");
      return;
    }
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.on("/app-logo.png", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/png", reinterpret_cast<const char*>(APP_LOGO_PNG), APP_LOGO_PNG_LEN);
  });

  server.on("/manifest.webmanifest", HTTP_GET, []() {
    const String manifest =
      "{\"name\":\"Prism\",\"short_name\":\"Prism\",\"description\":\"RGB Light Controller\","
      "\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#0b1018\","
      "\"theme_color\":\"#0b1018\",\"icons\":[{\"src\":\"/app-logo.png\","
      "\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]}";
    server.send(200, "application/manifest+json", manifest);
  });

  server.on("/api/state", HTTP_GET, []() {
    server.send(200, "application/json", buildStateJson());
  });

  server.on("/api/color", HTTP_POST, []() {
    setStaticColor(
      constrain(argInt("r", state.r), 0, 255),
      constrain(argInt("g", state.g), 0, 255),
      constrain(argInt("b", state.b), 0, 255)
    );
    sendOk();
  });

  server.on("/api/power", HTTP_POST, []() {
    state.power = argBool("value", state.power);
    if (state.power && state.effect == Effect::Off) state.effect = Effect::Static;
    markStateChanged();
    sendOk();
  });

  server.on("/api/brightness", HTTP_POST, []() {
    setBrightness(argInt("value", state.brightness));
    sendOk();
  });

  server.on("/api/effect", HTTP_POST, []() {
    // An unknown or missing name used to fall back to Static, so a malformed
    // request could switch the light out of the running effect.
    if (!parseEffect(server.arg("name"), state.effect)) {
      server.send(400, "text/plain", "Unknown effect");
      return;
    }
    state.speed = clampPercent(argInt("speed", state.speed));
    state.intensity = clampPercent(argInt("intensity", state.intensity));
    state.power = state.effect != Effect::Off;
    markStateChanged();
    sendOk();
  });

  server.on("/api/settings", HTTP_POST, []() {
    // Every field is optional and every value is validated here. Anything
    // missing or unrecognised leaves the current setting untouched rather than
    // resetting it, so a partial or malformed request cannot wipe the config.
    if (server.hasArg("deviceName")) {
      settings.deviceName = sanitizeDeviceName(server.arg("deviceName"));
    }
    if (server.hasArg("mdnsName")) {
      settings.mdnsName = sanitizeHostname(server.arg("mdnsName"));
    }

    settings.ledCount = constrain(argInt("ledCount", settings.ledCount), 1, MAX_LED_COUNT);
    settings.maxBrightness = constrain(argInt("maxBrightness", settings.maxBrightness), 1, 100);

    parseColorOrder(server.arg("colorOrder"), settings.colorOrder);
    parseLedChipset(server.arg("ledChipset"), settings.chipset);

    settings.restoreState = argBool("restoreState", settings.restoreState);
    settings.smoothTransitions = argBool("smoothTransitions", settings.smoothTransitions);
    settings.warmCompensation = argBool("warmCompensation", settings.warmCompensation);
    settings.defaultFade = constrain(argInt("defaultFade", settings.defaultFade), 0, 5000);

    settings.input1Enabled = argBool("input1Enabled", settings.input1Enabled);
    settings.input2Enabled = argBool("input2Enabled", settings.input2Enabled);
    parseInputAction(server.arg("input1Action"), settings.input1Action);
    parseInputAction(server.arg("input2Action"), settings.input2Action);

    configureInputs();
    markStateChanged();
    savePreferences();
    sendOk();
  });

  server.on("/api/restart", HTTP_POST, []() {
    sendOk();
    // A state change made less than SETTINGS_SAVE_DELAY_MS before the restart
    // was still pending and used to be lost.
    if (stateDirty) savePreferences();
    delay(250);
    ESP.restart();
  });

  server.on("/api/reset-wifi", HTTP_POST, []() {
    sendOk();
    delay(250);
    WiFiManager manager;
    manager.resetSettings();
    ESP.restart();
  });

  server.on("/api/factory-reset", HTTP_POST, []() {
    sendOk();
    delay(250);
    prefs.begin("marine-rgb", false);
    prefs.clear();
    prefs.end();
    WiFiManager manager;
    manager.resetSettings();
    ESP.restart();
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
}

static const char PRISM_WIFI_PORTAL_HEAD[] = R"PRISMSETUP(
<meta name="theme-color" content="#111521">
<style>
:root{
  color-scheme:dark;
  --bg:#090b10;
  --panel:#121721;
  --line:rgba(255,255,255,.09);
  --text:#f5f7fa;
  --muted:#929cac;
  --soft:#667182;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html{
  min-height:100%;
  background-color:#111521!important;
  background-image:
    radial-gradient(ellipse 105% 42% at 50% -8%,rgba(91,101,148,.145),transparent 72%),
    radial-gradient(circle at 10% 5%,rgba(220,45,160,.080),transparent 30%),
    radial-gradient(circle at 48% 2%,rgba(255,135,38,.064),transparent 29%),
    radial-gradient(circle at 91% 6%,rgba(52,220,90,.064),transparent 30%),
    radial-gradient(circle at 94% 48%,rgba(0,205,220,.058),transparent 31%),
    radial-gradient(circle at 48% 76%,rgba(35,92,255,.060),transparent 35%),
    radial-gradient(circle at 4% 52%,rgba(126,48,225,.070),transparent 31%),
    linear-gradient(180deg,#111521 0%,#0b1018 30%,#0b1018 100%)!important;
  background-repeat:no-repeat!important;
  background-size:100vw 100vh!important;
  background-attachment:fixed!important
}
body{
  margin:0!important;
  min-height:100vh!important;
  color:var(--text)!important;
  background:transparent!important;
  font-family:Inter,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif!important
}
.wrap{
  width:min(calc(100% - 32px),420px)!important;
  max-width:420px!important;
  margin:0 auto!important;
  padding:34px 0 40px!important
}
h1{
  margin:0 0 18px!important;
  color:var(--text)!important;
  font-size:26px!important;
  font-weight:750!important;
  letter-spacing:-.035em!important
}
h2,h3,h4{color:var(--text)!important}
p,label,small{color:var(--muted)!important}
a{color:#d6dcff!important}
form,.msg{
  border:1px solid var(--line)!important;
  border-radius:24px!important;
  background:linear-gradient(145deg,rgba(255,255,255,.035),rgba(255,255,255,.01)),rgba(16,21,30,.86)!important;
  box-shadow:0 26px 80px rgba(0,0,0,.28)!important;
  padding:18px!important
}
.q{
  border:1px solid var(--line)!important;
  border-radius:16px!important;
  background:rgba(18,23,33,.88)!important;
  margin:8px 0!important;
  padding:12px 14px!important
}
input,select{
  width:100%!important;
  min-height:48px!important;
  padding:0 14px!important;
  border:1px solid var(--line)!important;
  border-radius:16px!important;
  outline:none!important;
  color:var(--text)!important;
  background:#121721!important;
  font:inherit!important
}
input:focus,select:focus{
  border-color:rgba(255,255,255,.22)!important;
  box-shadow:0 0 0 3px rgba(255,255,255,.055)!important
}
button,.button,input[type=submit]{
  width:100%!important;
  min-height:48px!important;
  margin:8px 0!important;
  border:1px solid var(--line)!important;
  border-radius:16px!important;
  background:#f6f7f9!important;
  color:#10141b!important;
  font:inherit!important;
  font-weight:750!important;
  box-shadow:none!important
}
button:hover,.button:hover,input[type=submit]:hover{background:#fff!important}
hr{border:0!important;border-top:1px solid var(--line)!important}
@media(max-width:480px){
  .wrap{width:min(calc(100% - 28px),420px)!important;padding-top:24px!important}
  h1{font-size:24px!important}
}
</style>
)PRISMSETUP";

void configureWiFiManagerPortal(WiFiManager& manager) {
  manager.setTitle("Prism Wi-Fi Setup");
  manager.setCustomHeadElement(PRISM_WIFI_PORTAL_HEAD);
  manager.setShowInfoUpdate(false);
  manager.setShowInfoErase(false);
  manager.setRemoveDuplicateAPs(true);
  manager.setMinimumSignalQuality(8);
  manager.setWiFiAutoReconnect(true);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(settings.mdnsName.c_str());

  WiFiManager manager;
  configureWiFiManagerPortal(manager);
  manager.setConfigPortalTimeout(WIFI_SETUP_TIMEOUT_SECONDS);
  manager.setConnectTimeout(20);
  // Do not break out of the captive portal merely because credentials were
  // submitted. On first setup that can let Prism continue into mDNS/WebServer
  // startup before the station actually owns an IP address.
  manager.setBreakAfterConfig(false);

  if (!manager.autoConnect(WIFI_SETUP_AP_NAME)) {
    delay(500);
    ESP.restart();
  }

  const uint32_t readyStartedAt = millis();
  while ((WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) &&
         millis() - readyStartedAt < 5000) {
    delay(25);
  }
  if (WiFi.status() != WL_CONNECTED) {
    delay(250);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  loadPreferences();
  configureInputs();
  configureLedController();
  connectWiFi();

  if (MDNS.begin(settings.mdnsName.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }

  setupRoutes();
  httpUpdater.setup(&server, "/update", "admin", "marine-rgb");
  server.begin();

  Serial.println();
  Serial.println("Prism RGB Light Controller");
  Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
  Serial.printf("LEDs: %u x %s (%s)\n", static_cast<unsigned>(activeLedCount),
                ledChipsetName(settings.chipset), colorOrderName(settings.colorOrder));
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("mDNS: http://%s.local\n", settings.mdnsName.c_str());
}

void loop() {
  server.handleClient();
  updateLeds();
  updateInput(input1, settings.input1Action);
  updateInput(input2, settings.input2Action);

  if (stateDirty && millis() - stateDirtyAt >= SETTINGS_SAVE_DELAY_MS) {
    savePreferences();
  }

  delay(1);
}
