/*
	ADBEMDEV.c

	Copyright (C) 2008 Paul C. Pratt

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
	Apple Desktop Bus EMulated DEVice
*/

#include "core/common.h"



#include "devices/adb.h"
#include "devices/via.h"
#include "core/wire_bus.h"
#include "core/machine_obj.h"

/* Global singleton */

#ifdef _VIA_Debug
#include <stdio.h>
#endif

/*
	ReportAbnormalID unused 0x0C06 - 0x0CFF
*/

#include "devices/adb_shared.h"

static bool ADB_ListenDatBuf;
static uint8_t ADB_IndexDatBuf;

void ADBDevice::doNewState()
{
	uint8_t state = ADB_st1 * 2 + ADB_st0;
#ifdef _VIA_Debug
	fprintf(stderr, "ADB_DoNewState: %d\n", state);
#endif
	{
		g_wires.set(Wire_VIA1_iB3_ADB_Int, 1);
		switch (state) {
			case 0: /* Start a new command */
				if (ADB_ListenDatBuf) {
					ADB_ListenDatBuf = false;
					ADB_SzDatBuf = ADB_IndexDatBuf;
					ADB_EndListen();
				}
				ADB_TalkDatBuf = false;
				ADB_IndexDatBuf = 0;
				ADB_CurCmd = machine_->findDevice<VIA1Device>()->shiftOutData();
					/* which sets interrupt, acknowleding command */
#ifdef _VIA_Debug
				fprintf(stderr, "in: %d\n", ADB_CurCmd);
#endif
				switch ((ADB_CurCmd >> 2) & 3) {
					case 0: /* reserved */
						switch (ADB_CurCmd & 3) {
							case 0: /* Send Reset */
								ADB_DoReset();
								break;
							case 1: /* Flush */
								ADB_Flush();
								break;
							case 2: /* reserved */
							case 3: /* reserved */
								ReportAbnormalID(0x0C01,
									"Reserved ADB command");
								break;
						}
						break;
					case 1: /* reserved */
						ReportAbnormalID(0x0C02,
							"Reserved ADB command");
						break;
					case 2: /* listen */
						ADB_ListenDatBuf = true;
#ifdef _VIA_Debug
						fprintf(stderr, "*** listening\n");
#endif
						break;
					case 3: /* talk */
						ADB_DoTalk();
						break;
				}
				break;
			case 1: /* Transfer date byte (even) */
			case 2: /* Transfer date byte (odd) */
				if (! ADB_ListenDatBuf) {
					/*
						will get here even if no pending talk data,
						when there is pending event from device
						other than the one polled by the last talk
						command. this probably indicates a bug.
					*/
					if ((! ADB_TalkDatBuf)
						|| (ADB_IndexDatBuf >= ADB_SzDatBuf))
					{
						machine_->findDevice<VIA1Device>()->shiftInData(0xFF);
					g_wires.set(Wire_VIA1_iCB2_ADB_Data, 1);
					g_wires.set(Wire_VIA1_iB3_ADB_Int, 0);
					} else {
#ifdef _VIA_Debug
						fprintf(stderr, "*** talk one\n");
#endif
						machine_->findDevice<VIA1Device>()->shiftInData(ADB_DatBuf[ADB_IndexDatBuf]);
					g_wires.set(Wire_VIA1_iCB2_ADB_Data, 1);
						ADB_IndexDatBuf += 1;
					}
				} else {
					if (ADB_IndexDatBuf >= ADB_MaxSzDatBuf) {
						ReportAbnormalID(0x0C03, "ADB listen too much");
							/* ADB_MaxSzDatBuf isn't big enough */
						(void) machine_->findDevice<VIA1Device>()->shiftOutData();
					} else {
#ifdef _VIA_Debug
						fprintf(stderr, "*** listen one\n");
#endif
						ADB_DatBuf[ADB_IndexDatBuf] = machine_->findDevice<VIA1Device>()->shiftOutData();
						ADB_IndexDatBuf += 1;
					}
				}
				break;
			case 3: /* idle */
				if (ADB_ListenDatBuf) {
					ReportAbnormalID(0x0C04, "ADB idle follows listen");
					/* apparently doesn't happen */
				}
				if (ADB_TalkDatBuf) {
					if (ADB_IndexDatBuf != 0) {
						ReportAbnormalID(0x0C05,
							"idle when not done talking");
					}
					machine_->findDevice<VIA1Device>()->shiftInData(0xFF);
					/* ADB_Int = 0; */
				} else if (CheckForADBanyEvt()) {
					if (((ADB_CurCmd >> 2) & 3) == 3) {
						ADB_DoTalk();
					}
					machine_->findDevice<VIA1Device>()->shiftInData(0xFF);
					/* ADB_Int = 0; */
				}
				break;
		}
	}
}

void ADBDevice::stateChangeNtfy()
{
#ifdef _VIA_Debug
	fprintf(stderr, "ADBstate_ChangeNtfy: %d, %d, %d\n",
		ADB_st1, ADB_st0, GetCuriCount());
#endif
	ICT_add(kICT_ADB_NewState,
		348160UL * kCycleScale / 64 * machine_->config().clockMult);
		/*
			Macintosh Family Hardware Reference say device "must respond
			to talk command within 260 microseconds", which translates
			to about 190 instructions. But haven't seen much problems
			even for very large values (tens of thousands), and do see
			problems for small values. 50 is definitely too small,
			mouse doesn't move smoothly. There may still be some
			signs of this problem with 150.

			On the other hand, how fast the device must respond may
			not be related to how fast the ADB transceiver responds.
		*/
}

void ADBDevice::dataLineChngNtfy()
{
#ifdef _VIA_Debug
	fprintf(stderr, "ADB_DataLineChngNtfy: %d\n", ADB_Data);
#endif
}

void ADBDevice::update()
{
	uint8_t state = ADB_st1 * 2 + ADB_st0;

	if (state == 3) { /* idle */
		if (ADB_TalkDatBuf) {
			/* ignore, presumably being taken care of */
		} else if (CheckForADBanyEvt())
		{
			if (((ADB_CurCmd >> 2) & 3) == 3) {
				ADB_DoTalk();
			}
			machine_->findDevice<VIA1Device>()->shiftInData(0xFF);
				/*
					Wouldn't expect this would be needed unless
					there is actually talk data. But without it,
					ADB never polls the other devices. Clearing
					ADB_Int has no effect.
				*/
			/*
				ADB_Int = 0;
				seems to have no effect, which probably indicates a bug
			*/
		}
	}
}
