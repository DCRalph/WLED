#include "wled.h"

#include "WipeRevealStyles.h"
#include "WipeRevealTypes.h"

/*
 * Wipe Reveal usermod
 *
 * Applies a temporary brightness mask after the active effects have rendered,
 * so the reveal works with the current WLED effect instead of replacing it.
 */
class WipeRevealUsermod : public Usermod {
  private:
    static const char _name[];
    static const char _enabled[];
    static const char _trigger[];
    static const char _position[];
    static const char _style[];
    static const char _styleOn[];
    static const char _styleOff[];
    static const char _easing[];
    static const char _duration[];
    static const char _durationOn[];
    static const char _durationOff[];
    static const char _dropoff[];
    static const char _turnOn[];
    static const char _alwaysAnimate[];
    static const char _running[];
    static const char _progress[];

    bool enabled = true;
    bool turnOn = true;
    bool suppressPowerChange = false;
    byte lastBri = 0;
    bool initDone = false;
    WipeReveal::RevealConfig config;
    WipeReveal::RevealAnimationState animation;

    WipeReveal::RevealOptions onOptions() const {
      WipeReveal::RevealOptions options = config.options;
      options.style = config.styleOn;
      options.durationMs = config.durationOnMs;
      return options;
    }

    WipeReveal::RevealOptions offOptions() const {
      WipeReveal::RevealOptions options = config.options;
      options.style = config.styleOff;
      options.durationMs = config.durationOffMs;
      return options;
    }

    void applyConfiguredStyles(JsonObjectConst obj, uint8_t& styleOn, uint8_t& styleOff) {
      JsonVariantConst styleOnValue = obj[FPSTR(_styleOn)];
      if (!styleOnValue.isNull()) {
        styleOn = WipeReveal::parseRevealStyleJson(styleOnValue, styleOn);
      }

      JsonVariantConst styleOffValue = obj[FPSTR(_styleOff)];
      if (!styleOffValue.isNull()) {
        styleOff = WipeReveal::parseRevealStyleJson(styleOffValue, styleOff);
      }
    }

    void applyOverrides(JsonObjectConst obj, WipeReveal::RevealOptions& options, bool& always) {
      JsonVariantConst positionValue = obj[FPSTR(_position)];
      if (!positionValue.isNull()) {
        options.position = WipeReveal::parseRevealPositionJson(positionValue, options.position);
      }

      JsonVariantConst styleValue = obj[FPSTR(_style)];
      if (!styleValue.isNull()) {
        options.style = WipeReveal::parseRevealStyleJson(styleValue, options.style);
      }

      JsonVariantConst easingValue = obj[FPSTR(_easing)];
      if (!easingValue.isNull()) {
        options.easing = WipeReveal::parseRevealEasingJson(easingValue, options.easing);
      }

      if (obj[FPSTR(_alwaysAnimate)].is<bool>()) {
        always = obj[FPSTR(_alwaysAnimate)].as<bool>();
      }

      options.durationMs = constrain(obj[FPSTR(_duration)] | options.durationMs, 1, 60000);
      options.dropoffPx = constrain(obj[FPSTR(_dropoff)] | options.dropoffPx, 0, 255);
    }

    void startReveal(const WipeReveal::RevealOptions& options, bool reverse) {
      if (!enabled) return;

      animation.options.position = constrain(options.position, WipeReveal::RevealStart, WipeReveal::RevealEnd);
      animation.options.style = WipeReveal::revealStyleForId(options.style).id;
      animation.options.easing = constrain(options.easing, WipeReveal::EaseLinear, WipeReveal::EaseInOut);
      animation.options.durationMs = options.durationMs ? options.durationMs : 1;
      animation.options.dropoffPx = options.dropoffPx;
      animation.startTime = millis();
      animation.lastTriggerFrame = 0;
      animation.running = true;
      animation.reversing = reverse;

      if (turnOn && bri == 0 && !animation.reversing) {
        bri = briLast;
        colorUpdated(CALL_MODE_DIRECT_CHANGE);
      } else {
        strip.trigger();
      }
      stateChanged = true;
    }

    void completeOffReveal() {
      animation.running = false;
      animation.reversing = false;
      suppressPowerChange = true;
      bri = 0;
      offMode = true;
      applyFinalBri();
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
      suppressPowerChange = false;
      lastBri = 0;
    }

