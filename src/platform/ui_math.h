/*
	ui_math.h — Pure computation helpers for UI layout

	Extracting snap/viewport logic into testable free functions.
*/

#pragma once

#include <algorithm>
#include <cstdint>

struct SnapResult
{
	int scale;
	int width;
	int height;
};

inline SnapResult ComputeIntegerSnap(int newW, int newH, int guestW, int guestH)
{
	int scaleX = std::max(1, newW / guestW);
	int scaleY = std::max(1, newH / guestH);
	int scale = std::min(scaleX, scaleY);
	return {scale, guestW * scale, guestH * scale};
}

struct ViewportRect
{
	float x;
	float y;
	float w;
	float h;
};

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
