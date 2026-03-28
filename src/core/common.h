/*
	Platform Idendependent code COMMON

	First include of platform idendependent code files,
		containing definitions used by all of them.

	May be worthwhile to create a pre-compiled header from this,
		if supported by compiler, which can be used for all
		platform idendependent files of a given Variation.
*/

#pragma once

#include "core/types.h"
	/* see OSGCOMUI.h for comment */
#include "core/defaults.h"
	/* see OSGCOMUI.h for comment */
#include "core/endian.h"
#include "CNFUDALL.h"
	/* see OSGCOMUD.h for comment */
#include "platform/platform.h"
#include "core/emulation_config.h"
	/*
		Configuration file dependent on user options
		suitable for platform indendent code.
	*/
#include "core/machine.h"
