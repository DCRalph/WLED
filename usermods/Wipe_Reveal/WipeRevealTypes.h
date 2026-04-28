#pragma once

#include "wled.h"

namespace WipeReveal {

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
  StyleBounce = 1,
  StyleComet = 2,
  StyleTwinkle = 3
};

struct RevealOptions {
  uint8_t position = RevealCenter;
  uint8_t style = StyleWipe;
  uint8_t easing = EaseLinear;
  uint16_t durationMs = 1200;
  uint16_t dropoffPx = 8;
};

struct RevealConfig {
  RevealOptions options;
  uint8_t styleOn = StyleWipe;
  uint8_t styleOff = StyleWipe;
  bool alwaysAnimate = false;
};

struct RevealAnimationState {
  RevealOptions options;
  unsigned long startTime = 0;
  unsigned long lastTriggerFrame = 0;
  bool running = false;
  bool reversing = false;
};

inline uint8_t parseRevealPosition(const char* value, uint8_t fallback) {
  if (!value) return fallback;
  if (!strcmp_P(value, PSTR("start"))) return RevealStart;
  if (!strcmp_P(value, PSTR("center"))) return RevealCenter;
  if (!strcmp_P(value, PSTR("end"))) return RevealEnd;
  return fallback;
}

inline uint8_t parseRevealPositionJson(JsonVariantConst value, uint8_t fallback) {
  if (value.is<const char*>()) return parseRevealPosition(value.as<const char*>(), fallback);
  if (value.is<int>()) return constrain(value.as<int>(), RevealStart, RevealEnd);
  return fallback;
}

inline const char* revealPositionName(uint8_t value) {
  switch (value) {
    case RevealStart:  return "start";
    case RevealEnd:    return "end";
    case RevealCenter:
    default:           return "center";
  }
}

inline uint8_t parseRevealEasing(const char* value, uint8_t fallback) {
  if (!value) return fallback;
  if (!strcmp_P(value, PSTR("linear"))) return EaseLinear;
  if (!strcmp_P(value, PSTR("ease-in")) || !strcmp_P(value, PSTR("easeIn"))) return EaseIn;
  if (!strcmp_P(value, PSTR("ease-out")) || !strcmp_P(value, PSTR("easeOut"))) return EaseOut;
  if (!strcmp_P(value, PSTR("ease-in-out")) || !strcmp_P(value, PSTR("easeInOut"))) return EaseInOut;
  return fallback;
}

inline uint8_t parseRevealEasingJson(JsonVariantConst value, uint8_t fallback) {
  if (value.is<const char*>()) return parseRevealEasing(value.as<const char*>(), fallback);
  if (value.is<int>()) return constrain(value.as<int>(), EaseLinear, EaseInOut);
  return fallback;
}

inline const char* revealEasingName(uint8_t value) {
  switch (value) {
    case EaseIn:     return "ease-in";
    case EaseOut:    return "ease-out";
    case EaseInOut:  return "ease-in-out";
    case EaseLinear:
    default:         return "linear";
  }
}

}  // namespace WipeReveal
