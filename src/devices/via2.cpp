/*
	VIA2 — Versatile Interface Adapter (Mac II family) implementation
*/

#include "core/common.h"
#include "devices/via2.h"
#include "core/rig.h"
#include "core/abnormal_ids.h"

VIA2Device::VIA2Device()
	: VIABase(2, AbnormalID::kVIA2_Base, kICT_VIA2_Timer1Check, kICT_VIA2_Timer2Check)
{
}

const VIAConfig &VIA2Device::viaConfig() const
{
	return rig_->config().via2Config;
}
