#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <FastLED.h>
#include <WiFiManager.h>
#include <HTTPUpdateServer.h>

#include "Config.h"
#include "WebUi.h"

// ============================================================
// Types
// ============================================================

enum class Effect : uint8_t {
  Static,
  Rainbow,
  ColorFade,
  Disco,
  Sparkle,
  Off
};

enum class InputAction : uint8_t {
  CycleColors,
  TogglePower,
  NextEffect
};

struct RuntimeState {
  uint8_t r = 255;
  uint8_t g = 59;
  uint8_t b = 48;
  uint8_t brightness = 70;
  uint8_t speed = 50;
  uint8_t intensity = 65;
  uint16_t fadeMs = 800;
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
  uint16_t defaultFade = 800;
  InputAction input1Action = InputAction::CycleColors;
  InputAction input2Action = InputAction::NextEffect;
};

// ============================================================
// Globals
// ============================================================

CRGB leds[MAX_LED_COUNT];
WebServer server(80);
HTTPUpdateServer httpUpdater;
Preferences prefs;
RuntimeState state;
Settings settings;

uint32_t stateVersion = 1;
bool settingsDirty = false;
uint32_t settingsDirtyAt = 0;

struct DebouncedInput {
  int8_t pin = -1;
  uint8_t lastRaw = LOW;
  uint8_t stable = LOW;
  uint32_t changedAt = 0;
  bool initialized = false;
};

DebouncedInput input1{static_cast<int8_t>(ISO_INPUT_1_PIN)};
DebouncedInput input2{ISO_INPUT_2_PIN};

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
  CRGB(255, 180, 0),
  CRGB(170, 0, 255),
  CRGB(0, 220, 190),
  CRGB::Black
};
constexpr size_t PRESET_COLOR_COUNT = sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0]);
size_t presetIndex = 0;

// ============================================================
// Utility
// ============================================================

String jsonEscape(const String& value) {
  String result;
  result.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c; break;
    }
  }
  return result;
}

uint8_t clampPercent(int value) {
  return static_cast<uint8_t>(constrain(value, 1, 100));
}

uint8_t effectiveBrightness() {
  if (!state.power || state.effect == Effect::Off) return 0;
  const uint16_t scaled = static_cast<uint16_t>(state.brightness) * settings.maxBrightness / 100;
  return static_cast<uint8_t>(constrain(scaled, 1, 255));
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
  }
  return "colors";
}

InputAction parseInputAction(const String& value) {
  if (value == "power") return InputAction::TogglePower;
  if (value == "effects") return InputAction::NextEffect;
  return InputAction::CycleColors;
}

void markStateChanged() {
  ++stateVersion;
  settingsDirty = true;
  settingsDirtyAt = millis();
}

void startStaticTransition(const CRGB& target, uint16_t durationMs) {
  transitionFrom = displayedColor;
  transitionTo = target;
  transitionStartedAt = millis();
  transitionDuration = durationMs;
  transitionActive = durationMs > 0 && settings.smoothTransitions;
  if (!transitionActive) displayedColor = target;
}

void setStaticColor(uint8_t r, uint8_t g, uint8_t b, uint16_t requestedFade) {
  state.r = r;
  state.g = g;
  state.b = b;
  state.effect = Effect::Static;
  state.power = true;
  startStaticTransition(CRGB(r, g, b), requestedFade);
  markStateChanged();
}

// ============================================================
// Preferences
// ============================================================

