/*
	Operating System Glue COMmon includes, User options Independent

	First include of OSGLUxxx files. Things in common that are not
		affected by user selected options.
		(but are affected by developer selected options)

	May be worthwhile to create a pre-compiled header from this,
		if supported by compiler, which can be used for all
		Variations of a given port.
*/

#pragma once


#include "platform/platform_defs.h"
/*
	Configuration file independent of user options
	for operating system glue.
	Application metadata and standard library includes.
*/
#include "core/types.h"
/*
	Configuration file independent of user options
	for all code.
	In particular, configuration for current compiler.
*/
#include "core/defaults.h"
/*
	Default configuration of compiler
	If options for compiler haven't been defined in any
	configuration files, they are defined here.
*/
#include "core/endian.h"
