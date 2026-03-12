/*
	PMUEMDEV.c

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
	Power Management Unit EMulated DEVice
*/

#include "core/common.h"

#include "devices/pmu.h"
#include "core/wire_bus.h"
#include "core/wire_ids.h"


/*
	ReportAbnormalID unused 0x0E0E - 0x0EFF
*/

enum {
	kPMUStateReadyForCommand,
	kPMUStateRecievingLength,
	kPMUStateRecievingBuffer,
	kPMUStateRecievedCommand,
	kPMUStateSendLength,
	kPMUStateSendBuffer,

	kPMUStates
};

void PMUDevice::reset()
{
	std::memset(buffA_, 0, sizeof(buffA_));
	p_ = nullptr;
	rem_ = 0;
	i_ = 0;
	state_ = kPMUStateReadyForCommand;
	curCommand_ = 0;
	sendNext_ = 0;
	buffL_ = 0;
	sending_ = false;
	std::memset(paramRAM_, 0, sizeof(paramRAM_));
}

void PMUDevice::startSendResult(uint8_t resultCode, uint8_t len)
{
	sendNext_ = resultCode;
	buffL_ = len;
	state_ = kPMUStateSendLength;
}

void PMUDevice::checkCommandOp()
{
	switch (curCommand_) {
		case 0x10: /* kPMUpowerCntl - power plane/clock control */
			break;
		case 0x32: /* kPMUxPramWrite - write extended PRAM byte(s) */
			if (kPMUStateRecievingBuffer == state_) {
				if (0 == i_) {
					if (buffL_ >= 2) {
						p_ = buffA_;
						rem_ = 2;
					} else {
						ReportAbnormalID(0x0E01,
							"PMU_BuffL too small for kPMUxPramWrite");
					}
				} else if (2 == i_) {
					if ((buffA_[1] + 2 == buffL_)
						&& (buffA_[0] + buffA_[1] <= 0x80))
					{
						p_ = &paramRAM_[buffA_[0]];
						rem_ = buffA_[1];
					} else {
						ReportAbnormalID(0x0E02,
							"bad range for kPMUxPramWrite");
					}
				} else {
					ReportAbnormalID(0x0E03,
						"Wrong PMU_i for kPMUpramWrite");
				}
			} else if (kPMUStateRecievedCommand == state_) {
				/* already done */
			}
			break;
#if 0
		case 0xE2: /* kPMUdownloadStatus - PRAM status */
			break;
#endif
		case 0xE0: /* kPMUwritePmgrRAM - write to internal PMGR RAM */
			break;
		case 0x21: /* kPMUpMgrADBoff - turn ADB auto-poll off */
			if (kPMUStateRecievedCommand == state_) {
				if (0 != buffL_) {
					ReportAbnormalID(0x0E04,
						"kPMUpMgrADBoff nonzero length");
				}
			}
			break;
		case 0xEC: /* kPMUPmgrSelfTest - run the PMGR selftest */
			if (kPMUStateRecievedCommand == state_) {
				startSendResult(0, 0);
			}
			break;
		case 0x78:
			/* kPMUreadINT - get PMGR interrupt data */
		case 0x68:
			/*
				kPMUbatteryRead - read battery/charger level and status
			*/
		case 0x7F:
			/*
				kPMUsleepReq - put the system to sleep (sleepSig='MATT')
			*/
			if (kPMUStateRecievedCommand == state_) {
				buffA_[0] = 0;
				startSendResult(0, 1);
			}
			break;
		case 0xE8: /* kPMUreadPmgrRAM - read from internal PMGR RAM */
			if (kPMUStateRecievedCommand == state_) {
				if ((3 == buffL_)
					&& (0 == buffA_[0])
					&& (0xEE == buffA_[1])
					&& (1 == buffA_[2]))
				{
					buffA_[0] = 1 << 5;
					startSendResult(0, 1);
				} else {
					buffA_[0] = 0;
					startSendResult(0, 1);
					/* ReportAbnormal("Unknown kPMUreadPmgrRAM op"); */
				}
			}
			break;
		case 0x3A: /* kPMUxPramRead - read extended PRAM byte(s) */
			if (kPMUStateRecievedCommand == state_) {
				if ((2 == buffL_)
					&& (buffA_[0] + buffA_[1] <= 0x80))
				{
					p_ = &paramRAM_[buffA_[0]];
					rem_ = buffA_[1];
					startSendResult(0, rem_);
				} else {
					ReportAbnormalID(0x0E05,
						"Unknown kPMUxPramRead op");
				}
			}
			break;
		case 0x38:
			/* kPMUtimeRead - read the time from the clock chip */
			if (kPMUStateRecievedCommand == state_) {
				if (0 == buffL_) {
					buffA_[0] = 0;
					buffA_[1] = 0;
					buffA_[2] = 0;
					buffA_[3] = 0;
					startSendResult(0, 4);
				} else {
					ReportAbnormalID(0x0E06, "Unknown kPMUtimeRead op");
				}
			}
			break;
		case 0x31:
			/*
				kPMUpramWrite - write the original 20 bytes of PRAM
				(Portable only)
			*/
			if (kPMUStateRecievedCommand == state_) {
				if (20 == buffL_) {
					/* done */
				} else {
					ReportAbnormalID(0x0E07,
						"Unknown kPMUpramWrite op");
				}
			} else if (kPMUStateRecievingBuffer == state_) {
				if (20 == buffL_) {
					if (0 == i_) {
						p_ = &paramRAM_[16];
						rem_ = 16;
					} else if (16 == i_) {
						p_ = &paramRAM_[8];
						rem_ = 4;
					} else {
						ReportAbnormalID(0x0E08,
							"Wrong PMU_i for kPMUpramWrite");
					}
				}
			}
			break;
		case 0x39:
			/*
				kPMUpramRead - read the original 20 bytes of PRAM
				(Portable only)
			*/
			if (kPMUStateRecievedCommand == state_) {
				if (0 == buffL_) {
					startSendResult(0, 20);
				} else {
					ReportAbnormalID(0x0E09, "Unknown kPMUpramRead op");
				}
			} else if (kPMUStateSendBuffer == state_) {
#if 0
				{
					int i;

					for (i = 0; i < kBuffSz; ++i) {
						buffA_[i] = 0;
					}
				}
#endif
				if (0 == i_) {
					p_ = &paramRAM_[16];
					rem_ = 16;
				} else if (16 == i_) {
					p_ = &paramRAM_[8];
					rem_ = 4;
				} else {
					ReportAbnormalID(0x0E0A,
						"Wrong PMU_i for kPMUpramRead");
				}
			}
			break;
		default:
			if (kPMUStateRecievedCommand == state_) {
				ReportAbnormalID(0x0E0B, "Unknown PMU op");
#if dbglog_HAVE
				dbglog_writeCStr("Unknown PMU op ");
				dbglog_writeHex(curCommand_);
				dbglog_writeReturn();
				dbglog_writeCStr("PMU_BuffL = ");
				dbglog_writeHex(buffL_);
				dbglog_writeReturn();
				if (buffL_ <= kBuffSz) {
					int i;

					for (i = 0; i < buffL_; ++i) {
						dbglog_writeCStr("PMU_BuffA[");
						dbglog_writeNum(i);
						dbglog_writeCStr("] = ");
						dbglog_writeHex(buffA_[i]);
						dbglog_writeReturn();
					}
				}
#endif
			}
			break;
	}
}

