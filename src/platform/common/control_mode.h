/*
	CONTROL Mode — declarations
*/

#pragma once

#include "platform/common/osglu_ui.h"

/* --- Special Mode enum --- */

enum {
	SpclModeNoRom,
	SpclModeMessage,
#if UseControlKeys
	SpclModeControl,
#endif

	kNumSpclModes
};

/* --- Special mode variables --- */

extern uint32_t g_specialModes;
extern bool g_needWholeScreenDraw;
extern uint8_t * g_cntrlDisplayBuff;

/* --- Special mode macros --- */

#define SpecialModeSet(i) g_specialModes |= (1 << (i))
#define SpecialModeClr(i) g_specialModes &= ~ (1 << (i))
#define SpecialModeTst(i) (0 != (g_specialModes & (1 << (i))))

#define MacMsgDisplayed SpecialModeTst(SpclModeMessage)

/* --- Keyboard aliases --- */

#define Keyboard_UpdateKeyMap1 Keyboard_UpdateKeyMap
#define DisconnectKeyCodes1 DisconnectKeyCodes

/* --- Function prototypes --- */

uint8_t * GetCurDrawBuff();
uint8_t Keyboard_remapMac(uint8_t key);
void Keyboard_updateKeyMap2(uint8_t key, bool down);
void DisconnectKeyCodes2();
void MacMsgOverride(const char *briefMsg, const char *longMsg);
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
void MacMsgDebugAlert(const char *s);
#endif

void WarnMsgUnsupportedDisk();

/* --- Backend-provided functions --- */
/* These must be defined by each platform backend */

extern void ToggleWantFullScreen();
