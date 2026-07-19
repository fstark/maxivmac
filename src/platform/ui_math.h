/*
	ui_math.h — Pure computation helpers for UI layout

	Testable free functions for the integer-snap and viewport calculations
	used by the ImGui backend.  Keeping these out of ImGuiBackend makes
	them easy to unit-test without pulling in SDL or OpenGL.
*/

#pragma once

#include <algorithm>
#include <cstdint>

/* Result of an integer-snap computation. */
struct SnapResult
{
	int scale;
	int width;
	int height;
};

/* Given a proposed window size (newW × newH) and the guest framebuffer
   dimensions (guestW × guestH), compute the largest integer scale that
   fits, rounding to the nearest.  maxScale caps the result (e.g. to
   avoid exceeding the usable display area). */
inline SnapResult ComputeIntegerSnap(int newW, int newH, int guestW, int guestH,
									 int maxScale = 999)
{
	int scaleX = std::max(1, (newW + guestW / 2) / guestW);
	int scaleY = std::max(1, (newH + guestH / 2) / guestH);
	int scale = std::min({scaleX, scaleY, maxScale});
	return {scale, guestW * scale, guestH * scale};
}

/* A positioned rectangle for viewport layout. */
struct ViewportRect
{
	float x;
	float y;
	float w;
	float h;
};

/* Compute the largest aspect-ratio-preserving viewport that fits
   inside a window of size winW × winH for a guest of guestW × guestH.
   Returns the viewport rectangle centered in the window. */
inline ViewportRect ComputeStretchedViewport(float winW, float winH, int guestW, int guestH)
{
	float emuAspect = static_cast<float>(guestW) / guestH;
	float winAspect = winW / winH;
	float viewW, viewH;
	if (emuAspect > winAspect)
	{
		viewW = winW;
		viewH = winW / emuAspect;
	}
	else
	{
		viewH = winH;
		viewW = winH * emuAspect;
	}
	float offsetX = (winW - viewW) * 0.5f;
	float offsetY = (winH - viewH) * 0.5f;
	return {offsetX, offsetY, viewW, viewH};
}