void loadPreferences() {
  prefs.begin("marine-rgb", true);

  settings.deviceName = prefs.getString("device", DEFAULT_DEVICE_NAME);
  settings.mdnsName = prefs.getString("mdns", DEFAULT_MDNS_NAME);
  settings.ledCount = constrain(prefs.getUShort("leds", DEFAULT_LED_COUNT), 1, MAX_LED_COUNT);
  settings.maxBrightness = constrain(prefs.getUChar("maxb", 100), 1, 100);
  settings.colorOrder = prefs.getString("order", "BRG");
  settings.restoreState = prefs.getBool("restore", true);
  settings.smoothTransitions = prefs.getBool("smooth", true);
  settings.defaultFade = constrain(prefs.getUShort("fade", 800), 0, 5000);
  settings.input1Action = static_cast<InputAction>(prefs.getUChar("in1", static_cast<uint8_t>(InputAction::CycleColors)));
  settings.input2Action = static_cast<InputAction>(prefs.getUChar("in2", static_cast<uint8_t>(InputAction::NextEffect)));

  if (settings.restoreState) {
    state.r = prefs.getUChar("r", 255);
    state.g = prefs.getUChar("g", 59);
    state.b = prefs.getUChar("b", 48);
    state.brightness = constrain(prefs.getUChar("bright", 70), 1, 100);
    state.speed = constrain(prefs.getUChar("speed", 50), 1, 100);
    state.intensity = constrain(prefs.getUChar("intens", 65), 1, 100);
    state.fadeMs = constrain(prefs.getUShort("statefade", settings.defaultFade), 0, 5000);
    state.power = prefs.getBool("power", true);
    state.effect = static_cast<Effect>(prefs.getUChar("effect", static_cast<uint8_t>(Effect::Static)));
  } else {
    state.fadeMs = settings.defaultFade;
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
  prefs.putUShort("fade", settings.defaultFade);
  prefs.putUChar("in1", static_cast<uint8_t>(settings.input1Action));
  prefs.putUChar("in2", static_cast<uint8_t>(settings.input2Action));

  if (settings.restoreState) {
    prefs.putUChar("r", state.r);
    prefs.putUChar("g", state.g);
    prefs.putUChar("b", state.b);
    prefs.putUChar("bright", state.brightness);
    prefs.putUChar("speed", state.speed);
    prefs.putUChar("intens", state.intensity);
    prefs.putUShort("statefade", state.fadeMs);
    prefs.putBool("power", state.power);
    prefs.putUChar("effect", static_cast<uint8_t>(state.effect));
  }

  prefs.end();
  settingsDirty = false;
}

// ============================================================
// FastLED
// ============================================================

void configureLedController() {
  if (settings.colorOrder == "RGB") {
    FastLED.addLeds<WS2811, LED_DATA_PIN_1, RGB>(leds, MAX_LED_COUNT);
  } else if (settings.colorOrder == "GRB") {
    FastLED.addLeds<WS2811, LED_DATA_PIN_1, GRB>(leds, MAX_LED_COUNT);
  } else {
    FastLED.addLeds<WS2811, LED_DATA_PIN_1, BRG>(leds, MAX_LED_COUNT);
  }

  FastLED.setDither(true);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.clear(true);
  displayedColor = CRGB(state.r, state.g, state.b);
}

uint16_t effectFrameInterval() {
  // 1% -> about 80 ms, 100% -> about 10 ms
  return static_cast<uint16_t>(map(state.speed, 1, 100, 80, 10));
}

void renderStatic() {
  if (transitionActive) {
    const uint32_t elapsed = millis() - transitionStartedAt;
    if (elapsed >= transitionDuration) {
      displayedColor = transitionTo;
      transitionActive = false;
    } else {
      const uint8_t amount = static_cast<uint8_t>((elapsed * 255UL) / transitionDuration);
      displayedColor = blend(transitionFrom, transitionTo, amount);
    }
  } else {
    displayedColor = CRGB(state.r, state.g, state.b);
  }

  fill_solid(leds, settings.ledCount, displayedColor);
}

void renderRainbow() {
  fill_rainbow(leds, settings.ledCount, rainbowOffset, 255 / max<uint16_t>(settings.ledCount, 1));
  rainbowOffset += map(state.speed, 1, 100, 1, 5);
}

void renderColorFade() {
  CRGB color = CHSV(fadeHue, 255, 255);
  fill_solid(leds, settings.ledCount, color);
  fadeHue += map(state.speed, 1, 100, 1, 4);
}

void renderDisco() {
  const uint32_t interval = map(state.speed, 1, 100, 650, 90);
  if (millis() - lastDiscoChange >= interval) {
    lastDiscoChange = millis();
    const uint8_t hue = random8();
    discoColor = CHSV(hue, 255, 255);
  }
  fill_solid(leds, settings.ledCount, discoColor);
}

void renderSparkle() {
  CRGB base(state.r, state.g, state.b);
  base.nscale8_video(map(state.intensity, 1, 100, 35, 145));
  fill_solid(leds, settings.ledCount, base);

  const uint8_t sparkles = map(state.intensity, 1, 100, 1, max<uint16_t>(2, settings.ledCount / 5));
  for (uint8_t i = 0; i < sparkles; ++i) {
    if (random8() < map(state.speed, 1, 100, 35, 180)) {
      leds[random16(settings.ledCount)] = blend(CRGB(state.r, state.g, state.b), CRGB::White, 190);
    }
  }
}

void updateLeds() {
  const uint32_t now = millis();
  if (now - lastEffectFrame < effectFrameInterval()) return;
  lastEffectFrame = now;

  if (!state.power || state.effect == Effect::Off) {
    fill_solid(leds, settings.ledCount, CRGB::Black);
  } else {
    switch (state.effect) {
      case Effect::Static: renderStatic(); break;
      case Effect::Rainbow: renderRainbow(); break;
      case Effect::ColorFade: renderColorFade(); break;
      case Effect::Disco: renderDisco(); break;
      case Effect::Sparkle: renderSparkle(); break;
      case Effect::Off: fill_solid(leds, settings.ledCount, CRGB::Black); break;
    }
  }

  FastLED.setBrightness(map(effectiveBrightness(), 0, 100, 0, 255));
  FastLED.show();
}

// ============================================================
// Inputs
// ============================================================

void cyclePresetColors() {
  const CRGB color = PRESET_COLORS[presetIndex];
  presetIndex = (presetIndex + 1) % PRESET_COLOR_COUNT;

  if (color == CRGB::Black) {
    state.power = false;
    state.effect = Effect::Off;
    markStateChanged();
  } else {
    setStaticColor(color.r, color.g, color.b, state.fadeMs);
  }
}

void nextEffect() {
  switch (state.effect) {
    case Effect::Static: state.effect = Effect::Rainbow; break;
    case Effect::Rainbow: state.effect = Effect::ColorFade; break;
    case Effect::ColorFade: state.effect = Effect::Disco; break;
    case Effect::Disco: state.effect = Effect::Sparkle; break;
    case Effect::Sparkle:
    case Effect::Off: state.effect = Effect::Static; break;
  }
  state.power = true;
  markStateChanged();
}

void executeInputAction(InputAction action) {
  switch (action) {
    case InputAction::CycleColors: cyclePresetColors(); break;
    case InputAction::TogglePower:
      state.power = !state.power;
      if (state.power && state.effect == Effect::Off) state.effect = Effect::Static;
      markStateChanged();
      break;
    case InputAction::NextEffect: nextEffect(); break;
  }
}

void updateInput(DebouncedInput& input, InputAction action) {
  if (input.pin < 0) return;

  const uint8_t raw = digitalRead(input.pin);
  if (!input.initialized) {
    input.lastRaw = raw;
    input.stable = raw;
    input.changedAt = millis();
    input.initialized = true;
    return;
  }

  if (raw != input.lastRaw) {
    input.lastRaw = raw;
    input.changedAt = millis();
  }

  if (raw != input.stable && millis() - input.changedAt >= INPUT_DEBOUNCE_MS) {
    input.stable = raw;
    if (input.stable == ISO_INPUT_ACTIVE_LEVEL) {
      executeInputAction(action);
    }
  }
}

// ============================================================
// JSON and API
// ============================================================

String buildStateJson() {
  String json;
  json.reserve(900);
  json += "{\"version\":";
  json += stateVersion;
  json += ",\"state\":{";
  json += "\"r\":" + String(state.r);
  json += ",\"g\":" + String(state.g);
  json += ",\"b\":" + String(state.b);
  json += ",\"brightness\":" + String(state.brightness);
  json += ",\"speed\":" + String(state.speed);
  json += ",\"intensity\":" + String(state.intensity);
  json += ",\"fade\":" + String(state.fadeMs);
  json += ",\"power\":";
  json += state.power ? "true" : "false";
  json += ",\"effect\":\"";
  json += effectName(state.effect);
  json += "\"},\"settings\":{";
  json += "\"deviceName\":\"" + jsonEscape(settings.deviceName) + "\"";
  json += ",\"mdnsName\":\"" + jsonEscape(settings.mdnsName) + "\"";
  json += ",\"ledCount\":" + String(settings.ledCount);
  json += ",\"maxBrightness\":" + String(settings.maxBrightness);
  json += ",\"colorOrder\":\"" + settings.colorOrder + "\"";
  json += ",\"restoreState\":";
  json += settings.restoreState ? "true" : "false";
  json += ",\"smoothTransitions\":";
  json += settings.smoothTransitions ? "true" : "false";
  json += ",\"defaultFade\":" + String(settings.defaultFade);
  json += ",\"input1Action\":\"" + String(inputActionName(settings.input1Action)) + "\"";
  json += ",\"input2Action\":\"" + String(inputActionName(settings.input2Action)) + "\"";
  json += "},\"network\":{";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += "},\"firmware\":\"" + String(FIRMWARE_VERSION) + "\"}";
  return json;
}

void sendOk() {
  server.send(200, "text/plain", "OK");
}

int argInt(const char* name, int fallback) {
  return server.hasArg(name) ? server.arg(name).toInt() : fallback;
}

void setupApiRoutes() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.on("/api/state", HTTP_GET, []() {
    server.send(200, "application/json", buildStateJson());
  });

  server.on("/api/color", HTTP_POST, []() {
    const uint8_t r = constrain(argInt("r", state.r), 0, 255);
    const uint8_t g = constrain(argInt("g", state.g), 0, 255);
    const uint8_t b = constrain(argInt("b", state.b), 0, 255);
    const uint16_t fade = constrain(argInt("fade", state.fadeMs), 0, 5000);
    state.fadeMs = fade;
    setStaticColor(r, g, b, fade);
    sendOk();
  });

  server.on("/api/power", HTTP_POST, []() {
    state.power = argInt("value", state.power ? 1 : 0) != 0;
    if (state.power && state.effect == Effect::Off) state.effect = Effect::Static;
    markStateChanged();
    sendOk();
  });

  server.on("/api/brightness", HTTP_POST, []() {
    state.brightness = clampPercent(argInt("value", state.brightness));
    markStateChanged();
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
    settings.defaultFade = constrain(argInt("defaultFade", settings.defaultFade), 0, 5000);
    settings.input1Action = parseInputAction(server.arg("input1Action"));
    settings.input2Action = parseInputAction(server.arg("input2Action"));

    state.fadeMs = settings.defaultFade;
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

// ============================================================
// Setup
// ============================================================

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

  if (ISO_INPUT_1_PIN >= 0) pinMode(ISO_INPUT_1_PIN, INPUT);
  if (ISO_INPUT_2_PIN >= 0) pinMode(ISO_INPUT_2_PIN, INPUT);

  configureLedController();
  connectWiFi();

  if (MDNS.begin(settings.mdnsName.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }

  setupApiRoutes();
  httpUpdater.setup(&server, "/update", "admin", "marine-rgb");
  server.begin();

  Serial.println();
  Serial.println("Marine RGB Light Controller");
  Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("mDNS: http://%s.local\n", settings.mdnsName.c_str());
}

void loop() {
  server.handleClient();
  updateLeds();
  updateInput(input1, settings.input1Action);
  updateInput(input2, settings.input2Action);

  if (settingsDirty && millis() - settingsDirtyAt >= SETTINGS_SAVE_DELAY_MS) {
    savePreferences();
  }

  delay(1);
}
