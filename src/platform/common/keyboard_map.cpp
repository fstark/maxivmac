/*
	keyboard_map.cpp — Keyboard mapping and simple helpers

	Extracted from control_mode.cpp during overlay removal.
	Contains Keyboard_remapMac, Keyboard_updateKeyMap2,
	DisconnectKeyCodes2, ROM_IsValid, and WarnMsgUnsupportedDisk.
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/emulator_shell.h"

#include <cstdio>

/* globals formerly in intl_chars.cpp */
bool g_speedStopped = false;
bool g_runInBackground = false;
bool g_wantFullScreen = false;
bool g_wantMagnify = true;
bool g_requestInsertDisk = false;
uint8_t g_requestIthDisk = 0;
bool g_controlKeyPressed = false;

/* --- Keyboard_remapMac --- */

uint8_t Keyboard_remapMac(uint8_t key)
{
	if constexpr (MKC_formac_Control != MKC_Control) {
		if (key == MKC_Control) return MKC_formac_Control;
	}
	if constexpr (MKC_formac_Command != MKC_Command) {
		if (key == MKC_Command) return MKC_formac_Command;
	}
	if constexpr (MKC_formac_Option != MKC_Option) {
		if (key == MKC_Option) return MKC_formac_Option;
	}
	if constexpr (MKC_formac_Shift != MKC_Shift) {
		if (key == MKC_Shift) return MKC_formac_Shift;
	}
	if constexpr (MKC_formac_CapsLock != MKC_CapsLock) {
		if (key == MKC_CapsLock) return MKC_formac_CapsLock;
	}
	if constexpr (MKC_formac_F1 != MKC_F1) {
		if (key == MKC_F1) return MKC_formac_F1;
	}
	if constexpr (MKC_formac_F2 != MKC_F2) {
		if (key == MKC_F2) return MKC_formac_F2;
	}
	if constexpr (MKC_formac_F3 != MKC_F3) {
		if (key == MKC_F3) return MKC_formac_F3;
	}
	if constexpr (MKC_formac_F4 != MKC_F4) {
		if (key == MKC_F4) return MKC_formac_F4;
	}
	if constexpr (MKC_formac_F5 != MKC_F5) {
		if (key == MKC_F5) return MKC_formac_F5;
	}
	if constexpr (MKC_formac_Escape != MKC_Escape) {
		if (key == MKC_Escape) return MKC_formac_Escape;
	}
	if constexpr (MKC_formac_BackSlash != MKC_BackSlash) {
		if (key == MKC_BackSlash) return MKC_formac_BackSlash;
	}
	if constexpr (MKC_formac_Slash != MKC_Slash) {
		if (key == MKC_Slash) return MKC_formac_Slash;
	}
	if constexpr (MKC_formac_Grave != MKC_Grave) {
		if (key == MKC_Grave) return MKC_formac_Grave;
	}
	if constexpr (MKC_formac_Enter != MKC_Enter) {
		if (key == MKC_Enter) return MKC_formac_Enter;
	}
	if constexpr (MKC_formac_PageUp != MKC_PageUp) {
		if (key == MKC_PageUp) return MKC_formac_PageUp;
	}
	if constexpr (MKC_formac_PageDown != MKC_PageDown) {
		if (key == MKC_PageDown) return MKC_formac_PageDown;
	}
	if constexpr (MKC_formac_Home != MKC_Home) {
		if (key == MKC_Home) return MKC_formac_Home;
	}
	if constexpr (MKC_formac_End != MKC_End) {
		if (key == MKC_End) return MKC_formac_End;
	}
	if constexpr (MKC_formac_Help != MKC_Help) {
		if (key == MKC_Help) return MKC_formac_Help;
	}
	if constexpr (MKC_formac_ForwardDel != MKC_ForwardDel) {
		if (key == MKC_ForwardDel) return MKC_formac_ForwardDel;
	}

	return key;
}

/* --- Keyboard_updateKeyMap2 ---
   With the overlay removed, all keys pass through directly.
   CapsLock was always special-cased; now everything is treated the same. */

void Keyboard_updateKeyMap2(uint8_t key, bool down)
{
	Keyboard_UpdateKeyMap(key, down);
}

void DisconnectKeyCodes2()
{
	DisconnectKeyCodes(kKeepMaskControl | kKeepMaskCapsLock);
}

/* --- ROM_IsValid --- */

tMacErr ROM_IsValid()
{
	g_shell->setRomLoaded(true);
	g_speedStopped = false;
	return tMacErr::noErr;
}

/* --- WarnMsgUnsupportedDisk --- */

void WarnMsgUnsupportedDisk()
{
	fprintf(stderr, "Warning: Unsupported disk image format\n");
}
