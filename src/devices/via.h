/*
	VIA1 — Versatile Interface Adapter (primary)
*/
#pragma once

#include "devices/via_base.h"

// Thin subclass of the shared VIA implementation.
class VIA1Device : public VIABase {
public:
	VIA1Device();
	const char* name() const override { return "VIA1"; }
	const VIAConfig& viaConfig() const override;
};

