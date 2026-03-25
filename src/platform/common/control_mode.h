/*
	control_mode.h

	Copyright (C) 2007 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	CONTROL Mode — declarations
*/

#pragma once

#include "platform/common/osglu_ui.h"

/* --- Special Mode enum --- */

enum {
#if EnableAltKeysMode
	SpclModeAltKeyText,
#endif
	SpclModeNoRom,
	SpclModeMessage,
#if UseControlKeys
	SpclModeControl,
#endif

	kNumSpclModes
};

/* --- Special mode variables --- */

extern uint32_t SpecialModes;
extern bool NeedWholeScreenDraw;
extern uint8_t * CntrlDisplayBuff;

/* --- Special mode macros --- */

#define SpecialModeSet(i) SpecialModes |= (1 << (i))
#define SpecialModeClr(i) SpecialModes &= ~ (1 << (i))
#define SpecialModeTst(i) (0 != (SpecialModes & (1 << (i))))

#define MacMsgDisplayed SpecialModeTst(SpclModeMessage)

/* --- Keyboard aliases --- */

#if EnableAltKeysMode
/* When alt-keys mode is enabled, alt_keys.h provides the aliases */
#else
#define Keyboard_UpdateKeyMap1 Keyboard_UpdateKeyMap
#define DisconnectKeyCodes1 DisconnectKeyCodes
#endif

/* --- Function prototypes --- */

uint8_t * GetCurDrawBuff();
uint8_t Keyboard_RemapMac(uint8_t key);
void Keyboard_UpdateKeyMap2(uint8_t key, bool down);
void DisconnectKeyCodes2();
void MacMsgOverride(char *briefMsg, char *longMsg);
void MacMsgDisplayOn();
void MacMsgDisplayOff();
tMacErr ROM_IsValid();
bool WaitForRom();

#if NeedDoMoreCommandsMsg
void DoMoreCommandsMsg();
#endif

#if NeedDoAboutMsg
void DoAboutMsg();
#endif

#if dbglog_HAVE
void MacMsgDebugAlert(char *s);
#endif

#if NonDiskProtect
void WarnMsgUnsupportedDisk();
#endif

/* --- Backend-provided functions --- */
/* These must be defined by each platform backend */

#if VarFullScreen
extern void ToggleWantFullScreen();
#endif
