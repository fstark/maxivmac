/*
	keycodes.h

	Mac keyboard scan codes (MKC_*).
	Extracted from platform.h for clarity.
*/

#pragma once

#include <cstdint>

inline constexpr uint8_t MKC_A = 0x00;
inline constexpr uint8_t MKC_B = 0x0B;
inline constexpr uint8_t MKC_C = 0x08;
inline constexpr uint8_t MKC_D = 0x02;
inline constexpr uint8_t MKC_E = 0x0E;
inline constexpr uint8_t MKC_F = 0x03;
inline constexpr uint8_t MKC_G = 0x05;
inline constexpr uint8_t MKC_H = 0x04;
inline constexpr uint8_t MKC_I = 0x22;
inline constexpr uint8_t MKC_J = 0x26;
inline constexpr uint8_t MKC_K = 0x28;
inline constexpr uint8_t MKC_L = 0x25;
inline constexpr uint8_t MKC_M = 0x2E;
inline constexpr uint8_t MKC_N = 0x2D;
inline constexpr uint8_t MKC_O = 0x1F;
inline constexpr uint8_t MKC_P = 0x23;
inline constexpr uint8_t MKC_Q = 0x0C;
inline constexpr uint8_t MKC_R = 0x0F;
inline constexpr uint8_t MKC_S = 0x01;
inline constexpr uint8_t MKC_T = 0x11;
inline constexpr uint8_t MKC_U = 0x20;
inline constexpr uint8_t MKC_V = 0x09;
inline constexpr uint8_t MKC_W = 0x0D;
inline constexpr uint8_t MKC_X = 0x07;
inline constexpr uint8_t MKC_Y = 0x10;
inline constexpr uint8_t MKC_Z = 0x06;

inline constexpr uint8_t MKC_1 = 0x12;
inline constexpr uint8_t MKC_2 = 0x13;
inline constexpr uint8_t MKC_3 = 0x14;
inline constexpr uint8_t MKC_4 = 0x15;
inline constexpr uint8_t MKC_5 = 0x17;
inline constexpr uint8_t MKC_6 = 0x16;
inline constexpr uint8_t MKC_7 = 0x1A;
inline constexpr uint8_t MKC_8 = 0x1C;
inline constexpr uint8_t MKC_9 = 0x19;
inline constexpr uint8_t MKC_0 = 0x1D;

inline constexpr uint8_t MKC_Command = 0x37;
inline constexpr uint8_t MKC_Shift = 0x38;
inline constexpr uint8_t MKC_CapsLock = 0x39;
inline constexpr uint8_t MKC_Option = 0x3A;

inline constexpr uint8_t MKC_Space = 0x31;
inline constexpr uint8_t MKC_Return = 0x24;
inline constexpr uint8_t MKC_BackSpace = 0x33;
inline constexpr uint8_t MKC_Tab = 0x30;

inline constexpr uint8_t MKC_Left = /* 0x46 */ 0x7B;
inline constexpr uint8_t MKC_Right = /* 0x42 */ 0x7C;
inline constexpr uint8_t MKC_Down = /* 0x48 */ 0x7D;
inline constexpr uint8_t MKC_Up = /* 0x4D */ 0x7E;

inline constexpr uint8_t MKC_Minus = 0x1B;
inline constexpr uint8_t MKC_Equal = 0x18;
inline constexpr uint8_t MKC_BackSlash = 0x2A;
inline constexpr uint8_t MKC_Comma = 0x2B;
inline constexpr uint8_t MKC_Period = 0x2F;
inline constexpr uint8_t MKC_Slash = 0x2C;
inline constexpr uint8_t MKC_SemiColon = 0x29;
inline constexpr uint8_t MKC_SingleQuote = 0x27;
inline constexpr uint8_t MKC_LeftBracket = 0x21;
inline constexpr uint8_t MKC_RightBracket = 0x1E;
inline constexpr uint8_t MKC_Grave = 0x32;
inline constexpr uint8_t MKC_Clear = 0x47;
inline constexpr uint8_t MKC_KPEqual = 0x51;
inline constexpr uint8_t MKC_KPDevide = 0x4B;
inline constexpr uint8_t MKC_KPMultiply = 0x43;
inline constexpr uint8_t MKC_KPSubtract = 0x4E;
inline constexpr uint8_t MKC_KPAdd = 0x45;
inline constexpr uint8_t MKC_Enter = 0x4C;

