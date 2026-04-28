#pragma once

#include "WipeRevealMath.h"

namespace WipeReveal {

typedef uint8_t (*RevealStyleRenderFn)(uint16_t pixel, uint16_t len, uint16_t progress, const RevealOptions& options);

struct RevealStyleDescriptor {
  uint8_t id;
  const char* name;
  const char* label;
  RevealStyleRenderFn renderScale;
};

const RevealStyleDescriptor& revealStyleForId(uint8_t styleId);
const RevealStyleDescriptor& revealStyleAt(uint8_t index);
uint8_t revealStyleCount();
uint8_t parseRevealStyle(const char* value, uint8_t fallback);
uint8_t parseRevealStyleJson(JsonVariantConst value, uint8_t fallback);
const char* revealStyleName(uint8_t value);

}  // namespace WipeReveal
