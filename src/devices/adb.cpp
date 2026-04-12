/*
	Apple Desktop Bus EMulated DEVice
*/

#include "core/common.h"
#include "core/ict_scheduler.h"


#include "devices/adb.h"
#include "devices/via.h"
#include "core/wire_bus.h"
#include "core/machine_obj.h"

/* Global singleton */

#ifdef _VIA_Debug
#include <stdio.h>
#endif

/*
	REPORT_ABNORMAL_ID unused 0x0C06 - 0x0CFF
*/

#include "devices/adb_shared.h"
#include "core/abnormal_ids.h"

static bool s_adbListenDatBuf;
static uint8_t ADB_IndexDatBuf;

void ADBDevice::doNewState()
{
	uint8_t state = ADB_st1 * 2 + ADB_st0;
#ifdef _VIA_Debug
	fprintf(stderr, "ADB_DoNewState: %d\n", state);
#endif
	{
		g_wires.set(Wire_VIA1_iB3_ADB_Int, 1);
		switch (state)
		{
			case 0: /* Start a new command */
				if (s_adbListenDatBuf)
				{
					s_adbListenDatBuf = false;
					s_adbSzDatBuf = ADB_IndexDatBuf;
					ADB_EndListen();
				}
				s_adbTalkDatBuf = false;
				ADB_IndexDatBuf = 0;
				s_adbCurCmd = machine_->findDevice<VIA1Device>()->shiftOutData();
				/* which sets interrupt, acknowleding command */
#ifdef _VIA_Debug
				fprintf(stderr, "in: %d\n", s_adbCurCmd);
#endif
				switch ((s_adbCurCmd >> 2) & 3)
				{
					case 0: /* reserved */
						switch (s_adbCurCmd & 3)
						{
							case 0: /* Send Reset */
								ADB_DoReset();
								break;
							case 1: /* Flush */
								ADB_Flush();
								break;
							case 2: /* reserved */
							case 3: /* reserved */
								REPORT_ABNORMAL_ID(AbnormalID::kADB_Reserved_ADB_command,
												   "Reserved ADB command");
								break;
						}
						break;
					case 1: /* reserved */
						REPORT_ABNORMAL_ID(AbnormalID::kADB_Reserved_ADB_command_2,
										   "Reserved ADB command");
						break;
					case 2: /* listen */
						s_adbListenDatBuf = true;
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
				if (!s_adbListenDatBuf)
				{
					/*
						will get here even if no pending talk data,
						when there is pending event from device
						other than the one polled by the last talk
						command. this probably indicates a bug.
					*/
					if ((!s_adbTalkDatBuf) || (ADB_IndexDatBuf >= s_adbSzDatBuf))
					{
						machine_->findDevice<VIA1Device>()->shiftInData(0xFF);
						g_wires.set(Wire_VIA1_iCB2_ADB_Data, 1);
						g_wires.set(Wire_VIA1_iB3_ADB_Int, 0);
					}
					else
					{
#ifdef _VIA_Debug
						fprintf(stderr, "*** talk one\n");
#endif
						machine_->findDevice<VIA1Device>()->shiftInData(
							s_adbDatBuf[ADB_IndexDatBuf]);
						g_wires.set(Wire_VIA1_iCB2_ADB_Data, 1);
						ADB_IndexDatBuf += 1;
					}
				}
				else
				{
					if (ADB_IndexDatBuf >= ADB_MaxSzDatBuf)
					{
						REPORT_ABNORMAL_ID(AbnormalID::kADB_ADB_listen_too_much,
										   "ADB listen too much");
						/* ADB_MaxSzDatBuf isn't big enough */
						(void)machine_->findDevice<VIA1Device>()->shiftOutData();
					}
					else
					{
#ifdef _VIA_Debug
						fprintf(stderr, "*** listen one\n");
#endif
						s_adbDatBuf[ADB_IndexDatBuf] =
							machine_->findDevice<VIA1Device>()->shiftOutData();
						ADB_IndexDatBuf += 1;
					}
				}
				break;
			case 3: /* idle */
				if (s_adbListenDatBuf)
				{
					REPORT_ABNORMAL_ID(AbnormalID::kADB_ADB_idle_follows_listen,
									   "ADB idle follows listen");
					/* apparently doesn't happen */
				}
				if (s_adbTalkDatBuf)
				{
					if (ADB_IndexDatBuf != 0)
					{
						REPORT_ABNORMAL_ID(AbnormalID::kADB_idle_when_not_done_talking,
										   "idle when not done talking");
					}
					machine_->findDevice<VIA1Device>()->shiftInData(0xFF);
					/* ADB_Int = 0; */
				}
				else if (CheckForADBanyEvt())
				{
					if (((s_adbCurCmd >> 2) & 3) == 3)
					{
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
	fprintf(stderr, "ADBstate_ChangeNtfy: %d, %d, %d\n", ADB_st1, ADB_st0, g_ict.getCurrent());
#endif
	g_ict.add(kICT_ADB_NewState, 348160UL * kCycleScale / 64 * machine_->config().clockMult);
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

	if (state == 3)
	{ /* idle */
		if (s_adbTalkDatBuf)
		{
			/* ignore, presumably being taken care of */
		}
		else if (CheckForADBanyEvt())
		{
			if (((s_adbCurCmd >> 2) & 3) == 3)
			{
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
