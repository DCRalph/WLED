#include "WipeRevealStyleComet.h"

namespace WipeReveal {

static uint8_t scaleForPulseWindow(uint16_t distance, uint32_t center, uint16_t halfWidth, uint16_t dropoff) {
  const uint32_t pulseStart = center > halfWidth ? center - halfWidth : 0;
  const uint32_t pulseEnd = center + halfWidth;
  if (distance < pulseStart) {
    const uint32_t delta = pulseStart - distance;
    if (delta >= dropoff) return 0;
    return dropoff == 0 ? 0 : ((dropoff - delta) * 255U) / dropoff;
  }
  if (distance > pulseEnd) {
    const uint32_t delta = distance - pulseEnd;
    if (delta >= dropoff) return 0;
    return dropoff == 0 ? 0 : ((dropoff - delta) * 255U) / dropoff;
  }
  return 255;
}

uint8_t renderCometScale(uint16_t pixel, uint16_t len, uint16_t progress, const RevealOptions& options) {
  const uint16_t distance = distanceFromOrigin(pixel, len, options.position);
  const uint16_t eased = easedProgress(progress, options.easing);
  const uint16_t extent = revealExtent(len, options.position);
  const uint32_t edge = (uint32_t(eased) * uint32_t(extent + options.dropoffPx + 1)) / 0xFFFFU;
  const uint8_t fillScale = scaleForDistance(distance, edge, options.dropoffPx);

  uint16_t pulseHalfWidth = len / 10;
  if (pulseHalfWidth < 2) pulseHalfWidth = 2;
  if (pulseHalfWidth > 16) pulseHalfWidth = 16;
  const uint16_t pulseDropoff = options.dropoffPx > 3 ? options.dropoffPx : 3;
  const uint8_t pulseScale = scaleForPulseWindow(distance, edge, pulseHalfWidth, pulseDropoff);

  return max(fillScale, pulseScale);
}

}  // namespace WipeReveal
