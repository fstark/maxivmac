#pragma once

#include <cstdint>
#include "platform/platform.h"

/* MacRoman <-> UTF-8 conversion utilities. */

uint32_t MacRoman2UniCodeSize(uint8_t *s, uint32_t L);
void MacRoman2UniCodeData(uint8_t *s, uint32_t L, char *t);

tMacErr UniCodeStrLength(char *s, uint32_t *r);
uint8_t UniCodePoint2MacRoman(uint32_t x);
void UniCodeStr2MacRoman(char *s, char *r);
