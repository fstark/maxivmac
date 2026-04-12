/*
	Apple Desktop Bus SHAREd code
	shared by emulation of different implementations of ADB
*/

#pragma once


/*
	ReportAbnormalID unused 0x0D08 - 0x0DFF
*/

#define ADB_MaxSzDatBuf 8

static uint8_t s_adbSzDatBuf;
static bool s_adbTalkDatBuf = false;
static uint8_t s_adbDatBuf[ADB_MaxSzDatBuf];
static uint8_t s_adbCurCmd = 0;
static uint8_t s_notSoRandAddr = 1;

static uint8_t s_mouseADBAddress;
static bool s_savedCurMouseButton = false;
static uint16_t s_mouseADBDeltaH = 0;
static uint16_t s_mouseADBDeltaV = 0;

static void ADB_DoMouseTalk()
{
	switch (s_adbCurCmd & 3)
	{
		case 0:
		{
			EvtQEl *p;
			uint16_t partH;
			uint16_t partV;
			bool overflow = false;
			bool MouseButtonChange = false;

			if (nullptr != (p = EvtQOutP()))
			{
				if (EvtQElKind::MouseDelta == p->kind)
				{
					s_mouseADBDeltaH += p->u.pos.h;
					s_mouseADBDeltaV += p->u.pos.v;
					EvtQOutDone();
				}
			}
			partH = s_mouseADBDeltaH;
			partV = s_mouseADBDeltaV;

			if ((int16_t)s_mouseADBDeltaH < 0)
			{
				partH = -partH;
			}
			if ((int16_t)s_mouseADBDeltaV < 0)
			{
				partV = -partV;
			}
			if ((partH >> 6) > 0)
			{
				overflow = true;
				partH = (1 << 6) - 1;
			}
			if ((partV >> 6) > 0)
			{
				overflow = true;
				partV = (1 << 6) - 1;
			}
			if ((int16_t)s_mouseADBDeltaH < 0)
			{
				partH = -partH;
			}
			if ((int16_t)s_mouseADBDeltaV < 0)
			{
				partV = -partV;
			}
			s_mouseADBDeltaH -= partH;
			s_mouseADBDeltaV -= partV;
			if (!overflow)
			{
				if (nullptr != (p = EvtQOutP()))
				{
					if (EvtQElKind::MouseButton == p->kind)
					{
						s_savedCurMouseButton = p->u.press.down;
						MouseButtonChange = true;
						EvtQOutDone();
					}
				}
			}
			if ((0 != partH) || (0 != partV) || MouseButtonChange)
			{
				s_adbSzDatBuf = 2;
				s_adbTalkDatBuf = true;
				s_adbDatBuf[0] = (s_savedCurMouseButton ? 0x00 : 0x80) | (partV & 127);
				s_adbDatBuf[1] = /* 0x00 */ 0x80 | (partH & 127);
			}
		}
			ADBMouseDisabled = 0;
			break;
		case 3:
			s_adbSzDatBuf = 2;
			s_adbTalkDatBuf = true;
			s_adbDatBuf[0] = 0x60 | (s_notSoRandAddr & 0x0f);
			s_adbDatBuf[1] = 0x01;
			s_notSoRandAddr += 1;
			break;
		default:
			ReportAbnormalID(AbnormalID::kPMU_Talk_to_unknown_mouse_register,
							 "Talk to unknown mouse register");
			break;
	}
}

static void ADB_DoMouseListen()
{
	switch (s_adbCurCmd & 3)
	{
		case 3:
			if (s_adbDatBuf[1] == 0xFE)
			{
				/* change address */
				s_mouseADBAddress = (s_adbDatBuf[0] & 0x0F);
			}
			else
			{
				ReportAbnormalID(AbnormalID::kPMU_unknown_listen_op_to_mouse_register_3,
								 "unknown listen op to mouse register 3");
			}
			break;
		default:
			ReportAbnormalID(AbnormalID::kPMU_listen_to_unknown_mouse_register,
							 "listen to unknown mouse register");
			break;
	}
}

static uint8_t s_keyboardADBAddress;

