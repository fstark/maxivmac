/*
	Operating System Glue COMmon includes, User options Dependent

	Second include of OSGLUxxx files. All things in common
	that can not go in OSGLCMUI, because they depend on user options.
*/

#pragma once

#include "CNFUDOSG.h"
	/*
		Configuration file dependent on user options
		for operating system glue.
	*/
#include "CNFUDALL.h"
	/*
		Configuration file dependent on user options
		for all code.
	*/
#include "platform/platform.h"

#include "lang/localization_keys.h"
#include "lang/localization.h"
