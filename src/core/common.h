/*
	Platform Idendependent code COMMON

	First include of platform idendependent code files,
		containing definitions used by all of them.

	May be worthwhile to create a pre-compiled header from this,
		if supported by compiler, which can be used for all
		platform idendependent files of a given Variation.
*/

#pragma once

#include "CNFUIALL.h"
	/* see OSGCOMUI.h for comment */
#include "CNFUIPIC.h"
	/*
		Configuration file independent of user options
		suitable for platform indendent code.
		Usually empty, but if use different compiler for
		operating system glue, then could define the different compiler
		configuration here and in CNFUIOSG, instead of CNFUIALL.
	*/
#include "core/defaults.h"
	/* see OSGCOMUI.h for comment */
#include "core/endian.h"
#include "CNFUDALL.h"
	/* see OSGCOMUD.h for comment */
#include "platform/platform.h"
#include "CNFUDPIC.h"
	/*
		Configuration file dependent on user options
		suitable for platform indendent code.
	*/
#include "core/machine.h"
