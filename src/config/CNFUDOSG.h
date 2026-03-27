/*
	OS-glue display/input configuration.
*/

#pragma once

#include "platform/keycodes.h"

#define SaveDialogEnable 1
#define EnableAltKeysMode 0
inline constexpr uint8_t MKC_formac_Control = MKC_CM;
inline constexpr uint8_t MKC_formac_Command = MKC_Command;
inline constexpr uint8_t MKC_formac_Option = MKC_Option;
inline constexpr uint8_t MKC_formac_Shift = MKC_Shift;
inline constexpr uint8_t MKC_formac_CapsLock = MKC_CapsLock;
inline constexpr uint8_t MKC_formac_Escape = MKC_Escape;
inline constexpr uint8_t MKC_formac_BackSlash = MKC_BackSlash;
inline constexpr uint8_t MKC_formac_Slash = MKC_Slash;
inline constexpr uint8_t MKC_formac_Grave = MKC_Grave;
inline constexpr uint8_t MKC_formac_Enter = MKC_Enter;
inline constexpr uint8_t MKC_formac_PageUp = MKC_PageUp;
inline constexpr uint8_t MKC_formac_PageDown = MKC_PageDown;
inline constexpr uint8_t MKC_formac_Home = MKC_Home;
inline constexpr uint8_t MKC_formac_End = MKC_End;
inline constexpr uint8_t MKC_formac_Help = MKC_Help;
inline constexpr uint8_t MKC_formac_ForwardDel = MKC_ForwardDel;
inline constexpr uint8_t MKC_formac_F1 = MKC_Option;
inline constexpr uint8_t MKC_formac_F2 = MKC_Command;
inline constexpr uint8_t MKC_formac_F3 = MKC_F3;
inline constexpr uint8_t MKC_formac_F4 = MKC_F4;
inline constexpr uint8_t MKC_formac_F5 = MKC_F5;
inline constexpr uint8_t MKC_formac_RControl = MKC_CM;
inline constexpr uint8_t MKC_formac_RCommand = MKC_Command;
inline constexpr uint8_t MKC_formac_ROption = MKC_Option;
inline constexpr uint8_t MKC_formac_RShift = MKC_Shift;
inline constexpr uint8_t MKC_UnMappedKey = MKC_Control;
#define WantInitRunInBackground 0
#define WantEnblCtrlInt 1
#define WantEnblCtrlRst 1
#define WantEnblCtrlKtg 1
#define UseControlKeys 1

/* version and other info to display to user */

#define NeedIntlChars 0

#ifndef MAXIVMAC_VERSION
#define MAXIVMAC_VERSION "dev-unknown"
#endif

#define kBldOpts "maxivmac " MAXIVMAC_VERSION
