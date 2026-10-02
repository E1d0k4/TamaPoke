#pragma once
// Eevee Jump - first real sprite frame extracted from the supplied Eevee sheet.
// Original TamaPoke sprite system remains untouched.

#include <stdint.h>

static constexpr uint16_t TD_EEVEE_RUN0_W = 21;
static constexpr uint16_t TD_EEVEE_RUN0_H = 19;

// Palette is exact RGB565 data from the supplied sprite sheet.
// Index 0 is the sheet background and is transparent when drawn.
static constexpr uint16_t TD_EEVEE_RUN0_PALETTE[] = {
  0x03F2, 0x0000, 0xDBA3, 0xA2E3, 0x71E0,
  0xD5EC, 0xB424, 0xFFB3, 0xE1EC, 0xFC75
};

static constexpr uint8_t TD_EEVEE_RUN0_PIXELS[] = {
  0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,1,4,2,3,1,0,0,0,0,0,0,0,0,0,
  0,0,0,1,1,1,1,3,2,1,1,1,1,1,0,0,0,0,0,0,0,
  0,0,1,3,2,2,3,4,1,4,3,2,2,3,1,0,0,0,0,0,0,
  0,1,3,2,2,2,2,3,2,2,2,3,4,1,0,0,0,0,0,0,0,
  0,1,2,2,3,2,2,3,3,4,1,1,1,0,0,0,0,0,0,0,0,
  1,3,2,3,2,2,2,3,4,1,0,0,0,0,0,0,0,0,0,0,0,
  1,2,2,1,4,2,3,3,4,1,1,1,0,0,0,0,0,0,0,0,0,
  1,8,8,2,2,2,3,4,1,6,5,6,1,0,0,0,0,0,0,0,0,
  0,1,9,2,2,3,4,6,5,6,7,5,6,1,1,0,0,0,0,0,0,
  0,1,1,4,4,4,6,5,7,6,5,7,5,1,4,1,1,1,1,0,0,
  0,1,6,5,7,5,6,7,7,5,6,5,4,3,3,4,3,3,4,1,0,
  0,1,5,7,7,6,5,7,5,5,4,4,3,2,3,3,2,2,2,3,1,
  0,0,1,5,6,4,3,6,5,6,1,3,2,2,4,6,2,2,2,2,1,
  0,0,0,1,1,2,3,1,1,1,4,1,2,3,1,5,5,2,2,2,1,
  0,0,0,1,2,3,1,0,0,1,1,2,3,1,1,7,7,5,2,3,1,
  0,0,0,0,1,1,0,0,0,0,0,1,1,1,6,7,7,7,6,1,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,1,7,7,5,6,1,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0
};