    uint8_t progressPercent() const {
      if (!animation.running) return 0;
      const unsigned long elapsed = millis() - animation.startTime;
      if (elapsed >= animation.options.durationMs) return 100;
      return (elapsed * 100UL) / animation.options.durationMs;
    }

  public:
    void setup() override {
      initDone = true;
      lastBri = bri;
    }

    void loop() override {
      if (!animation.running) return;

      const unsigned long now = millis();
      if (now - animation.startTime >= animation.options.durationMs) {
        animation.running = false;
        if (animation.reversing) {
          completeOffReveal();
        }
        strip.trigger();
        stateChanged = true;
        return;
      }

      if (now - animation.lastTriggerFrame >= 10) {
        animation.lastTriggerFrame = now;
        strip.trigger();
      }
    }

    void handleOverlayDraw() override {
      if (!enabled || !animation.running) return;

      const unsigned long elapsed = millis() - animation.startTime;
      if (elapsed >= animation.options.durationMs) {
        animation.running = false;
        if (animation.reversing) {
          completeOffReveal();
        }
        return;
      }

      const WipeReveal::RevealStyleDescriptor& style = WipeReveal::revealStyleForId(animation.options.style);
      const uint16_t linearProgress = (elapsed * 0xFFFFUL) / animation.options.durationMs;
      const uint16_t progress = animation.reversing ? (0xFFFFU - linearProgress) : linearProgress;
      const unsigned totalLen = strip.getLengthTotal();

      for (unsigned segmentIndex = 0; segmentIndex < strip.getSegmentsNum(); segmentIndex++) {
        const Segment& seg = strip.getSegment(segmentIndex);
        if (!seg.isActive()) continue;

        const uint16_t len = seg.length();
        if (len == 0) continue;

        for (uint16_t pixel = 0; pixel < len; pixel++) {
          const unsigned absolutePixel = seg.start + pixel;
          if (absolutePixel >= totalLen) break;

          const uint8_t scale = style.renderScale(pixel, len, progress, animation.options);
          if (scale >= 255) continue;

          strip.setPixelColor(absolutePixel, color_fade(strip.getPixelColor(absolutePixel), scale, true));
        }
      }
    }

    void addToJsonState(JsonObject& root) override {
      if (!initDone || !enabled) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      WipeReveal::RevealOptions currentOptions = animation.running ? animation.options : onOptions();
      usermod[FPSTR(_running)] = animation.running;
      usermod[FPSTR(_position)] = WipeReveal::revealPositionName(currentOptions.position);
      usermod[FPSTR(_style)] = WipeReveal::revealStyleName(currentOptions.style);
      usermod[FPSTR(_styleOn)] = WipeReveal::revealStyleName(config.styleOn);
      usermod[FPSTR(_styleOff)] = WipeReveal::revealStyleName(config.styleOff);
      usermod[FPSTR(_easing)] = WipeReveal::revealEasingName(currentOptions.easing);
      usermod[FPSTR(_duration)] = config.options.durationMs;
      usermod[FPSTR(_durationOn)] = config.durationOnMs;
      usermod[FPSTR(_durationOff)] = config.durationOffMs;
      usermod[FPSTR(_alwaysAnimate)] = config.alwaysAnimate;
      usermod[FPSTR(_progress)] = progressPercent();
    }

