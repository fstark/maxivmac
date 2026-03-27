#pragma once

#include "devices/via_base.h"

// VIA2 Device — thin subclass of the shared VIA implementation
class VIA2Device : public VIABase {
public:
	VIA2Device();
	const char* name() const override { return "VIA2"; }
	const VIAConfig& viaConfig() const override;
};

