#pragma once

#include "WipeRevealTypes.h"

namespace WipeReveal {

uint16_t easedProgress(uint16_t progress, uint8_t ease);
uint16_t revealExtent(uint16_t len, uint8_t pos);
uint16_t distanceFromOrigin(uint16_t pixel, uint16_t len, uint8_t pos);
uint8_t scaleForDistance(uint16_t distance, uint32_t edge, uint16_t dropoff);

}  // namespace WipeReveal