    void readFromJsonState(JsonObject& root) override {
      if (!initDone) return;

      JsonObjectConst usermod = root[FPSTR(_name)];
      if (usermod.isNull()) return;

      if (usermod[FPSTR(_enabled)].is<bool>()) enabled = usermod[FPSTR(_enabled)].as<bool>();

      WipeReveal::RevealOptions options = onOptions();
      bool always = config.alwaysAnimate;
      uint8_t styleOn = config.styleOn;
      uint8_t styleOff = config.styleOff;
      applyOverrides(usermod, options, always);
      applyConfiguredStyles(usermod, styleOn, styleOff);
      config.alwaysAnimate = always;
      config.styleOn = WipeReveal::revealStyleForId(styleOn).id;
      config.styleOff = WipeReveal::revealStyleForId(styleOff).id;
      const JsonVariantConst durationOnValue = usermod[FPSTR(_durationOn)];
      if (durationOnValue.is<int>()) {
        config.durationOnMs = constrain(durationOnValue.as<int>(), 1, 60000);
      }
      const JsonVariantConst durationOffValue = usermod[FPSTR(_durationOff)];
      if (durationOffValue.is<int>()) {
        config.durationOffMs = constrain(durationOffValue.as<int>(), 1, 60000);
      }
      if (usermod[FPSTR(_duration)].isNull()) {
        options.durationMs = config.durationOnMs;
      }
      config.options.style = config.styleOn;
      if (usermod[FPSTR(_style)].isNull()) {
        options.style = config.styleOn;
      }

      if (usermod[FPSTR(_trigger)].is<bool>() && usermod[FPSTR(_trigger)].as<bool>()) {
        startReveal(options, false);
      } else if (usermod[FPSTR(_trigger)].is<const char*>()) {
        const char* triggerValue = usermod[FPSTR(_trigger)].as<const char*>();
        options.position = WipeReveal::parseRevealPosition(triggerValue, options.position);
        options.style = WipeReveal::parseRevealStyle(triggerValue, options.style);
        options.easing = WipeReveal::parseRevealEasing(triggerValue, options.easing);

        if (!strcmp_P(triggerValue, PSTR("on")) ||
            WipeReveal::parseRevealPosition(triggerValue, 255) != 255 ||
            WipeReveal::parseRevealStyle(triggerValue, 255) != 255 ||
            WipeReveal::parseRevealEasing(triggerValue, 255) != 255) {
          startReveal(options, false);
        }
      }
    }

#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent) override {
      char subuf[80];
      if (mqttDeviceTopic[0] != 0) {
        strcpy(subuf, mqttDeviceTopic);
        strcat_P(subuf, PSTR("/wipeReveal"));
        mqtt->subscribe(subuf, 0);
      }
    }

    bool onMqttMessage(char* topic, char* payload) override {
      if (strlen(topic) != 11 || strncmp_P(topic, PSTR("/wipeReveal"), 11) != 0) return false;

      String action = payload;
      action.trim();
      action.toLowerCase();

      WipeReveal::RevealOptions options = onOptions();
      bool always = config.alwaysAnimate;
      uint8_t styleOn = config.styleOn;
      uint8_t styleOff = config.styleOff;

      if (WipeReveal::parseRevealPosition(action.c_str(), 255) != 255) {
        options.position = WipeReveal::parseRevealPosition(action.c_str(), options.position);
      } else if (WipeReveal::parseRevealStyle(action.c_str(), 255) != 255) {
        options.style = WipeReveal::parseRevealStyle(action.c_str(), options.style);
      } else if (WipeReveal::parseRevealEasing(action.c_str(), 255) != 255) {
        options.easing = WipeReveal::parseRevealEasing(action.c_str(), options.easing);
      } else if (action.startsWith("{")) {
        StaticJsonDocument<192> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) return false;

        JsonObjectConst obj = doc.as<JsonObjectConst>();
        applyOverrides(obj, options, always);
        applyConfiguredStyles(obj, styleOn, styleOff);
        config.alwaysAnimate = always;
        config.styleOn = WipeReveal::revealStyleForId(styleOn).id;
        config.styleOff = WipeReveal::revealStyleForId(styleOff).id;
        const JsonVariantConst durationOnValue = obj[FPSTR(_durationOn)];
        if (durationOnValue.is<int>()) {
          config.durationOnMs = constrain(durationOnValue.as<int>(), 1, 60000);
        }
        const JsonVariantConst durationOffValue = obj[FPSTR(_durationOff)];
        if (durationOffValue.is<int>()) {
          config.durationOffMs = constrain(durationOffValue.as<int>(), 1, 60000);
        }
        if (obj[FPSTR(_duration)].isNull()) {
          options.durationMs = config.durationOnMs;
        }
        config.options.style = config.styleOn;
        if (obj[FPSTR(_style)].isNull()) {
          options.style = config.styleOn;
        }
      } else if (action != "on" && action != "trigger" && action != "true" && action != "1") {
        return false;
      }

      startReveal(options, false);
      return true;
    }