inline constexpr uint8_t MKC_KP1 = 0x53;
inline constexpr uint8_t MKC_KP2 = 0x54;
inline constexpr uint8_t MKC_KP3 = 0x55;
inline constexpr uint8_t MKC_KP4 = 0x56;
inline constexpr uint8_t MKC_KP5 = 0x57;
inline constexpr uint8_t MKC_KP6 = 0x58;
inline constexpr uint8_t MKC_KP7 = 0x59;
inline constexpr uint8_t MKC_KP8 = 0x5B;
inline constexpr uint8_t MKC_KP9 = 0x5C;
inline constexpr uint8_t MKC_KP0 = 0x52;
inline constexpr uint8_t MKC_Decimal = 0x41;

/* these aren't on the Mac Plus keyboard */

inline constexpr uint8_t MKC_Control = 0x3B;
inline constexpr uint8_t MKC_Escape = 0x35;
inline constexpr uint8_t MKC_F1 = 0x7a;
inline constexpr uint8_t MKC_F2 = 0x78;
inline constexpr uint8_t MKC_F3 = 0x63;
inline constexpr uint8_t MKC_F4 = 0x76;
inline constexpr uint8_t MKC_F5 = 0x60;
inline constexpr uint8_t MKC_F6 = 0x61;
inline constexpr uint8_t MKC_F7 = 0x62;
inline constexpr uint8_t MKC_F8 = 0x64;
inline constexpr uint8_t MKC_F9 = 0x65;
inline constexpr uint8_t MKC_F10 = 0x6d;
inline constexpr uint8_t MKC_F11 = 0x67;
inline constexpr uint8_t MKC_F12 = 0x6f;

inline constexpr uint8_t MKC_Home = 0x73;
inline constexpr uint8_t MKC_End = 0x77;
inline constexpr uint8_t MKC_PageUp = 0x74;
inline constexpr uint8_t MKC_PageDown = 0x79;
inline constexpr uint8_t MKC_Help = 0x72 /* = Insert */;
inline constexpr uint8_t MKC_ForwardDel = 0x75;
inline constexpr uint8_t MKC_Print = 0x69;
inline constexpr uint8_t MKC_ScrollLock = 0x6B;
inline constexpr uint8_t MKC_Pause = 0x71;

inline constexpr uint8_t MKC_AngleBracket = 0x0A /* found on german keyboard */;

/*
	Additional codes found in Apple headers

	#define MKC_RightShift 0x3C
	#define MKC_RightOption 0x3D
	#define MKC_RightControl 0x3E
	#define MKC_Function 0x3F

	#define MKC_VolumeUp 0x48
	#define MKC_VolumeDown 0x49
	#define MKC_Mute 0x4A

	#define MKC_F16 0x6A
	#define MKC_F17 0x40
	#define MKC_F18 0x4F
	#define MKC_F19 0x50
	#define MKC_F20 0x5A

	#define MKC_F13 MKC_Print
	#define MKC_F14 MKC_ScrollLock
	#define MKC_F15 MKC_Pause
*/

/* not Apple key codes, only for Mini vMac */

inline constexpr uint8_t MKC_CM = 0x80;
inline constexpr uint8_t MKC_real_CapsLock = 0x81;
	/*
		for use in platform specific code
		when CapsLocks need special handling.
	*/
inline constexpr uint8_t MKC_None = 0xFF;