void PMUDevice::locBuffSetUpNextChunk()
{
	p_ = buffA_;
	rem_ = buffL_ - i_;
	if (rem_ >= kBuffSz) {
		rem_ = kBuffSz;
	}
}

uint8_t PMUDevice::getPMUbus() const
{
	uint8_t v;

	v = g_wires.get(Wire_VIA1_iA7);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA6);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA5);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA4);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA3);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA2);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA1);
	v <<= 1;
	v |= g_wires.get(Wire_VIA1_iA0);

	return v;
}

void PMUDevice::setPMUbus(uint8_t v)
{
	g_wires.set(Wire_VIA1_iA0, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA1, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA2, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA3, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA4, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA5, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA6, v & 0x01);
	v >>= 1;
	g_wires.set(Wire_VIA1_iA7, v & 0x01);
}

void PMUDevice::checkCommandCompletion()
{
	if (i_ == buffL_) {
		state_ = kPMUStateRecievedCommand;
		checkCommandOp();
		if ((curCommand_ & 0x08) == 0) {
			state_ = kPMUStateReadyForCommand;
			setPMUbus(0xFF);
		} else {
			if (state_ != kPMUStateSendLength) {
				startSendResult(0xFF, 0);
				state_ = kPMUStateSendLength;
			}
			i_ = 0;
			sending_ = true;
			ICT_add(kICT_PMU_Task,
				20400UL * kCycleScale / 64 * kMyClockMult);
		}
	}
}

