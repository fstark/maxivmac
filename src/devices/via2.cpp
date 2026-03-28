#include "core/common.h"
#include "devices/via2.h"
#include "core/machine_obj.h"
#include "core/abnormal_ids.h"

VIA2Device::VIA2Device()
	: VIABase(2, AbnormalID::kVIA2_Base, kICT_VIA2_Timer1Check, kICT_VIA2_Timer2Check)
{}

const VIAConfig& VIA2Device::viaConfig() const {
	return machine_->config().via2Config;
}
