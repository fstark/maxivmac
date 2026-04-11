/*
	ALTernate KEYs Mode
*/

#pragma once

static bool s_altKeysLockText = false;
static bool s_altKeysTrueCmnd = false;
static bool s_altKeysTrueOption = false;
static bool s_altKeysTrueShift = false;
static bool s_altKeysModOn = false;
static bool s_altKeysTextOn = false;

static void CheckAltKeyUseMode()
{
	bool NewAltKeysTextOn;

	s_altKeysModOn = s_altKeysTrueCmnd || s_altKeysTrueOption || s_altKeysTrueShift;
	NewAltKeysTextOn = s_altKeysLockText || s_altKeysModOn;
	if (NewAltKeysTextOn != s_altKeysTextOn)
	{
		DisconnectKeyCodes(kKeepMaskControl | kKeepMaskCapsLock |
						   (s_altKeysTrueCmnd ? kKeepMaskCommand : 0) |
						   (s_altKeysTrueOption ? kKeepMaskOption : 0) |
						   (s_altKeysTrueShift ? kKeepMaskShift : 0));
		s_altKeysTextOn = NewAltKeysTextOn;
	}
}

static void Keyboard_UpdateKeyMap1(uint8_t key, bool down)
{
	if (MKC_Command == key)
	{
		s_altKeysTrueCmnd = down;
		CheckAltKeyUseMode();
		Keyboard_UpdateKeyMap(key, down);
	}
	else if (MKC_Option == key)
	{
		s_altKeysTrueOption = down;
		CheckAltKeyUseMode();
		Keyboard_UpdateKeyMap(key, down);
	}
	else if (MKC_Shift == key)
	{
		s_altKeysTrueShift = down;
		CheckAltKeyUseMode();
		Keyboard_UpdateKeyMap(key, down);
	}
	else if (MKC_SemiColon == key)
	{
		if (down && !s_altKeysModOn)
		{
			if (s_altKeysLockText)
			{
				s_altKeysLockText = false;
				g_needWholeScreenDraw = true;
				SpecialModeClr(SpclModeAltKeyText);

				CheckAltKeyUseMode();
			}
		}
		else
		{
			Keyboard_UpdateKeyMap(key, down);
		}
	}
	else if (s_altKeysTextOn)
	{
		Keyboard_UpdateKeyMap(key, down);
	}
	else if (MKC_M == key)
	{
		if (down)
		{
			if (!s_altKeysLockText)
			{
				s_altKeysLockText = true;
				SpecialModeSet(SpclModeAltKeyText);
				g_needWholeScreenDraw = true;
				CheckAltKeyUseMode();
			}
		}
	}
	else
	{
		switch (key)
		{
			case MKC_A:
				key = MKC_SemiColon;
				break;
			case MKC_B:
				key = MKC_BackSlash;
				break;
			case MKC_C:
				key = MKC_F3;
				break;
			case MKC_D:
				key = MKC_Option;
				break;
			case MKC_E:
				key = MKC_BackSpace;
				break;
			case MKC_F:
				key = MKC_Command;
				break;
			case MKC_G:
				key = MKC_Enter;
				break;
			case MKC_H:
				key = MKC_Equal;
				break;
			case MKC_I:
				key = MKC_Up;
				break;
			case MKC_J:
				key = MKC_Left;
				break;
			case MKC_K:
				key = MKC_Down;
				break;
			case MKC_L:
				key = MKC_Right;
				break;
			case MKC_M:
				/* handled above */
				break;
			case MKC_N:
				key = MKC_Minus;
				break;
			case MKC_O:
				key = MKC_RightBracket;
				break;
			case MKC_P:
				return; /* none */
				break;
			case MKC_Q:
				key = MKC_Grave;
				break;
			case MKC_R:
				key = MKC_Return;
				break;
			case MKC_S:
				key = MKC_Shift;
				break;
			case MKC_T:
				key = MKC_Tab;
				break;
			case MKC_U:
				key = MKC_LeftBracket;
				break;
			case MKC_V:
				key = MKC_F4;
				break;
			case MKC_W:
				return; /* none */
				break;
			case MKC_X:
				key = MKC_F2;
				break;
			case MKC_Y:
				key = MKC_Escape;
				break;
			case MKC_Z:
				key = MKC_F1;
				break;
			default:
				break;
		}
		Keyboard_UpdateKeyMap(key, down);
	}
}

static void DisconnectKeyCodes1(uint32_t KeepMask)
{
	DisconnectKeyCodes(KeepMask);

	if (!(0 != (KeepMask & kKeepMaskCommand)))
	{
		s_altKeysTrueCmnd = false;
	}
	if (!(0 != (KeepMask & kKeepMaskOption)))
	{
		s_altKeysTrueOption = false;
	}
	if (!(0 != (KeepMask & kKeepMaskShift)))
	{
		s_altKeysTrueShift = false;
	}
	s_altKeysModOn = s_altKeysTrueCmnd || s_altKeysTrueOption || s_altKeysTrueShift;
	s_altKeysTextOn = s_altKeysLockText || s_altKeysModOn;
}

static void DrawAltKeyMode()
{
	int i;

	CurCellv0 = ControlBoxv0;
	CurCellh0 = ControlBoxh0;

	DrawCellAdvance(kInsertText00);
	for (i = (ControlBoxw - 4) / 2; --i >= 0;)
	{
		DrawCellAdvance(kInsertText04);
	}
	DrawCellAdvance(kInsertText01);
	DrawCellAdvance(kInsertText02);
	for (i = (ControlBoxw - 4) / 2; --i >= 0;)
	{
		DrawCellAdvance(kInsertText04);
	}
	DrawCellAdvance(kInsertText03);
}
