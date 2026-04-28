#include "WipeRevealStyles.h"

#include "WipeRevealStyleBounce.h"
#include "WipeRevealStyleComet.h"
#include "WipeRevealStyleTwinkle.h"
#include "WipeRevealStyleWipe.h"

namespace WipeReveal {

static const RevealStyleDescriptor revealStyles[] = {
  {StyleWipe, "wipe", "Wipe", &renderWipeScale},
  {StyleBounce, "bounce", "Bounce", &renderBounceScale},
  {StyleComet, "comet", "Comet", &renderCometScale},
  {StyleTwinkle, "twinkle", "Twinkle", &renderTwinkleScale}
};

static const RevealStyleDescriptor* findStyleById(uint8_t styleId) {
  for (uint8_t i = 0; i < revealStyleCount(); i++) {
    if (revealStyles[i].id == styleId) return &revealStyles[i];
  }
  return nullptr;
}

const RevealStyleDescriptor& revealStyleForId(uint8_t styleId) {
  const RevealStyleDescriptor* style = findStyleById(styleId);
  return style ? *style : revealStyles[0];
}

const RevealStyleDescriptor& revealStyleAt(uint8_t index) {
  return index < revealStyleCount() ? revealStyles[index] : revealStyles[0];
}

uint8_t revealStyleCount() {
  return sizeof(revealStyles) / sizeof(revealStyles[0]);
}

uint8_t parseRevealStyle(const char* value, uint8_t fallback) {
  if (!value) return fallback;
  for (uint8_t i = 0; i < revealStyleCount(); i++) {
    if (!strcmp(value, revealStyles[i].name)) return revealStyles[i].id;
  }
  return fallback;
}

uint8_t parseRevealStyleJson(JsonVariantConst value, uint8_t fallback) {
  if (value.is<const char*>()) return parseRevealStyle(value.as<const char*>(), fallback);
  if (value.is<int>()) {
    const uint8_t index = constrain(value.as<int>(), 0, int(revealStyleCount() - 1));
    return revealStyleAt(index).id;
  }
  return fallback;
}

const char* revealStyleName(uint8_t value) {
  return revealStyleForId(value).name;
}

}  // namespace WipeReveal
