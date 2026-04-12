/*
	null_keyboard.cpp — stub keyboard functions for headless builds

	Provides no-op implementations of SDL-specific keyboard functions
	that the emulator shell calls. Linked instead of sdl_keyboard.cpp
	when building without SDL.
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/keyboard_map.h"
#include "platform/platform.h"

void ReconnectKeyCodes3() {}

void DisconnectKeyCodes3()
{
	DisconnectKeyCodes2();
	MouseButtonSet(false);
}

void DisableKeyRepeat() {}

void RestoreKeyRepeat() {}
