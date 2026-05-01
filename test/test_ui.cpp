/*
	test_ui.cpp — Unit tests for UI math and logic
*/

#include <doctest/doctest.h>
#include "platform/ui_math.h"
#include <cstdint>
#include <algorithm>

/* ── Integer snap tests ──────────────────────────────── */

TEST_CASE("ComputeIntegerSnap: exact 2x")
{
	auto r = ComputeIntegerSnap(1024, 684, 512, 342);
	CHECK(r.scale == 2);
	CHECK(r.width == 1024);
	CHECK(r.height == 684);
}

TEST_CASE("ComputeIntegerSnap: between 1x and 2x rounds to nearest")
{
	/* 700/512 = 1.37 → rounds to 1x; but 800/512 = 1.56 → rounds to 2x */
	auto r1 = ComputeIntegerSnap(700, 500, 512, 342);
	CHECK(r1.scale == 1);
	CHECK(r1.width == 512);
	CHECK(r1.height == 342);

	auto r2 = ComputeIntegerSnap(800, 600, 512, 342);
	CHECK(r2.scale == 2);
	CHECK(r2.width == 1024);
	CHECK(r2.height == 684);
}

TEST_CASE("ComputeIntegerSnap: large window snaps to 3x")
{
	auto r = ComputeIntegerSnap(1600, 1100, 512, 342);
	CHECK(r.scale == 3);
	CHECK(r.width == 1536);
	CHECK(r.height == 1026);
}

TEST_CASE("ComputeIntegerSnap: tiny window clamps to 1x")
{
	auto r = ComputeIntegerSnap(200, 150, 512, 342);
	CHECK(r.scale == 1);
	CHECK(r.width == 512);
	CHECK(r.height == 342);
}

TEST_CASE("ComputeIntegerSnap: non-square guest (MacII 640x480)")
{
	auto r = ComputeIntegerSnap(1300, 1000, 640, 480);
	CHECK(r.scale == 2);
	CHECK(r.width == 1280);
	CHECK(r.height == 960);
}

/* ── Stretched viewport tests ────────────────────────── */

TEST_CASE("ComputeStretchedViewport: wider window → pillarbox")
{
	auto v = ComputeStretchedViewport(1600.0f, 900.0f, 512, 342);
	// 512/342 ≈ 1.497, 1600/900 ≈ 1.778 → window is wider
	// viewH = 900, viewW = 900 * (512/342) ≈ 1347.37
	CHECK(v.h == doctest::Approx(900.0f));
	CHECK(v.w == doctest::Approx(900.0f * 512.0f / 342.0f).epsilon(0.01));
	CHECK(v.x > 0); // pillarbox offset
	CHECK(v.y == doctest::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("ComputeStretchedViewport: taller window → letterbox")
{
	auto v = ComputeStretchedViewport(800.0f, 800.0f, 512, 342);
	// 512/342 ≈ 1.497, 800/800 = 1.0 → window is taller
	// viewW = 800, viewH = 800 / (512/342) ≈ 534.375
	CHECK(v.w == doctest::Approx(800.0f));
	CHECK(v.h == doctest::Approx(800.0f / (512.0f / 342.0f)).epsilon(0.01));
	CHECK(v.x == doctest::Approx(0.0f).epsilon(0.01));
	CHECK(v.y > 0); // letterbox offset
}

TEST_CASE("ComputeStretchedViewport: exact aspect ratio → no bars")
{
	auto v = ComputeStretchedViewport(1024.0f, 684.0f, 512, 342);
	// Aspect matches exactly (2x)
	CHECK(v.w == doctest::Approx(1024.0f).epsilon(1.0));
	CHECK(v.h == doctest::Approx(684.0f).epsilon(1.0));
	CHECK(v.x == doctest::Approx(0.0f).epsilon(1.0));
	CHECK(v.y == doctest::Approx(0.0f).epsilon(1.0));
}

/* ── Speed preset tests ──────────────────────────────── */

static constexpr uint8_t kSpeedPresets[] = {1, 2, 4, 8, 16, 32, 0};
static constexpr int kSpeedPresetCount = 7;

static int findPresetIndex(uint8_t val)
{
	for (int i = 0; i < kSpeedPresetCount; ++i)
		if (kSpeedPresets[i] == val) return i;
	return 0;
}

static uint8_t adjustSpeed(uint8_t current, int delta)
{
	int idx = findPresetIndex(current);
	idx = std::clamp(idx + delta, 0, kSpeedPresetCount - 1);
	return kSpeedPresets[idx];
}

TEST_CASE("Speed: adjustSpeed +1 from 1x → 2x")
{
	CHECK(adjustSpeed(1, +1) == 2);
}

TEST_CASE("Speed: adjustSpeed +1 from 32x → unlimited")
{
	CHECK(adjustSpeed(32, +1) == 0);
}

TEST_CASE("Speed: adjustSpeed -1 from 1x → clamped at 1x")
{
	CHECK(adjustSpeed(1, -1) == 1);
}

TEST_CASE("Speed: adjustSpeed +1 from unlimited → clamped at unlimited")
{
	CHECK(adjustSpeed(0, +1) == 0);
}

TEST_CASE("Speed: adjustSpeed -1 from unlimited → 32x")
{
	CHECK(adjustSpeed(0, -1) == 32);
}

/* ── BGRA → RGBA swizzle test ────────────────────────── */

TEST_CASE("BGRA to RGBA swizzle")
{
	// BGRA pixel: B=0x12, G=0x34, R=0x56, A=0x78
	// In little-endian memory as uint32: 0x78563412
	uint32_t bgra = 0x78563412u;

	uint8_t r = (bgra >> 16) & 0xFF;
	uint8_t g = (bgra >> 8) & 0xFF;
	uint8_t b = (bgra >> 0) & 0xFF;

	CHECK(r == 0x56);
	CHECK(g == 0x34);
	CHECK(b == 0x12);
}
