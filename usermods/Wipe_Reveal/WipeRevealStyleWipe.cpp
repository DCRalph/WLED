#include "WipeRevealStyleWipe.h"

namespace WipeReveal {

uint8_t renderWipeScale(uint16_t pixel, uint16_t len, uint16_t progress, const RevealOptions& options) {
  const uint16_t eased = easedProgress(progress, options.easing);
  const uint32_t edge = (uint32_t(eased) * uint32_t(revealExtent(len, options.position) + options.dropoffPx + 1)) / 0xFFFFUL;
  const uint16_t distance = distanceFromOrigin(pixel, len, options.position);
  return scaleForDistance(distance, edge, options.dropoffPx);
}

}  // namespace WipeReveal
