#include "WipeRevealMath.h"

namespace WipeReveal {

uint16_t easedProgress(uint16_t progress, uint8_t ease) {
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

uint16_t revealExtent(uint16_t len, uint8_t pos) {
  if (pos != RevealCenter) return len;

  const uint16_t leftCenter = (len - 1) / 2;
  const uint16_t rightCenter = len / 2;
  const uint16_t leftExtent = leftCenter + 1;
  const uint16_t rightExtent = len - rightCenter;
  return max(leftExtent, rightExtent);
}

uint16_t distanceFromOrigin(uint16_t pixel, uint16_t len, uint8_t pos) {
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

uint8_t scaleForDistance(uint16_t distance, uint32_t edge, uint16_t dropoff) {
  if (dropoff == 0) return distance < edge ? 255 : 0;
  if (edge <= distance) return 0;
  if (edge >= uint32_t(distance) + dropoff) return 255;
  return ((edge - distance) * 255U) / dropoff;
}

}  // namespace WipeReveal
