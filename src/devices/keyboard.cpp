/*
	KBRDEMDV.c

	Copyright (C) 2006 Philip Cummins, Paul C. Pratt

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
	KeyBoaRD EMulated DeVice

	Emulation of the keyboard in the Mac Plus.

	This code adapted from "Keyboard.c" in vMac by Philip Cummins.
*/

#include "core/common.h"

#include "devices/keyboard.h"
#include "devices/via.h"
#include "core/wire_bus.h"
#include "core/wire_ids.h"
#include "core/machine_obj.h"

/* Global singleton */

#ifdef _VIA_Debug
#include <stdio.h>
#endif

/*
	ReportAbnormalID unused 0x0B03 - 0x0BFF
*/

VIA1Device* KeyboardDevice::via1() const
{
	return machine_->findDevice<VIA1Device>();
}

void KeyboardDevice::reset()
{
	kybdState_ = kKybdStateIdle;
	haveKeyBoardResult_ = false;
	keyBoardResult_ = 0;
	instantCommandData_ = 0x7B;
	inquiryCommandTimer_ = 0;
}

void KeyboardDevice::gotKeyBoardData(uint8_t v)
{
	if (kybdState_ != kKybdStateIdle) {
		haveKeyBoardResult_ = true;
		keyBoardResult_ = v;
	} else {
		via1()->shiftInData(v);
		g_wires.set(Wire_VIA1_iCB2, 1);
	}
}

bool KeyboardDevice::attemptToFinishInquiry()
{
	int i;
	bool KeyDown;
	uint8_t Keyboard_Data;

	if (FindKeyEvent(&i, &KeyDown)) {
		if (i < 64) {
			Keyboard_Data = i << 1;
			if (! KeyDown) {
				Keyboard_Data += 128;
			}
		} else {
			Keyboard_Data = 121;
			instantCommandData_ = (i - 64) << 1;
			if (! KeyDown) {
				instantCommandData_ += 128;
			}
		}
		gotKeyBoardData(Keyboard_Data);
		return true;
	} else {
		return false;
	}
}

#define MaxKeyboardWait 16 /* in 60ths of a second */
	/*
		Code in the mac rom will reset the keyboard if
		it hasn't been heard from in 32/60th of a second.
		So time out and send something before that
		to keep connection.
	*/

void KeyboardDevice::receiveCommand()
{
	if (kybdState_ != kKybdStateRecievingCommand) {
		ReportAbnormalID(0x0B01,
			"KybdState != kKybdStateRecievingCommand");
	} else {
		uint8_t in = via1()->shiftOutData();

		kybdState_ = kKybdStateRecievedCommand;

		switch (in) {
			case 0x10 : /* Inquiry Command */
				if (! attemptToFinishInquiry()) {
					inquiryCommandTimer_ = MaxKeyboardWait;
				}
				break;
			case 0x14 : /* Instant Command */
				gotKeyBoardData(instantCommandData_);
				instantCommandData_ = 0x7B;
				break;
			case 0x16 : /* Model Command */
				gotKeyBoardData(0x0b /* 0x01 */);
					/* Test value, means Model 0, no extra devices */
				/*
					Fixed by Hoshi Takanori -
						it uses the proper keyboard type now
				*/
				break;
			case 0x36 : /* Test Command */
				gotKeyBoardData(0x7D);
				break;
			case 0x00:
				gotKeyBoardData(0);
				break;
			default :
				/* Debugger(); */
				gotKeyBoardData(0);
				break;
		}
	}
}

void KeyboardDevice::receiveEndCommand()
{
	if (kybdState_ != kKybdStateRecievingEndCommand) {
		ReportAbnormalID(0x0B02,
			"KybdState != kKybdStateRecievingEndCommand");
	} else {
		kybdState_ = kKybdStateIdle;
#ifdef _VIA_Debug
		fprintf(stderr, "enter DoKybd_ReceiveEndCommand\n");
#endif
		if (haveKeyBoardResult_) {
#ifdef _VIA_Debug
			fprintf(stderr, "HaveKeyBoardResult: %d\n", keyBoardResult_);
#endif
			haveKeyBoardResult_ = false;
			via1()->shiftInData(keyBoardResult_);
			g_wires.set(Wire_VIA1_iCB2, 1);
		}
	}
}

void KeyboardDevice::dataLineChngNtfy()
{
	switch (kybdState_) {
		case kKybdStateIdle:
			if (g_wires.get(Wire_VIA1_iCB2) == 0) {
				kybdState_ = kKybdStateRecievingCommand;
#ifdef _VIA_Debug
				fprintf(stderr, "posting kICT_Kybd_ReceiveCommand\n");
#endif
				ICT_add(kICT_Kybd_ReceiveCommand,
					6800UL * kCycleScale / 64 * machine_->config().clockMult);

				if (inquiryCommandTimer_ != 0) {
					inquiryCommandTimer_ = 0; /* abort Inquiry */
				}
			}
			break;
		case kKybdStateRecievedCommand:
			if (g_wires.get(Wire_VIA1_iCB2) == 1) {
				kybdState_ = kKybdStateRecievingEndCommand;
#ifdef _VIA_Debug
				fprintf(stderr,
					"posting kICT_Kybd_ReceiveEndCommand\n");
#endif
				ICT_add(kICT_Kybd_ReceiveEndCommand,
					6800UL * kCycleScale / 64 * machine_->config().clockMult);
			}
			break;
		default:
			break;
	}
}

void KeyboardDevice::update()
{
	if (inquiryCommandTimer_ != 0) {
		if (attemptToFinishInquiry()) {
			inquiryCommandTimer_ = 0;
		} else {
			--inquiryCommandTimer_;
			if (inquiryCommandTimer_ == 0) {
				gotKeyBoardData(0x7B);
			}
		}
	}
}