#endif

    void onStateChange(uint8_t mode) override {
      if (suppressPowerChange) {
        lastBri = bri;
        return;
      }
      if (!enabled || !config.alwaysAnimate || mode == CALL_MODE_INIT) {
        lastBri = bri;
        return;
      }

      const byte currentBri = bri;
      const bool wasOn = lastBri > 0;
      const bool isOn = currentBri > 0;

      if (!wasOn && isOn) {
        startReveal(onOptions(), false);
      } else if (wasOn && !isOn) {
        bri = briLast;
        offMode = false;
        applyFinalBri();
        startReveal(offOptions(), true);
      }

      lastBri = bri;
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_position)] = WipeReveal::revealPositionName(config.options.position);
      top[FPSTR(_styleOn)] = WipeReveal::revealStyleName(config.styleOn);
      top[FPSTR(_styleOff)] = WipeReveal::revealStyleName(config.styleOff);
      top[FPSTR(_easing)] = WipeReveal::revealEasingName(config.options.easing);
      top[FPSTR(_duration)] = config.options.durationMs;
      top[FPSTR(_durationOn)] = config.durationOnMs;
      top[FPSTR(_durationOff)] = config.durationOffMs;
      top[FPSTR(_dropoff)] = config.options.dropoffPx;
      top[FPSTR(_turnOn)] = turnOn;
      top[FPSTR(_alwaysAnimate)] = config.alwaysAnimate;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
      configComplete &= getJsonValue(top[FPSTR(_duration)], config.options.durationMs, 1200);
      const uint16_t durationDefault = config.options.durationMs;
      configComplete &= getJsonValue(top[FPSTR(_durationOn)], config.durationOnMs, durationDefault);
      configComplete &= getJsonValue(top[FPSTR(_durationOff)], config.durationOffMs, durationDefault);
      configComplete &= getJsonValue(top[FPSTR(_dropoff)], config.options.dropoffPx, 8);
      configComplete &= getJsonValue(top[FPSTR(_turnOn)], turnOn, true);
      configComplete &= getJsonValue(top[FPSTR(_alwaysAnimate)], config.alwaysAnimate, false);

      if (top[FPSTR(_duration)].isNull()) {
        config.options.durationMs = config.durationOnMs;
      }

      JsonVariantConst positionValue = top[FPSTR(_position)];
      if (!positionValue.isNull()) {
        config.options.position = WipeReveal::parseRevealPositionJson(positionValue, WipeReveal::RevealCenter);
      } else {
        configComplete = false;
        config.options.position = WipeReveal::RevealCenter;
      }

      uint8_t legacyStyle = WipeReveal::StyleWipe;
      JsonVariantConst styleValue = top[FPSTR(_style)];
      if (!styleValue.isNull()) {
        legacyStyle = WipeReveal::parseRevealStyleJson(styleValue, WipeReveal::StyleWipe);
      }

      JsonVariantConst styleOnValue = top[FPSTR(_styleOn)];
      if (!styleOnValue.isNull()) {
        config.styleOn = WipeReveal::parseRevealStyleJson(styleOnValue, legacyStyle);
      } else {
        config.styleOn = legacyStyle;
        configComplete = false;
      }

      JsonVariantConst styleOffValue = top[FPSTR(_styleOff)];
      if (!styleOffValue.isNull()) {
        config.styleOff = WipeReveal::parseRevealStyleJson(styleOffValue, legacyStyle);
      } else {
        config.styleOff = legacyStyle;
        configComplete = false;
      }

      config.options.style = config.styleOn;

      JsonVariantConst easingValue = top[FPSTR(_easing)];
      if (!easingValue.isNull()) {
        config.options.easing = WipeReveal::parseRevealEasingJson(easingValue, WipeReveal::EaseLinear);
      } else {
        configComplete = false;
        config.options.easing = WipeReveal::EaseLinear;
      }

      config.options.durationMs = constrain(config.options.durationMs, 1, 60000);
      config.durationOnMs = constrain(config.durationOnMs, 1, 60000);
      config.durationOffMs = constrain(config.durationOffMs, 1, 60000);
      config.options.dropoffPx = constrain(config.options.dropoffPx, 0, 255);

      return configComplete;
    }

    void appendConfigData() override {
      oappend(F("function triggerWipeReveal(){"));
      oappend(F("let g=n=>document.getElementsByName('WipeReveal:'+n)[0];"));
      oappend(F("let v=n=>g(n)?g(n).value:null;"));
      oappend(F("let c=n=>g(n)&&g(n).type==='checkbox'?g(n).checked:false;"));
      oappend(F("let d=v('duration-on-ms');"));
      oappend(F("if(d===null||d==='') d=v('duration-ms');"));
      oappend(F("fetch(getURL('/json/state'),{method:'POST',body:JSON.stringify({WipeReveal:{trigger:true,position:v('position'),easing:v('easing'),style:v('style-on'),'duration-ms':parseInt(d||0),'dropoff-px':parseInt(v('dropoff-px')),'always-animate':c('always-animate')}})});"));
      oappend(F("}"));
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_position)).c_str()); oappend(F("');"));
      oappend(F("addOption(dd,'Start','start');"));
      oappend(F("addOption(dd,'Center','center');"));
      oappend(F("addOption(dd,'End','end');"));
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_styleOn)).c_str()); oappend(F("');"));
      for (uint8_t i = 0; i < WipeReveal::revealStyleCount(); i++) {
        const WipeReveal::RevealStyleDescriptor& style = WipeReveal::revealStyleAt(i);
        oappend(F("addOption(dd,'"));
        oappend(style.label);
        oappend(F("','"));
        oappend(style.name);
        oappend(F("');"));
      }
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_styleOff)).c_str()); oappend(F("');"));
      for (uint8_t i = 0; i < WipeReveal::revealStyleCount(); i++) {
        const WipeReveal::RevealStyleDescriptor& style = WipeReveal::revealStyleAt(i);
        oappend(F("addOption(dd,'"));
        oappend(style.label);
        oappend(F("','"));
        oappend(style.name);
        oappend(F("');"));
      }
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_easing)).c_str()); oappend(F("');"));
      oappend(F("addOption(dd,'Linear','linear');"));
      oappend(F("addOption(dd,'Ease In','ease-in');"));
      oappend(F("addOption(dd,'Ease Out','ease-out');"));
      oappend(F("addOption(dd,'Ease In Out','ease-in-out');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_enabled)).c_str()); oappend(F("',1,'<button type=\"button\" onclick=\"triggerWipeReveal()\">Trigger Wipe Reveal</button>');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_duration)).c_str()); oappend(F("',1,'Manual trigger duration in milliseconds.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_durationOn)).c_str()); oappend(F("',1,'Duration for the power-on (on style) animation.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_durationOff)).c_str()); oappend(F("',1,'Duration for the power-off (off style) animation.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_dropoff)).c_str()); oappend(F("',1,'Soft edge width in pixels.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_styleOn)).c_str()); oappend(F("',1,'Style used for manual trigger and power-on animation.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_styleOff)).c_str()); oappend(F("',1,'Style used for power-off animation when always animate is enabled.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_alwaysAnimate)).c_str()); oappend(F("',1,'Animate on every power on and off. Power off uses the selected off style in reverse.');"));
    }

    uint16_t getId() override {
      return USERMOD_ID_WIPE_REVEAL;
    }
};