void PMUDevice::toReadyChangeNtfy()
{
	if (sending_) {
		sending_ = false;
		ReportAbnormalID(0x0E0C,
			"PmuToReady_ChangeNtfy while PMU_Sending");
		g_wires.set(Wire_PMU_FromReady, 0);
	}
	switch (state_) {
		case kPMUStateReadyForCommand:
			if (! g_wires.get(Wire_PMU_ToReady)) {
				g_wires.set(Wire_PMU_FromReady, 0);
			} else {
				curCommand_ = getPMUbus();
				state_ = kPMUStateRecievingLength;
				g_wires.set(Wire_PMU_FromReady, 1);
			}
			break;
		case kPMUStateRecievingLength:
			if (! g_wires.get(Wire_PMU_ToReady)) {
				g_wires.set(Wire_PMU_FromReady, 0);
			} else {
				buffL_ = getPMUbus();
				i_ = 0;
				rem_ = 0;
				state_ = kPMUStateRecievingBuffer;
				checkCommandCompletion();
				g_wires.set(Wire_PMU_FromReady, 1);
			}
			break;
		case kPMUStateRecievingBuffer:
			if (! g_wires.get(Wire_PMU_ToReady)) {
				g_wires.set(Wire_PMU_FromReady, 0);
			} else {
				uint8_t v = getPMUbus();
				if (0 == rem_) {
					p_ = nullptr;
					checkCommandOp();
					if (nullptr == p_) {
						/* default handler */
						locBuffSetUpNextChunk();
					}
				}
				if (nullptr == p_) {
					/* mini vmac bug if ever happens */
					ReportAbnormalID(0x0E0D,
						"PMU_p null while kPMUStateRecievingBuffer");
				}
				*p_++ = v;
				--rem_;
				++i_;
				checkCommandCompletion();
				g_wires.set(Wire_PMU_FromReady, 1);
			}
			break;
		case kPMUStateSendLength:
			if (! g_wires.get(Wire_PMU_ToReady)) {
				/* receiving */
				g_wires.set(Wire_PMU_FromReady, 1);
			} else {
				sendNext_ = buffL_;
				state_ = kPMUStateSendBuffer;
				sending_ = true;
				ICT_add(kICT_PMU_Task,
					20400UL * kCycleScale / 64 * kMyClockMult);
			}
			break;
		case kPMUStateSendBuffer:
			if (! g_wires.get(Wire_PMU_ToReady)) {
				/* receiving */
				g_wires.set(Wire_PMU_FromReady, 1);
			} else {
				if (i_ == buffL_) {
					state_ = kPMUStateReadyForCommand;
					setPMUbus(0xFF);
				} else {
					if (0 == rem_) {
						p_ = nullptr;
						checkCommandOp();
						if (nullptr == p_) {
							/* default handler */
							locBuffSetUpNextChunk();
						}
					}
					sendNext_ = *p_++;
					--rem_;
					++i_;
					sending_ = true;
					ICT_add(kICT_PMU_Task,
						20400UL * kCycleScale / 64 * kMyClockMult);
				}
			}
			break;
	}
}

void PMUDevice::doTask()
{
	if (sending_) {
		sending_ = false;
		setPMUbus(sendNext_);
		g_wires.set(Wire_PMU_FromReady, 0);
	}
}