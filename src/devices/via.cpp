/*
	VIA1 — Versatile Interface Adapter (primary) implementation
*/

#include "core/common.h"
#include "devices/via.h"
#include "core/machine_obj.h"
#include "core/abnormal_ids.h"

VIA1Device::VIA1Device()
	: VIABase(1, AbnormalID::kVIA1_Base, kICT_VIA1_Timer1Check, kICT_VIA1_Timer2Check)
{}

const VIAConfig& VIA1Device::viaConfig() const {
	return machine_->config().via1Config;
}
