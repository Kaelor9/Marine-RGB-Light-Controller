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
  String colorOrder = "BRG";
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
uint32_t stateDirtyAt = 0;
uint32_t stateVersion = 1;

CRGB displayedColor = CRGB::Black;
CRGB transitionFrom = CRGB::Black;
CRGB transitionTo = CRGB::Black;
uint32_t transitionStartedAt = 0;
uint16_t transitionDuration = 0;
bool transitionActive = false;

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

Effect parseEffect(const String& value) {
  if (value == "rainbow") return Effect::Rainbow;
  if (value == "fade") return Effect::ColorFade;
  if (value == "disco") return Effect::Disco;
  if (value == "sparkle") return Effect::Sparkle;
  if (value == "off") return Effect::Off;
  return Effect::Static;
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

InputAction parseInputAction(const String& value) {
  if (value == "power") return InputAction::TogglePower;
  if (value == "effects") return InputAction::NextEffect;
  if (value == "previous-effect") return InputAction::PreviousEffect;
  if (value == "brightness-up") return InputAction::BrightnessUp;
  if (value == "brightness-down") return InputAction::BrightnessDown;
  if (value == "warm-white") return InputAction::WarmWhite;
  if (value == "red") return InputAction::RedScene;
  if (value == "green") return InputAction::GreenScene;
  if (value == "blue") return InputAction::BlueScene;
  return InputAction::CycleColors;
}

void markStateChanged() {
  ++stateVersion;
  stateDirty = true;
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

  settings.deviceName = prefs.getString("device", DEFAULT_DEVICE_NAME);
  settings.mdnsName = prefs.getString("mdns", DEFAULT_MDNS_NAME);
  settings.ledCount = constrain(prefs.getUShort("leds", DEFAULT_LED_COUNT), 1, MAX_LED_COUNT);
  settings.maxBrightness = constrain(prefs.getUChar("maxb", 100), 1, 100);
  settings.colorOrder = prefs.getString("order", "BRG");
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
  prefs.putString("order", settings.colorOrder);
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

void configureLedController() {
  activeLedCount = settings.ledCount;

  if (settings.colorOrder == "RGB") FastLED.addLeds<WS2811, LED_DATA_PIN_1, RGB>(leds, activeLedCount);
  else if (settings.colorOrder == "GRB") FastLED.addLeds<WS2811, LED_DATA_PIN_1, GRB>(leds, activeLedCount);
  else FastLED.addLeds<WS2811, LED_DATA_PIN_1, BRG>(leds, activeLedCount);

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

  // Only compensate the calibrated warm-white area. Applying this to every
  // static colour would shift colours selected from the hue wheel.
  const bool warmWhiteSelected =
    state.r >= 235 &&
    state.g >= 100 && state.g <= 156 &&
    state.b >= 10 && state.b <= 70;

  if (!warmWhiteSelected) return color;

  const uint8_t strength = map(state.brightness, 1, 100, 0, 42);
  color.g = qsub8(color.g, scale8(color.g, strength));
  color.b = qsub8(color.b, scale8(color.b, min<uint8_t>(70, strength + 18)));
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

void updateLeds() {
  const uint32_t now = millis();
  if (now - lastEffectFrame < effectFrameInterval()) return;
  lastEffectFrame = now;

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

  const uint8_t limitedPercent = static_cast<uint8_t>(
    (static_cast<uint16_t>(state.brightness) * settings.maxBrightness) / 100
  );
  FastLED.setBrightness(map(limitedPercent, 0, 100, 0, 255));
  FastLED.show();
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
  int current = static_cast<int>(state.effect);
  constexpr int effectCount = 5; // Excludes Off from normal cycling.
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
    input.initialized = true;
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
      if (!input.longPressActive) executeShortAction(action);
    }
  }

  if (input.stable == ISO_INPUT_ACTIVE_LEVEL) {
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
  json += ",\"colorOrder\":\"" + settings.colorOrder + "\"";
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

int argInt(const char* name, int fallback) {
  return server.hasArg(name) ? server.arg(name).toInt() : fallback;
}

void setupRoutes() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.on("/app-logo.png", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/png", reinterpret_cast<const char*>(APP_LOGO_PNG), APP_LOGO_PNG_LEN);
  });

  server.on("/manifest.webmanifest", HTTP_GET, []() {
    const String manifest =
      "{\"name\":\"Prism\",\"short_name\":\"Prism\",\"description\":\"RGB Light Controller\","
      "\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#090b10\","
      "\"theme_color\":\"#090b10\",\"icons\":[{\"src\":\"/app-logo.png\","
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
    state.power = argInt("value", state.power ? 1 : 0) != 0;
    if (state.power && state.effect == Effect::Off) state.effect = Effect::Static;
    markStateChanged();
    sendOk();
  });

  server.on("/api/brightness", HTTP_POST, []() {
    setBrightness(argInt("value", state.brightness));
    sendOk();
  });

  server.on("/api/effect", HTTP_POST, []() {
    state.effect = parseEffect(server.arg("name"));
    state.speed = clampPercent(argInt("speed", state.speed));
    state.intensity = clampPercent(argInt("intensity", state.intensity));
    state.power = state.effect != Effect::Off;
    markStateChanged();
    sendOk();
  });

  server.on("/api/settings", HTTP_POST, []() {
    String device = server.arg("deviceName");
    device.trim();
    if (device.length() > 0) settings.deviceName = device.substring(0, 31);

    String mdns = server.arg("mdnsName");
    mdns.toLowerCase();
    mdns.replace(" ", "-");
    if (mdns.length() > 0) settings.mdnsName = mdns.substring(0, 31);

    settings.ledCount = constrain(argInt("ledCount", settings.ledCount), 1, MAX_LED_COUNT);
    settings.maxBrightness = constrain(argInt("maxBrightness", settings.maxBrightness), 1, 100);

    const String order = server.arg("colorOrder");
    if (order == "RGB" || order == "GRB" || order == "BRG") settings.colorOrder = order;

    settings.restoreState = argInt("restoreState", settings.restoreState) != 0;
    settings.smoothTransitions = argInt("smoothTransitions", settings.smoothTransitions) != 0;
    settings.warmCompensation = argInt("warmCompensation", settings.warmCompensation) != 0;
    settings.defaultFade = constrain(argInt("defaultFade", settings.defaultFade), 0, 5000);

    settings.input1Enabled = argInt("input1Enabled", settings.input1Enabled) != 0;
    settings.input2Enabled = argInt("input2Enabled", settings.input2Enabled) != 0;
    settings.input1Action = parseInputAction(server.arg("input1Action"));
    settings.input2Action = parseInputAction(server.arg("input2Action"));

    configureInputs();
    markStateChanged();
    savePreferences();
    sendOk();
  });

  server.on("/api/restart", HTTP_POST, []() {
    sendOk();
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

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(settings.mdnsName.c_str());

  WiFiManager manager;
  manager.setConfigPortalTimeout(WIFI_SETUP_TIMEOUT_SECONDS);
  manager.setConnectTimeout(20);
  manager.setBreakAfterConfig(true);

  if (!manager.autoConnect(WIFI_SETUP_AP_NAME)) {
    delay(500);
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