static bool CheckForADBkeyEvt(uint8_t *NextADBkeyevt)
{
	int i;
	bool KeyDown;

	if (!FindKeyEvent(&i, &KeyDown))
	{
		return false;
	}
	else
	{
#if 0
		if (KeyDown) {
			dbglog_WriteNote("Got a KeyDown");
		}
#endif
		switch (i)
		{
			case MKC_Control:
				i = 0x36;
				break;
			case MKC_Left:
				i = 0x3B;
				break;
			case MKC_Right:
				i = 0x3C;
				break;
			case MKC_Down:
				i = 0x3D;
				break;
			case MKC_Up:
				i = 0x3E;
				break;
			default:
				/* unchanged */
				break;
		}
		*NextADBkeyevt = (KeyDown ? 0x00 : 0x80) | i;
		return true;
	}
}

static void ADB_DoKeyboardTalk()
{
	switch (s_adbCurCmd & 3)
	{
		case 0:
		{
			uint8_t NextADBkeyevt;

			if (CheckForADBkeyEvt(&NextADBkeyevt))
			{
				s_adbSzDatBuf = 2;
				s_adbTalkDatBuf = true;
				s_adbDatBuf[0] = NextADBkeyevt;
				if (!CheckForADBkeyEvt(&NextADBkeyevt))
				{
					s_adbDatBuf[1] = 0xFF;
				}
				else
				{
					s_adbDatBuf[1] = NextADBkeyevt;
				}
			}
		}
		break;
		case 3:
			s_adbSzDatBuf = 2;
			s_adbTalkDatBuf = true;
			s_adbDatBuf[0] = 0x60 | (s_notSoRandAddr & 0x0f);
			s_adbDatBuf[1] = 0x01;
			s_notSoRandAddr += 1;
			break;
		default:
			ReportAbnormalID(AbnormalID::kPMU_Talk_to_unknown_keyboard_register,
							 "Talk to unknown keyboard register");
			break;
	}
}

static void ADB_DoKeyboardListen()
{
	switch (s_adbCurCmd & 3)
	{
		case 3:
			if (s_adbDatBuf[1] == 0xFE)
			{
				/* change address */
				s_keyboardADBAddress = (s_adbDatBuf[0] & 0x0F);
			}
			else
			{
				ReportAbnormalID(AbnormalID::kPMU_unknown_listen_op_to_keyboard_register_3,
								 "unknown listen op to keyboard register 3");
			}
			break;
		default:
			ReportAbnormalID(AbnormalID::kPMU_listen_to_unknown_keyboard_register,
							 "listen to unknown keyboard register");
			break;
	}
}

static bool CheckForADBanyEvt()
{
	EvtQEl *p = EvtQOutP();
	if (nullptr != p)
	{
		switch (p->kind)
		{
			case EvtQElKind::MouseButton:
			case EvtQElKind::MouseDelta:
			case EvtQElKind::Key:
				return true;
				break;
			default:
				break;
		}
	}

	return (0 != s_mouseADBDeltaH) && (0 != s_mouseADBDeltaV);
}

static void ADB_DoTalk()
{
	uint8_t Address = s_adbCurCmd >> 4;
	if (Address == s_mouseADBAddress)
	{
		ADB_DoMouseTalk();
	}
	else if (Address == s_keyboardADBAddress)
	{
		ADB_DoKeyboardTalk();
	}
}

static void ADB_EndListen()
{
	uint8_t Address = s_adbCurCmd >> 4;
	if (Address == s_mouseADBAddress)
	{
		ADB_DoMouseListen();
	}
	else if (Address == s_keyboardADBAddress)
	{
		ADB_DoKeyboardListen();
	}
}

static void ADB_DoReset()
{
	s_mouseADBAddress = 3;
	s_keyboardADBAddress = 2;
}

static void ADB_Flush()
{
	uint8_t Address = s_adbCurCmd >> 4;

	if ((Address == s_keyboardADBAddress) || (Address == s_mouseADBAddress))
	{
		s_adbSzDatBuf = 2;
		s_adbTalkDatBuf = true;
		s_adbDatBuf[0] = 0x00;
		s_adbDatBuf[1] = 0x00;
	}
	else
	{
		ReportAbnormalID(AbnormalID::kPMU_Unhandled_ADB_Flush, "Unhandled ADB Flush");
	}
}
