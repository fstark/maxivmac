#include "core/common.h"
#include "devices/via.h"
#include "core/machine_obj.h"

VIA1Device::VIA1Device()
	: VIABase(1, 0x0400, kICT_VIA1_Timer1Check, kICT_VIA1_Timer2Check)
{}

const VIAConfig& VIA1Device::viaConfig() const {
	return machine_->config().via1Config;
}
