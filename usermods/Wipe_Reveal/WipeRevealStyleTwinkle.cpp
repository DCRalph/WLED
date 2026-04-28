#include "WipeRevealStyleTwinkle.h"

namespace WipeReveal {

static uint8_t sparkleNoise(uint16_t pixel, uint16_t len, uint16_t progress) {
  uint32_t value = uint32_t(pixel + 1U) * 1103515245UL;
  value ^= uint32_t(len + 1U) * 2246822519UL;
  value ^= uint32_t((progress >> 7) + 1U) * 3266489917UL;
  value ^= value >> 16;
  return value & 0xFF;
}

uint8_t renderTwinkleScale(uint16_t pixel, uint16_t len, uint16_t progress, const RevealOptions& options) {
  const uint16_t distance = distanceFromOrigin(pixel, len, options.position);
  const uint16_t eased = easedProgress(progress, options.easing);
  const uint16_t extent = revealExtent(len, options.position);
  const uint32_t edge = (uint32_t(eased) * uint32_t(extent + options.dropoffPx + 1)) / 0xFFFFU;

  uint16_t sparkleBand = len / 6;
  if (sparkleBand < 4) sparkleBand = 4;
  if (sparkleBand > 24) sparkleBand = 24;

  if (edge >= uint32_t(distance) + sparkleBand) return 255;

  const uint8_t baseScale = scaleForDistance(distance, edge, options.dropoffPx);
  const int32_t leadingBand = int32_t(edge) + int32_t(sparkleBand) - int32_t(distance);
  if (leadingBand <= 0) return baseScale;

  uint32_t thresholdValue = (uint32_t(leadingBand) * 255U) / sparkleBand;
  if (thresholdValue > 255U) thresholdValue = 255U;
  const uint8_t threshold = thresholdValue;
  uint8_t sparkle = sparkleNoise(pixel, len, progress);
  return sparkle < threshold ? 255 : baseScale;
}

}  // namespace WipeReveal
