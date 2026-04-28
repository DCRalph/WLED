#include "WipeRevealStyleBounce.h"

namespace WipeReveal {

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

  if (progress < (6UL * 0xFFFFU) / 27U) { // first phase shoot out from center
    const uint16_t p = easedProgress(phaseProgress(progress, 0, 6), ease);
    const uint16_t dashEnd = (uint32_t(p) * extent) / 0xFFFFU;
    const uint16_t dashStart = dashEnd > dashLength ? dashEnd - dashLength : 0;
    return scaleForWindow(distance, dashStart, dashEnd, dropoff);
  }

  if (progress < (12UL * 0xFFFFU) / 27U) { // second phase shoot in from edges
    const uint16_t p = easedProgress(phaseProgress(progress, 6, 6), ease);
    const uint16_t dashEnd = (uint32_t(0xFFFFU - p) * extent) / 0xFFFFU;
    const uint16_t dashStart = dashEnd > dashLength ? dashEnd - dashLength : 0;
    return scaleForWindow(distance, dashStart, dashEnd, dropoff);
  }

  if (progress < (18UL * 0xFFFFU) / 27U) { // third phase full out from center
    const uint16_t p = easedProgress(phaseProgress(progress, 12, 6), ease);
    const uint32_t edge = (uint32_t(p) * uint32_t(extent + dropoff + 1)) / 0xFFFFU;
    return scaleForDistance(distance, edge, dropoff);
  }

  // Once the center-out fill completes, keep the segment fully visible.
  return 255;
}

uint8_t renderBounceScale(uint16_t pixel, uint16_t len, uint16_t progress, const RevealOptions& options) {
  const uint16_t distance = distanceFromOrigin(pixel, len, RevealCenter);
  return bounceScaleForDistance(distance, len, progress, options.easing, options.dropoffPx);
}

}  // namespace WipeReveal
