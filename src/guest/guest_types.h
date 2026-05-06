/*
	guest_types.h — Shared types for guest memory introspection.
*/
#pragma once
#include <cstdint>

namespace guest
{

/// Address in the 68k guest address space.
using GuestAddr = uint32_t;

struct Rect
{
	int16_t top, left, bottom, right;
	int16_t centerH() const { return (left + right) / 2; }
	int16_t centerV() const { return (top + bottom) / 2; }
};

struct Point
{
	int16_t h, v;
};

} // namespace guest