const char WipeRevealUsermod::_name[]     PROGMEM = "WipeReveal";
const char WipeRevealUsermod::_enabled[]  PROGMEM = "enabled";
const char WipeRevealUsermod::_trigger[]  PROGMEM = "trigger";
const char WipeRevealUsermod::_position[] PROGMEM = "position";
const char WipeRevealUsermod::_style[]    PROGMEM = "style";
const char WipeRevealUsermod::_styleOn[]  PROGMEM = "style-on";
const char WipeRevealUsermod::_styleOff[] PROGMEM = "style-off";
const char WipeRevealUsermod::_easing[]   PROGMEM = "easing";
const char WipeRevealUsermod::_duration[] PROGMEM = "duration-ms";
const char WipeRevealUsermod::_durationOn[] PROGMEM = "duration-on-ms";
const char WipeRevealUsermod::_durationOff[] PROGMEM = "duration-off-ms";
const char WipeRevealUsermod::_dropoff[]  PROGMEM = "dropoff-px";
const char WipeRevealUsermod::_turnOn[]   PROGMEM = "turn-on";
const char WipeRevealUsermod::_alwaysAnimate[] PROGMEM = "always-animate";
const char WipeRevealUsermod::_running[]  PROGMEM = "running";
const char WipeRevealUsermod::_progress[] PROGMEM = "progress";

static WipeRevealUsermod wipeReveal;
REGISTER_USERMOD(wipeReveal);
