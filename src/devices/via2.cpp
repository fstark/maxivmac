#include "core/common.h"
#include "devices/via2.h"
#include "core/machine_obj.h"

VIA2Device::VIA2Device()
	: VIABase(2, 0x0500, kICT_VIA2_Timer1Check, kICT_VIA2_Timer2Check)
{}

const VIAConfig& VIA2Device::viaConfig() const {
	return machine_->config().via2Config;
}
