/*
	VIA2 — Versatile Interface Adapter (Mac II family)
*/
#pragma once

#include "devices/via_base.h"

// Thin subclass of the shared VIA implementation.
class VIA2Device : public VIABase {
public:
	VIA2Device();
	const char* name() const override { return "VIA2"; }
	const VIAConfig& viaConfig() const override;
};

