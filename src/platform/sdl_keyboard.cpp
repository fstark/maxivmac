#include "platform/sdl_keyboard.h"

#include <array>

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/keyboard_map.h"

static constexpr auto BuildScanToMkcTable()
{
	std::array<uint8_t, SDL_SCANCODE_COUNT> t{};
	t.fill(MKC_None);

	/* Typing keys */
	t[SDL_SCANCODE_BACKSPACE] = MKC_BackSpace;
	t[SDL_SCANCODE_TAB] = MKC_Tab;
	t[SDL_SCANCODE_CLEAR] = MKC_Clear;
	t[SDL_SCANCODE_RETURN] = MKC_Return;
	t[SDL_SCANCODE_PAUSE] = MKC_Pause;
	t[SDL_SCANCODE_ESCAPE] = MKC_formac_Escape;
	t[SDL_SCANCODE_SPACE] = MKC_Space;
	t[SDL_SCANCODE_APOSTROPHE] = MKC_SingleQuote;
	t[SDL_SCANCODE_COMMA] = MKC_Comma;
	t[SDL_SCANCODE_MINUS] = MKC_Minus;
	t[SDL_SCANCODE_PERIOD] = MKC_Period;
	t[SDL_SCANCODE_SLASH] = MKC_formac_Slash;
	t[SDL_SCANCODE_SEMICOLON] = MKC_SemiColon;
	t[SDL_SCANCODE_EQUALS] = MKC_Equal;
	t[SDL_SCANCODE_LEFTBRACKET] = MKC_LeftBracket;
	t[SDL_SCANCODE_BACKSLASH] = MKC_formac_BackSlash;
	t[SDL_SCANCODE_RIGHTBRACKET] = MKC_RightBracket;
	t[SDL_SCANCODE_GRAVE] = MKC_formac_Grave;

	/* Digits */
	t[SDL_SCANCODE_0] = MKC_0;
	t[SDL_SCANCODE_1] = MKC_1;
	t[SDL_SCANCODE_2] = MKC_2;
	t[SDL_SCANCODE_3] = MKC_3;
	t[SDL_SCANCODE_4] = MKC_4;
	t[SDL_SCANCODE_5] = MKC_5;
	t[SDL_SCANCODE_6] = MKC_6;
	t[SDL_SCANCODE_7] = MKC_7;
	t[SDL_SCANCODE_8] = MKC_8;
	t[SDL_SCANCODE_9] = MKC_9;

	/* Letters */
	t[SDL_SCANCODE_A] = MKC_A;
	t[SDL_SCANCODE_B] = MKC_B;
	t[SDL_SCANCODE_C] = MKC_C;
	t[SDL_SCANCODE_D] = MKC_D;
	t[SDL_SCANCODE_E] = MKC_E;
	t[SDL_SCANCODE_F] = MKC_F;
	t[SDL_SCANCODE_G] = MKC_G;
	t[SDL_SCANCODE_H] = MKC_H;
	t[SDL_SCANCODE_I] = MKC_I;
	t[SDL_SCANCODE_J] = MKC_J;
	t[SDL_SCANCODE_K] = MKC_K;
	t[SDL_SCANCODE_L] = MKC_L;
	t[SDL_SCANCODE_M] = MKC_M;
	t[SDL_SCANCODE_N] = MKC_N;
	t[SDL_SCANCODE_O] = MKC_O;
	t[SDL_SCANCODE_P] = MKC_P;
	t[SDL_SCANCODE_Q] = MKC_Q;
	t[SDL_SCANCODE_R] = MKC_R;
	t[SDL_SCANCODE_S] = MKC_S;
	t[SDL_SCANCODE_T] = MKC_T;
	t[SDL_SCANCODE_U] = MKC_U;
	t[SDL_SCANCODE_V] = MKC_V;
	t[SDL_SCANCODE_W] = MKC_W;
	t[SDL_SCANCODE_X] = MKC_X;
	t[SDL_SCANCODE_Y] = MKC_Y;
	t[SDL_SCANCODE_Z] = MKC_Z;

	/* Keypad digits */
	t[SDL_SCANCODE_KP_0] = MKC_KP0;
	t[SDL_SCANCODE_KP_1] = MKC_KP1;
	t[SDL_SCANCODE_KP_2] = MKC_KP2;
	t[SDL_SCANCODE_KP_3] = MKC_KP3;
	t[SDL_SCANCODE_KP_4] = MKC_KP4;
	t[SDL_SCANCODE_KP_5] = MKC_KP5;
	t[SDL_SCANCODE_KP_6] = MKC_KP6;
	t[SDL_SCANCODE_KP_7] = MKC_KP7;
	t[SDL_SCANCODE_KP_8] = MKC_KP8;
	t[SDL_SCANCODE_KP_9] = MKC_KP9;

	/* Keypad operators */
	t[SDL_SCANCODE_KP_PERIOD] = MKC_Decimal;
	t[SDL_SCANCODE_KP_DIVIDE] = MKC_KPDevide;
	t[SDL_SCANCODE_KP_MULTIPLY] = MKC_KPMultiply;
	t[SDL_SCANCODE_KP_MINUS] = MKC_KPSubtract;
	t[SDL_SCANCODE_KP_PLUS] = MKC_KPAdd;
	t[SDL_SCANCODE_KP_ENTER] = MKC_formac_Enter;
	t[SDL_SCANCODE_KP_EQUALS] = MKC_KPEqual;

	/* Arrow keys */
	t[SDL_SCANCODE_UP] = MKC_Up;
	t[SDL_SCANCODE_DOWN] = MKC_Down;
	t[SDL_SCANCODE_RIGHT] = MKC_Right;
	t[SDL_SCANCODE_LEFT] = MKC_Left;

	/* Navigation */
	t[SDL_SCANCODE_INSERT] = MKC_formac_Help;
	t[SDL_SCANCODE_HOME] = MKC_formac_Home;
	t[SDL_SCANCODE_END] = MKC_formac_End;
	t[SDL_SCANCODE_PAGEUP] = MKC_formac_PageUp;
	t[SDL_SCANCODE_PAGEDOWN] = MKC_formac_PageDown;

	/* Function keys */
	t[SDL_SCANCODE_F1] = MKC_formac_F1;
	t[SDL_SCANCODE_F2] = MKC_formac_F2;
	t[SDL_SCANCODE_F3] = MKC_formac_F3;
	t[SDL_SCANCODE_F4] = MKC_formac_F4;
	t[SDL_SCANCODE_F5] = MKC_formac_F5;
	t[SDL_SCANCODE_F6] = MKC_F6;
	t[SDL_SCANCODE_F7] = MKC_F7;
	t[SDL_SCANCODE_F8] = MKC_F8;
	t[SDL_SCANCODE_F9] = MKC_F9;
	t[SDL_SCANCODE_F10] = MKC_F10;
	t[SDL_SCANCODE_F11] = MKC_F11;
	t[SDL_SCANCODE_F12] = MKC_F12;

	/* Modifier keys */
	t[SDL_SCANCODE_NUMLOCKCLEAR] = MKC_formac_ForwardDel;
	t[SDL_SCANCODE_CAPSLOCK] = MKC_formac_CapsLock;
	t[SDL_SCANCODE_SCROLLLOCK] = MKC_ScrollLock;
	t[SDL_SCANCODE_RSHIFT] = MKC_formac_RShift;
	t[SDL_SCANCODE_LSHIFT] = MKC_formac_Shift;
	t[SDL_SCANCODE_RCTRL] = MKC_formac_RControl;
	t[SDL_SCANCODE_LCTRL] = MKC_formac_Control;
	t[SDL_SCANCODE_RALT] = MKC_formac_ROption;
	t[SDL_SCANCODE_LALT] = MKC_formac_Option;
	t[SDL_SCANCODE_RGUI] = MKC_formac_RCommand;
	t[SDL_SCANCODE_LGUI] = MKC_formac_Command;

	/* Misc */
	t[SDL_SCANCODE_HELP] = MKC_formac_Help;
	t[SDL_SCANCODE_PRINTSCREEN] = MKC_Print;

	/* Editing keys mapped to function keys */
	t[SDL_SCANCODE_UNDO] = MKC_formac_F1;
	t[SDL_SCANCODE_CUT] = MKC_formac_F2;
	t[SDL_SCANCODE_COPY] = MKC_formac_F3;
	t[SDL_SCANCODE_PASTE] = MKC_formac_F4;

	t[SDL_SCANCODE_AC_HOME] = MKC_formac_Home;

	/* Keypad hex letters */
	t[SDL_SCANCODE_KP_A] = MKC_A;
	t[SDL_SCANCODE_KP_B] = MKC_B;
	t[SDL_SCANCODE_KP_C] = MKC_C;
	t[SDL_SCANCODE_KP_D] = MKC_D;
	t[SDL_SCANCODE_KP_E] = MKC_E;
	t[SDL_SCANCODE_KP_F] = MKC_F;

	/* Keypad extras */
	t[SDL_SCANCODE_KP_BACKSPACE] = MKC_BackSpace;
	t[SDL_SCANCODE_KP_CLEAR] = MKC_Clear;
	t[SDL_SCANCODE_KP_COMMA] = MKC_Comma;
	t[SDL_SCANCODE_KP_DECIMAL] = MKC_Decimal;

	return t;
}

static constexpr auto s_scanToMkc = BuildScanToMkcTable();

uint8_t SDLScan2MacKeyCode(SDL_Scancode i)
{
	auto idx = static_cast<unsigned>(i);
	if (idx >= s_scanToMkc.size()) return MKC_None;
	return s_scanToMkc[idx];
}

void DisableKeyRepeat() {}

void RestoreKeyRepeat() {}

void ReconnectKeyCodes3() {}

void DisconnectKeyCodes3()
{
	DisconnectKeyCodes2();
	MouseButtonSet(false);
}
