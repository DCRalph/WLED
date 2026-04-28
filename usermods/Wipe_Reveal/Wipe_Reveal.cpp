#include "wled.h"

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
    static const char _easing[];
    static const char _duration[];
    static const char _dropoff[];
    static const char _turnOn[];
    static const char _alwaysAnimate[];
    static const char _running[];
    static const char _progress[];

    enum RevealPosition : uint8_t {
      RevealStart = 0,
      RevealCenter = 1,
      RevealEnd = 2
    };

    enum RevealEasing : uint8_t {
      EaseLinear = 0,
      EaseIn = 1,
      EaseOut = 2,
      EaseInOut = 3
    };

    enum RevealStyle : uint8_t {
      StyleWipe = 0,
      StyleBounce = 1
    };

    bool enabled = true;
    bool turnOn = true;
    bool alwaysAnimate = false;
    bool running = false;
    bool revealingOff = false;
    bool suppressPowerChange = false;
    byte lastBri = 0;
    uint8_t position = RevealCenter;
    uint8_t style = StyleWipe;
    uint8_t easing = EaseLinear;
    uint16_t durationMs = 1200;
    uint16_t dropoffPx = 8;

    uint8_t activePosition = RevealCenter;
    uint8_t activeStyle = StyleWipe;
    uint8_t activeEasing = EaseLinear;
    uint16_t activeDurationMs = 1200;
    uint16_t activeDropoffPx = 8;
    unsigned long startTime = 0;
    unsigned long lastTriggerFrame = 0;
    bool initDone = false;

    static uint8_t parsePosition(const char* value, uint8_t fallback) {
      if (!value) return fallback;
      if (!strcmp_P(value, PSTR("start"))) return RevealStart;
      if (!strcmp_P(value, PSTR("center"))) return RevealCenter;
      if (!strcmp_P(value, PSTR("end"))) return RevealEnd;
      return fallback;
    }

    static uint8_t parseEasing(const char* value, uint8_t fallback) {
      if (!value) return fallback;
      if (!strcmp_P(value, PSTR("linear"))) return EaseLinear;
      if (!strcmp_P(value, PSTR("ease-in")) || !strcmp_P(value, PSTR("easeIn"))) return EaseIn;
      if (!strcmp_P(value, PSTR("ease-out")) || !strcmp_P(value, PSTR("easeOut"))) return EaseOut;
      if (!strcmp_P(value, PSTR("ease-in-out")) || !strcmp_P(value, PSTR("easeInOut"))) return EaseInOut;
      return fallback;
    }

    static uint8_t parseStyle(const char* value, uint8_t fallback) {
      if (!value) return fallback;
      if (!strcmp_P(value, PSTR("wipe"))) return StyleWipe;
      if (!strcmp_P(value, PSTR("bounce"))) return StyleBounce;
      return fallback;
    }

    static const __FlashStringHelper* positionName(uint8_t value) {
      switch (value) {
        case RevealStart:  return F("start");
        case RevealEnd:    return F("end");
        case RevealCenter:
        default:           return F("center");
      }
    }

    static const __FlashStringHelper* easingName(uint8_t value) {
      switch (value) {
        case EaseIn:     return F("ease-in");
        case EaseOut:    return F("ease-out");
        case EaseInOut:  return F("ease-in-out");
        case EaseLinear:
        default:         return F("linear");
      }
    }

    static const __FlashStringHelper* styleName(uint8_t value) {
      switch (value) {
        case StyleBounce: return F("bounce");
        case StyleWipe:
        default:          return F("wipe");
      }
    }

    static uint8_t parsePositionJson(JsonVariantConst value, uint8_t fallback) {
      if (value.is<const char*>()) return parsePosition(value.as<const char*>(), fallback);
      if (value.is<int>()) return constrain(value.as<int>(), RevealStart, RevealEnd);
      return fallback;
    }

    static uint8_t parseEasingJson(JsonVariantConst value, uint8_t fallback) {
      if (value.is<const char*>()) return parseEasing(value.as<const char*>(), fallback);
      if (value.is<int>()) return constrain(value.as<int>(), EaseLinear, EaseInOut);
      return fallback;
    }

    static uint8_t parseStyleJson(JsonVariantConst value, uint8_t fallback) {
      if (value.is<const char*>()) return parseStyle(value.as<const char*>(), fallback);
      if (value.is<int>()) return constrain(value.as<int>(), StyleWipe, StyleBounce);
      return fallback;
    }

    static uint16_t easedProgress(uint16_t progress, uint8_t ease) {
      const uint32_t inv = 0xFFFFU - progress;
      switch (ease) {
        case EaseIn:
          return (uint64_t(progress) * progress) / 0xFFFFU;
        case EaseOut:
          return 0xFFFFU - ((uint64_t(inv) * inv) / 0xFFFFU);
        case EaseInOut:
          if (progress < 0x8000U) return (2ULL * progress * progress) / 0xFFFFU;
          return 0xFFFFU - ((2ULL * inv * inv) / 0xFFFFU);
        case EaseLinear:
        default:
          return progress;
      }
    }

    static uint16_t revealExtent(uint16_t len, uint8_t pos) {
      if (pos != RevealCenter) return len;

      const uint16_t leftCenter = (len - 1) / 2;
      const uint16_t rightCenter = len / 2;
      const uint16_t leftExtent = leftCenter + 1;
      const uint16_t rightExtent = len - rightCenter;
      return max(leftExtent, rightExtent);
    }

    static uint16_t distanceFromOrigin(uint16_t pixel, uint16_t len, uint8_t pos) {
      switch (pos) {
        case RevealStart:
          return pixel;
        case RevealEnd:
          return len - 1 - pixel;
        case RevealCenter:
        default: {
          const uint16_t leftCenter = (len - 1) / 2;
          const uint16_t rightCenter = len / 2;
          if (pixel < leftCenter) return leftCenter - pixel;
          if (pixel > rightCenter) return pixel - rightCenter;
          return 0;
        }
      }
    }

    static uint8_t scaleForDistance(uint16_t distance, uint32_t edge, uint16_t dropoff) {
      if (dropoff == 0) return distance < edge ? 255 : 0;
      if (edge <= distance) return 0;
      if (edge >= uint32_t(distance) + dropoff) return 255;
      return ((edge - distance) * 255U) / dropoff;
    }

    static uint8_t scaleForWindow(uint16_t distance, uint16_t start, uint16_t end, uint16_t dropoff) {
      if (end < start) return 0;
      if (dropoff == 0) return (distance >= start && distance <= end) ? 255 : 0;
      if (distance < start) {
        const uint16_t delta = start - distance;
        if (delta >= dropoff) return 0;
        return ((dropoff - delta) * 255U) / dropoff;
      }
      if (distance > end) {
        const uint16_t delta = distance - end;
        if (delta >= dropoff) return 0;
        return ((dropoff - delta) * 255U) / dropoff;
      }
      return 255;
    }

    static uint16_t phaseProgress(uint16_t progress, uint8_t startUnit, uint8_t durationUnit) {
      constexpr uint8_t totalUnits = 27;
      const uint32_t start = (uint32_t(startUnit) * 0xFFFFU) / totalUnits;
      const uint32_t stop = (uint32_t(startUnit + durationUnit) * 0xFFFFU) / totalUnits;
      if (progress <= start) return 0;
      if (progress >= stop) return 0xFFFFU;
      return (uint32_t(progress - start) * 0xFFFFU) / (stop - start);
    }

    static uint8_t bounceScaleForDistance(uint16_t distance, uint16_t len, uint16_t progress, uint8_t ease, uint16_t dropoff) {
      if (progress >= 0xFFFFU) return 255;

      const uint16_t extent = revealExtent(len, RevealCenter);
      uint16_t dashLength = len / 8;
      if (dashLength < 2) dashLength = 2;
      if (dashLength > 15) dashLength = 15;
      uint16_t edgeStop = extent / 3;
      if (edgeStop < 1) edgeStop = 1;
      if (edgeStop > 15) edgeStop = 15;

      if (progress < (6UL * 0xFFFFU) / 27U) {
        const uint16_t p = easedProgress(phaseProgress(progress, 0, 6), ease);
        const uint16_t dashEnd = (uint32_t(p) * extent) / 0xFFFFU;
        const uint16_t dashStart = dashEnd > dashLength ? dashEnd - dashLength : 0;
        return scaleForWindow(distance, dashStart, dashEnd, dropoff);
      }

      if (progress < (12UL * 0xFFFFU) / 27U) {
        const uint16_t p = easedProgress(phaseProgress(progress, 6, 6), ease);
        const uint16_t dashEnd = (uint32_t(0xFFFFU - p) * extent) / 0xFFFFU;
        const uint16_t dashStart = dashEnd > dashLength ? dashEnd - dashLength : 0;
        return scaleForWindow(distance, dashStart, dashEnd, dropoff);
      }

      if (progress < (18UL * 0xFFFFU) / 27U) {
        const uint16_t p = easedProgress(phaseProgress(progress, 12, 6), ease);
        const uint32_t edge = (uint32_t(p) * uint32_t(extent + dropoff + 1)) / 0xFFFFU;
        return scaleForDistance(distance, edge, dropoff);
      }

      if (progress < (21UL * 0xFFFFU) / 27U) return 255;

      const uint16_t p = easedProgress(phaseProgress(progress, 21, 6), ease);
      const uint16_t cutoff = (uint32_t(p) * (extent > edgeStop ? extent - edgeStop : extent)) / 0xFFFFU;
      return distance >= cutoff ? 255 : scaleForWindow(distance, cutoff, cutoff + dropoff, dropoff);
    }

    void applyOverrides(JsonObjectConst obj, uint8_t& pos, uint8_t& styleValue, uint8_t& ease, uint16_t& dur, uint16_t& dropoff, bool& always) {
      JsonVariantConst positionValue = obj[FPSTR(_position)];
      if (!positionValue.isNull()) {
        pos = parsePositionJson(positionValue, pos);
      }
      JsonVariantConst styleJsonValue = obj[FPSTR(_style)];
      if (!styleJsonValue.isNull()) {
        styleValue = parseStyleJson(styleJsonValue, styleValue);
      }
      JsonVariantConst easingValue = obj[FPSTR(_easing)];
      if (!easingValue.isNull()) {
        ease = parseEasingJson(easingValue, ease);
      }
      if (obj[FPSTR(_alwaysAnimate)].is<bool>()) {
        always = obj[FPSTR(_alwaysAnimate)].as<bool>();
      }
      dur = constrain(obj[FPSTR(_duration)] | dur, 1, 60000);
      dropoff = constrain(obj[FPSTR(_dropoff)] | dropoff, 0, 255);
    }

    void startReveal(uint8_t pos, uint8_t styleValue, uint8_t ease, uint16_t dur, uint16_t dropoff, bool reverse) {
      if (!enabled) return;

      activePosition = constrain(pos, RevealStart, RevealEnd);
      activeStyle = constrain(styleValue, StyleWipe, StyleBounce);
      activeEasing = constrain(ease, EaseLinear, EaseInOut);
      activeDurationMs = dur ? dur : 1;
      activeDropoffPx = dropoff;
      startTime = millis();
      lastTriggerFrame = 0;
      running = true;
      revealingOff = reverse;

      if (turnOn && bri == 0 && !revealingOff) {
        bri = briLast;
        colorUpdated(CALL_MODE_DIRECT_CHANGE);
      } else {
        strip.trigger();
      }
      stateChanged = true;
    }

    void completeOffReveal() {
      running = false;
      revealingOff = false;
      suppressPowerChange = true;
      bri = 0;
      offMode = true;
      applyFinalBri();
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
      suppressPowerChange = false;
      lastBri = 0;
    }

    uint8_t progressPercent() const {
      if (!running) return 0;
      const unsigned long elapsed = millis() - startTime;
      if (elapsed >= activeDurationMs) return 100;
      return (elapsed * 100UL) / activeDurationMs;
    }

  public:
    void setup() override {
      initDone = true;
      lastBri = bri;
    }

    void loop() override {
      if (!running) return;

      const unsigned long now = millis();
      if (now - startTime >= activeDurationMs) {
        running = false;
        if (revealingOff) {
          completeOffReveal();
        }
        strip.trigger();
        stateChanged = true;
        return;
      }

      if (now - lastTriggerFrame >= 10) {
        lastTriggerFrame = now;
        strip.trigger();
      }
    }

    void handleOverlayDraw() override {
      if (!enabled || !running) return;

      const unsigned long elapsed = millis() - startTime;
      if (elapsed >= activeDurationMs) {
        running = false;
        if (revealingOff) {
          completeOffReveal();
        }
        return;
      }

      const unsigned totalLen = strip.getLengthTotal();

      for (unsigned segmentIndex = 0; segmentIndex < strip.getSegmentsNum(); segmentIndex++) {
        const Segment& seg = strip.getSegment(segmentIndex);
        if (!seg.isActive()) continue;

        const uint16_t len = seg.length();
        if (len == 0) continue;

        const uint16_t linearProgress = (elapsed * 0xFFFFUL) / activeDurationMs;
        const uint16_t progress = revealingOff ? (0xFFFFU - linearProgress) : linearProgress;
        const uint16_t eased = easedProgress(progress, activeEasing);
        const uint32_t wipeEdge = (uint32_t(eased) * uint32_t(revealExtent(len, activePosition) + activeDropoffPx + 1)) / 0xFFFFUL;

        for (uint16_t pixel = 0; pixel < len; pixel++) {
          const unsigned absolutePixel = seg.start + pixel;
          if (absolutePixel >= totalLen) break;

          const uint16_t distance = distanceFromOrigin(pixel, len, activePosition);
          const uint8_t scale = activeStyle == StyleBounce
            ? bounceScaleForDistance(distanceFromOrigin(pixel, len, RevealCenter), len, progress, activeEasing, activeDropoffPx)
            : scaleForDistance(distance, wipeEdge, activeDropoffPx);
          if (scale >= 255) continue;

          strip.setPixelColor(absolutePixel, color_fade(strip.getPixelColor(absolutePixel), scale, true));
        }
      }
    }

    void addToJsonState(JsonObject& root) override {
      if (!initDone || !enabled) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      usermod[FPSTR(_running)] = running;
      usermod[FPSTR(_position)] = positionName(running ? activePosition : position);
      usermod[FPSTR(_style)] = styleName(running ? activeStyle : style);
      usermod[FPSTR(_easing)] = easingName(running ? activeEasing : easing);
      usermod[FPSTR(_alwaysAnimate)] = alwaysAnimate;
      usermod[FPSTR(_progress)] = progressPercent();
    }

    void readFromJsonState(JsonObject& root) override {
      if (!initDone) return;

      JsonObjectConst usermod = root[FPSTR(_name)];
      if (usermod.isNull()) return;

      if (usermod[FPSTR(_enabled)].is<bool>()) enabled = usermod[FPSTR(_enabled)].as<bool>();

      uint8_t pos = position;
      uint8_t styleValue = style;
      uint8_t ease = easing;
      bool always = alwaysAnimate;
      uint16_t dur = durationMs;
      uint16_t dropoff = dropoffPx;
      applyOverrides(usermod, pos, styleValue, ease, dur, dropoff, always);
      alwaysAnimate = always;

      if (usermod[FPSTR(_trigger)].is<bool>() && usermod[FPSTR(_trigger)].as<bool>()) {
        startReveal(pos, styleValue, ease, dur, dropoff, false);
      } else if (usermod[FPSTR(_trigger)].is<const char*>()) {
        const char* triggerValue = usermod[FPSTR(_trigger)].as<const char*>();
        pos = parsePosition(triggerValue, pos);
        styleValue = parseStyle(triggerValue, styleValue);
        ease = parseEasing(triggerValue, ease);
        if (!strcmp_P(triggerValue, PSTR("on")) || !strcmp_P(triggerValue, PSTR("start")) ||
            !strcmp_P(triggerValue, PSTR("center")) || !strcmp_P(triggerValue, PSTR("end")) ||
            parseStyle(triggerValue, 255) != 255 || parseEasing(triggerValue, 255) != 255) {
          startReveal(pos, styleValue, ease, dur, dropoff, false);
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

      uint8_t pos = position;
      uint8_t styleValue = style;
      uint8_t ease = easing;
      bool always = alwaysAnimate;
      uint16_t dur = durationMs;
      uint16_t dropoff = dropoffPx;

      if (action == "start" || action == "center" || action == "end") {
        pos = parsePosition(action.c_str(), pos);
      } else if (parseStyle(action.c_str(), 255) != 255) {
        styleValue = parseStyle(action.c_str(), styleValue);
      } else if (parseEasing(action.c_str(), 255) != 255) {
        ease = parseEasing(action.c_str(), ease);
      } else if (action.startsWith("{")) {
        StaticJsonDocument<192> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) return false;

        JsonObjectConst obj = doc.as<JsonObjectConst>();
        applyOverrides(obj, pos, styleValue, ease, dur, dropoff, always);
        alwaysAnimate = always;
      } else if (action != "on" && action != "trigger" && action != "true" && action != "1") {
        return false;
      }

      startReveal(pos, styleValue, ease, dur, dropoff, false);
      return true;
    }
#endif

    void onStateChange(uint8_t mode) override {
      if (suppressPowerChange) {
        lastBri = bri;
        return;
      }
      if (!enabled || !alwaysAnimate || mode == CALL_MODE_INIT) {
        lastBri = bri;
        return;
      }

      const byte currentBri = bri;
      const bool wasOn = lastBri > 0;
      const bool isOn = currentBri > 0;

      if (!wasOn && isOn) {
        startReveal(position, style, easing, durationMs, dropoffPx, false);
      } else if (wasOn && !isOn) {
        bri = briLast;
        offMode = false;
        applyFinalBri();
        startReveal(position, style, easing, durationMs, dropoffPx, true);
      }

      lastBri = bri;
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_position)] = positionName(position);
      top[FPSTR(_style)] = styleName(style);
      top[FPSTR(_easing)] = easingName(easing);
      top[FPSTR(_duration)] = durationMs;
      top[FPSTR(_dropoff)] = dropoffPx;
      top[FPSTR(_turnOn)] = turnOn;
      top[FPSTR(_alwaysAnimate)] = alwaysAnimate;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
      configComplete &= getJsonValue(top[FPSTR(_duration)], durationMs, 1200);
      configComplete &= getJsonValue(top[FPSTR(_dropoff)], dropoffPx, 8);
      configComplete &= getJsonValue(top[FPSTR(_turnOn)], turnOn, true);
      configComplete &= getJsonValue(top[FPSTR(_alwaysAnimate)], alwaysAnimate, false);

      JsonVariantConst positionValue = top[FPSTR(_position)];
      if (!positionValue.isNull()) {
        position = parsePositionJson(positionValue, RevealCenter);
      } else {
        configComplete = false;
        position = RevealCenter;
      }
      JsonVariantConst styleValue = top[FPSTR(_style)];
      if (!styleValue.isNull()) {
        style = parseStyleJson(styleValue, StyleWipe);
      } else {
        configComplete = false;
        style = StyleWipe;
      }
      JsonVariantConst easingValue = top[FPSTR(_easing)];
      if (!easingValue.isNull()) {
        easing = parseEasingJson(easingValue, EaseLinear);
      } else {
        configComplete = false;
        easing = EaseLinear;
      }

      durationMs = constrain(durationMs, 1, 60000);
      dropoffPx = constrain(dropoffPx, 0, 255);

      return configComplete;
    }

    void appendConfigData() override {
      oappend(F("function triggerWipeReveal(){"));
      oappend(F("let g=n=>document.getElementsByName('WipeReveal:'+n)[0];"));
      oappend(F("let v=n=>g(n)?g(n).value:null;"));
      oappend(F("let c=n=>g(n)&&g(n).type==='checkbox'?g(n).checked:false;"));
      oappend(F("fetch(getURL('/json/state'),{method:'POST',body:JSON.stringify({WipeReveal:{trigger:true,position:v('position'),easing:v('easing'),style:v('style'),'duration-ms':parseInt(v('duration-ms')),'dropoff-px':parseInt(v('dropoff-px')),'always-animate':c('always-animate')}})});"));
      oappend(F("}"));
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_position)).c_str()); oappend(F("');"));
      oappend(F("addOption(dd,'Start','start');"));
      oappend(F("addOption(dd,'Center','center');"));
      oappend(F("addOption(dd,'End','end');"));
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_style)).c_str()); oappend(F("');"));
      oappend(F("addOption(dd,'Wipe','wipe');"));
      oappend(F("addOption(dd,'Bounce','bounce');"));
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','")); oappend(String(FPSTR(_easing)).c_str()); oappend(F("');"));
      oappend(F("addOption(dd,'Linear','linear');"));
      oappend(F("addOption(dd,'Ease In','ease-in');"));
      oappend(F("addOption(dd,'Ease Out','ease-out');"));
      oappend(F("addOption(dd,'Ease In Out','ease-in-out');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_enabled)).c_str()); oappend(F("',1,'<button type=\"button\" onclick=\"triggerWipeReveal()\">Trigger Wipe Reveal</button>');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_duration)).c_str()); oappend(F("',1,'Reveal animation duration in milliseconds.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_dropoff)).c_str()); oappend(F("',1,'Soft edge width in pixels.');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_alwaysAnimate)).c_str()); oappend(F("',1,'Animate on every power on and off. Off uses the reverse wipe.');"));
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
const char WipeRevealUsermod::_easing[]   PROGMEM = "easing";
const char WipeRevealUsermod::_duration[] PROGMEM = "duration-ms";
const char WipeRevealUsermod::_dropoff[]  PROGMEM = "dropoff-px";
const char WipeRevealUsermod::_turnOn[]   PROGMEM = "turn-on";
const char WipeRevealUsermod::_alwaysAnimate[] PROGMEM = "always-animate";
const char WipeRevealUsermod::_running[]  PROGMEM = "running";
const char WipeRevealUsermod::_progress[] PROGMEM = "progress";

static WipeRevealUsermod wipeReveal;
REGISTER_USERMOD(wipeReveal);
