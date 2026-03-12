/*
	VIAEMDEV.c

	Copyright (C) 2008 Philip Cummins, Paul C. Pratt

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
	Versatile Interface Adapter EMulated DEVice

	Emulates the VIA found in the Mac Plus and Mac II.
	Wrapped in VIA1Device class (Phase 4).

	This code adapted from vMac by Philip Cummins.
*/

#include "core/common.h"



#include "devices/via.h"
#include "core/wire_bus.h"

/*
	ReportAbnormalID unused 0x0410 - 0x04FF
*/

/* ChangeNtfy externs - these are #define aliases in CNFUDPIC.h */

#ifdef VIA1_iA0_ChangeNtfy
extern void VIA1_iA0_ChangeNtfy(void);
#endif
#ifdef VIA1_iA1_ChangeNtfy
extern void VIA1_iA1_ChangeNtfy(void);
#endif
#ifdef VIA1_iA2_ChangeNtfy
extern void VIA1_iA2_ChangeNtfy(void);
#endif
#ifdef VIA1_iA3_ChangeNtfy
extern void VIA1_iA3_ChangeNtfy(void);
#endif
#ifdef VIA1_iA4_ChangeNtfy
extern void VIA1_iA4_ChangeNtfy(void);
#endif
#ifdef VIA1_iA5_ChangeNtfy
extern void VIA1_iA5_ChangeNtfy(void);
#endif
#ifdef VIA1_iA6_ChangeNtfy
extern void VIA1_iA6_ChangeNtfy(void);
#endif
#ifdef VIA1_iA7_ChangeNtfy
extern void VIA1_iA7_ChangeNtfy(void);
#endif
#ifdef VIA1_iB0_ChangeNtfy
extern void VIA1_iB0_ChangeNtfy(void);
#endif
#ifdef VIA1_iB1_ChangeNtfy
extern void VIA1_iB1_ChangeNtfy(void);
#endif
#ifdef VIA1_iB2_ChangeNtfy
extern void VIA1_iB2_ChangeNtfy(void);
#endif
#ifdef VIA1_iB3_ChangeNtfy
extern void VIA1_iB3_ChangeNtfy(void);
#endif
#ifdef VIA1_iB4_ChangeNtfy
extern void VIA1_iB4_ChangeNtfy(void);
#endif
#ifdef VIA1_iB5_ChangeNtfy
extern void VIA1_iB5_ChangeNtfy(void);
#endif
#ifdef VIA1_iB6_ChangeNtfy
extern void VIA1_iB6_ChangeNtfy(void);
#endif
#ifdef VIA1_iB7_ChangeNtfy
extern void VIA1_iB7_ChangeNtfy(void);
#endif
#ifdef VIA1_iCB2_ChangeNtfy
extern void VIA1_iCB2_ChangeNtfy(void);
#endif

#define Ui3rPowOf2(p) (1 << (p))
#define Ui3rTestBit(i, p) (((i) & Ui3rPowOf2(p)) != 0)

#define VIA1_ORA_CanInOrOut (VIA1_ORA_CanIn | VIA1_ORA_CanOut)
#define VIA1_ORB_CanInOrOut (VIA1_ORB_CanIn | VIA1_ORB_CanOut)

#define kIntCA2 0
#define kIntCA1 1
#define kIntSR 2
#define kIntCB2 3
#define kIntCB1 4
#define kIntT2 5
#define kIntT1 6

#define VIA1_dolog (dbglog_HAVE && 0)

/* Global singleton */
VIA1Device* g_via1 = nullptr;

/* ===== VIA1Device method implementations ===== */

uint8_t VIA1Device::getORA(uint8_t Selection)
{
	uint8_t Value = (~ VIA1_ORA_CanIn) & Selection & VIA1_ORA_FloatVal;

#if Ui3rTestBit(VIA1_ORA_CanIn, 7)
	if (Ui3rTestBit(Selection, 7)) {
		Value |= (VIA1_iA7 << 7);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 6)
	if (Ui3rTestBit(Selection, 6)) {
		Value |= (VIA1_iA6 << 6);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 5)
	if (Ui3rTestBit(Selection, 5)) {
		Value |= (VIA1_iA5 << 5);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 4)
	if (Ui3rTestBit(Selection, 4)) {
		Value |= (VIA1_iA4 << 4);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 3)
	if (Ui3rTestBit(Selection, 3)) {
		Value |= (VIA1_iA3 << 3);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 2)
	if (Ui3rTestBit(Selection, 2)) {
		Value |= (VIA1_iA2 << 2);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 1)
	if (Ui3rTestBit(Selection, 1)) {
		Value |= (VIA1_iA1 << 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 0)
	if (Ui3rTestBit(Selection, 0)) {
		Value |= (VIA1_iA0 << 0);
	}
#endif

	return Value;
}

uint8_t VIA1Device::getORB(uint8_t Selection)
{
	uint8_t Value = (~ VIA1_ORB_CanIn) & Selection & VIA1_ORB_FloatVal;

#if Ui3rTestBit(VIA1_ORB_CanIn, 7)
	if (Ui3rTestBit(Selection, 7)) {
		Value |= (VIA1_iB7 << 7);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 6)
	if (Ui3rTestBit(Selection, 6)) {
		Value |= (VIA1_iB6 << 6);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 5)
	if (Ui3rTestBit(Selection, 5)) {
		Value |= (VIA1_iB5 << 5);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 4)
	if (Ui3rTestBit(Selection, 4)) {
		Value |= (VIA1_iB4 << 4);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 3)
	if (Ui3rTestBit(Selection, 3)) {
		Value |= (VIA1_iB3 << 3);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 2)
	if (Ui3rTestBit(Selection, 2)) {
		Value |= (VIA1_iB2 << 2);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 1)
	if (Ui3rTestBit(Selection, 1)) {
		Value |= (VIA1_iB1 << 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 0)
	if (Ui3rTestBit(Selection, 0)) {
		Value |= (VIA1_iB0 << 0);
	}
#endif

	return Value;
}

void VIA1Device::putORA(uint8_t Selection, uint8_t Data)
{
#if Ui3rTestBit(VIA1_ORA_CanOut, 7)
	if (Ui3rTestBit(Selection, 7)) {
		g_wires.set(Wire_VIA1_iA7_SCCwaitrq, (Data >> 7) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 5)
	if (Ui3rTestBit(Selection, 5)) {
		g_wires.set(Wire_VIA1_iA5_IWMvSel, (Data >> 5) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 4)
	if (Ui3rTestBit(Selection, 4)) {
		g_wires.set(Wire_VIA1_iA4_MemOverlay, (Data >> 4) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 3)
	if (Ui3rTestBit(Selection, 3)) {
		g_wires.set(Wire_VIA1_iA3_SCCvSync, (Data >> 3) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 2)
	if (Ui3rTestBit(Selection, 2)) {
		g_wires.set(Wire_VIA1_iA2_unknown, (Data >> 2) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 1)
	if (Ui3rTestBit(Selection, 1)) {
		g_wires.set(Wire_VIA1_iA1_unknown, (Data >> 1) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 0)
	if (Ui3rTestBit(Selection, 0)) {
		g_wires.set(Wire_VIA1_iA0_unknown, Data & 1);
	}
#endif
}

void VIA1Device::putORB(uint8_t Selection, uint8_t Data)
{
#if Ui3rTestBit(VIA1_ORB_CanOut, 7)
	if (Ui3rTestBit(Selection, 7)) {
		g_wires.set(Wire_VIA1_iB7_unknown, (Data >> 7) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 5)
	if (Ui3rTestBit(Selection, 5)) {
		g_wires.set(Wire_VIA1_iB5_ADB_st1, (Data >> 5) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 4)
	if (Ui3rTestBit(Selection, 4)) {
		g_wires.set(Wire_VIA1_iB4_ADB_st0, (Data >> 4) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 3)
	if (Ui3rTestBit(Selection, 3)) {
		g_wires.set(Wire_VIA1_iB3_ADB_Int, (Data >> 3) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 2)
	if (Ui3rTestBit(Selection, 2)) {
		g_wires.set(Wire_VIA1_iB2_RTCunEnabled, (Data >> 2) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 1)
	if (Ui3rTestBit(Selection, 1)) {
		g_wires.set(Wire_VIA1_iB1_RTCclock, (Data >> 1) & 1);
	}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 0)
	if (Ui3rTestBit(Selection, 0)) {
		g_wires.set(Wire_VIA1_iB0_RTCdataLine, Data & 1);
	}
#endif
}

void VIA1Device::setDDR_A(uint8_t Data)
{
	uint8_t floatbits = d_.DDR_A & ~ Data;
	uint8_t unfloatbits = Data & ~ d_.DDR_A;

	if (floatbits != 0) {
		putORA(floatbits, VIA1_ORA_FloatVal);
	}
	d_.DDR_A = Data;
	if (unfloatbits != 0) {
		putORA(unfloatbits, d_.ORA);
	}
	if ((Data & ~ VIA1_ORA_CanOut) != 0) {
		ReportAbnormalID(0x0401, "Set d_.DDR_A unexpected direction");
	}
}

void VIA1Device::setDDR_B(uint8_t Data)
{
	uint8_t floatbits = d_.DDR_B & ~ Data;
	uint8_t unfloatbits = Data & ~ d_.DDR_B;

	if (floatbits != 0) {
		putORB(floatbits, VIA1_ORB_FloatVal);
	}
	d_.DDR_B = Data;
	if (unfloatbits != 0) {
		putORB(unfloatbits, d_.ORB);
	}
	if ((Data & ~ VIA1_ORB_CanOut) != 0) {
		ReportAbnormalID(0x0402, "Set d_.DDR_B unexpected direction");
	}
}

void VIA1Device::checkInterruptFlag()
{
	uint8_t NewInterruptRequest =
		((d_.IFR & d_.IER) != 0) ? 1 : 0;

	g_wires.set(Wire_VIA1_InterruptRequest, NewInterruptRequest);
}

void VIA1Device::setInterruptFlag(uint8_t VIA_Int)
{
	d_.IFR |= ((uint8_t)1 << VIA_Int);
	checkInterruptFlag();
}

void VIA1Device::clrInterruptFlag(uint8_t VIA_Int)
{
	d_.IFR &= ~ ((uint8_t)1 << VIA_Int);
	checkInterruptFlag();
}

void VIA1Device::clear()
{
	d_.ORA = 0; d_.DDR_A = 0;
	d_.ORB = 0; d_.DDR_B = 0;
	d_.T1L_L = d_.T1L_H = 0x00;
	d_.T2L_L = 0x00;
	d_.T1C_F = 0;
	d_.T2C_F = 0;
	d_.SR = d_.ACR = 0x00;
	d_.PCR = d_.IFR = d_.IER = 0x00;
	T1_Active = T2_Active = 0x00;
	T1IntReady = false;
}

void VIA1Device::zap()
{
	clear();
	g_wires.set(Wire_VIA1_InterruptRequest, 0);
}

void VIA1Device::reset()
{
	setDDR_A(0);
	setDDR_B(0);
	clear();
	checkInterruptFlag();
}

#ifdef _VIA_Debug
#include <stdio.h>
#endif

void VIA1Device::shiftInData(uint8_t v)
{
	uint8_t ShiftMode = (d_.ACR & 0x1C) >> 2;

	if (ShiftMode != 3) {
#if ExtraAbnormalReports
		if (ShiftMode == 0) {
			/* happens on reset */
		} else {
			ReportAbnormalID(0x0403, "VIA Not ready to shift in");
		}
#endif
	} else {
		d_.SR = v;
		setInterruptFlag(kIntSR);
		setInterruptFlag(kIntCB1);
	}
}

uint8_t VIA1Device::shiftOutData()
{
	if (((d_.ACR & 0x1C) >> 2) != 7) {
		ReportAbnormalID(0x0404, "VIA Not ready to shift out");
		return 0;
	} else {
		setInterruptFlag(kIntSR);
		setInterruptFlag(kIntCB1);
		g_wires.set(Wire_VIA1_iCB2_ADB_Data, d_.SR & 1);
		return d_.SR;
	}
}

#define CyclesPerViaTime (10 * kMyClockMult)
#define CyclesScaledPerViaTime (kCycleScale * CyclesPerViaTime)

void VIA1Device::doTimer1Check()
{
	if (T1Running) {
		iCountt NewTime = GetCuriCount();
		iCountt deltaTime = (NewTime - T1LastTime);
		if (deltaTime != 0) {
			uint32_t Temp = d_.T1C_F;
			uint32_t deltaTemp =
				(deltaTime / CyclesPerViaTime) << (16 - kLn2CycleScale);
			uint32_t NewTemp = Temp - deltaTemp;
			if ((deltaTime > (0x00010000UL * CyclesScaledPerViaTime))
				|| ((Temp <= deltaTemp) && (Temp != 0)))
			{
				if ((d_.ACR & 0x40) != 0) { /* Free Running? */
					uint16_t v = (d_.T1L_H << 8) + d_.T1L_L;
					uint16_t ntrans = 1 + ((v == 0) ? 0 :
						(((deltaTemp - Temp) / v) >> 16));
					NewTemp += (((uint32_t)v * ntrans) << 16);
#if Ui3rTestBit(VIA1_ORB_CanOut, 7)
					if ((d_.ACR & 0x80) != 0) { /* invert ? */
						if ((ntrans & 1) != 0) {
							g_wires.set(Wire_VIA1_iB7_unknown, VIA1_iB7 ^ 1);
						}
					}
#endif
					setInterruptFlag(kIntT1);
#if VIA1_dolog && 1
					dbglog_WriteNote("VIA1 Timer 1 Interrupt");
#endif
				} else {
					if (T1_Active == 1) {
						T1_Active = 0;
						setInterruptFlag(kIntT1);
#if VIA1_dolog && 1
						dbglog_WriteNote("VIA1 Timer 1 Interrupt");
#endif
					}
				}
			}

			d_.T1C_F = NewTemp;
			T1LastTime = NewTime;
		}

		T1IntReady = false;
		if ((d_.IFR & (1 << kIntT1)) == 0) {
			if (((d_.ACR & 0x40) != 0) || (T1_Active == 1)) {
				uint32_t NewTemp = d_.T1C_F;
				uint32_t NewTimer;
#ifdef _VIA_Debug
				fprintf(stderr, "posting Timer1Check, %d, %d\n",
					NewTemp, GetCuriCount());
#endif
				if (NewTemp == 0) {
					NewTimer = (0x00010000UL * CyclesScaledPerViaTime);
				} else {
					NewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale)))
						* CyclesPerViaTime;
				}
				ICT_add(kICT_VIA1_Timer1Check, NewTimer);
				T1IntReady = true;
			}
		}
	}
}

void VIA1Device::checkT1IntReady()
{
	if (T1Running) {
		bool NewT1IntReady = false;

		if ((d_.IFR & (1 << kIntT1)) == 0) {
			if (((d_.ACR & 0x40) != 0) || (T1_Active == 1)) {
				NewT1IntReady = true;
			}
		}

		if (T1IntReady != NewT1IntReady) {
			T1IntReady = NewT1IntReady;
			if (NewT1IntReady) {
				doTimer1Check();
			}
		}
	}
}

uint16_t VIA1Device::getT1InvertTime()
{
	uint16_t v;

	if ((d_.ACR & 0xC0) == 0xC0) {
		v = (d_.T1L_H << 8) + d_.T1L_L;
	} else {
		v = 0;
	}
	return v;
}

void VIA1Device::doTimer2Check()
{
	if (T2Running || T2C_ShortTime) {
		iCountt NewTime = GetCuriCount();
		uint32_t Temp = d_.T2C_F;
		iCountt deltaTime = (NewTime - T2LastTime);
		uint32_t deltaTemp = (deltaTime / CyclesPerViaTime)
			<< (16 - kLn2CycleScale);
		uint32_t NewTemp = Temp - deltaTemp;
		if (T2_Active == 1) {
			if ((deltaTime > (0x00010000UL * CyclesScaledPerViaTime))
				|| ((Temp <= deltaTemp) && (Temp != 0)))
			{
				T2C_ShortTime = false;
				T2_Active = 0;
				setInterruptFlag(kIntT2);
#if VIA1_dolog && 1
				dbglog_WriteNote("VIA1 Timer 2 Interrupt");
#endif
			} else {
				uint32_t NewTimer;
#ifdef _VIA_Debug
				fprintf(stderr, "posting Timer2Check, %d, %d\n",
					Temp, GetCuriCount());
#endif
#if VIA1_dolog
				dbglog_WriteNote("VIA1 Timer 2 Later");
#endif
				if (NewTemp == 0) {
					NewTimer = (0x00010000UL * CyclesScaledPerViaTime);
				} else {
					NewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale)))
						* CyclesPerViaTime;
				}
				ICT_add(kICT_VIA1_Timer2Check, NewTimer);
			}
		}
		d_.T2C_F = NewTemp;
		T2LastTime = NewTime;
	}
}

#define kORB    0x00
#define kORA_H  0x01
#define kDDR_B  0x02
#define kDDR_A  0x03
#define kT1C_L  0x04
#define kT1C_H  0x05
#define kT1L_L  0x06
#define kT1L_H  0x07
#define kT2_L   0x08
#define kT2_H   0x09
#define kSR     0x0A
#define kACR    0x0B
#define kPCR    0x0C
#define kIFR    0x0D
#define kIER    0x0E
#define kORA    0x0F

uint32_t VIA1Device::access(uint32_t Data, bool WriteMem, uint32_t addr)
{
	switch (addr) {
		case kORB   :
#if VIA1_CB2modesAllowed != 0x01
			if ((d_.PCR & 0xE0) == 0)
#endif
			{
				clrInterruptFlag(kIntCB2);
			}
			clrInterruptFlag(kIntCB1);
			if (WriteMem) {
				d_.ORB = Data;
				putORB(d_.DDR_B, d_.ORB);
			} else {
				Data = (d_.ORB & d_.DDR_B)
					| getORB(~ d_.DDR_B);
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kORB", Data, WriteMem);
#endif
			break;
		case kDDR_B :
			if (WriteMem) {
				setDDR_B(Data);
			} else {
				Data = d_.DDR_B;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kDDR_B", Data, WriteMem);
#endif
			break;
		case kDDR_A :
			if (WriteMem) {
				setDDR_A(Data);
			} else {
				Data = d_.DDR_A;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kDDR_A", Data, WriteMem);
#endif
			break;
		case kT1C_L :
			if (WriteMem) {
				d_.T1L_L = Data;
			} else {
				clrInterruptFlag(kIntT1);
				doTimer1Check();
				Data = (d_.T1C_F & 0x00FF0000) >> 16;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kT1C_L", Data, WriteMem);
#endif
			break;
		case kT1C_H :
			if (WriteMem) {
				d_.T1L_H = Data;
				clrInterruptFlag(kIntT1);
				d_.T1C_F = (Data << 24) + (d_.T1L_L << 16);
				if ((d_.ACR & 0x40) == 0) {
					T1_Active = 1;
				}
				T1LastTime = GetCuriCount();
				doTimer1Check();
			} else {
				doTimer1Check();
				Data = (d_.T1C_F & 0xFF000000) >> 24;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kT1C_H", Data, WriteMem);
#endif
			break;
		case kT1L_L :
			if (WriteMem) {
				d_.T1L_L = Data;
			} else {
				Data = d_.T1L_L;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kT1L_L", Data, WriteMem);
#endif
			break;
		case kT1L_H :
			if (WriteMem) {
				d_.T1L_H = Data;
			} else {
				Data = d_.T1L_H;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kT1L_H", Data, WriteMem);
#endif
			break;
		case kT2_L  :
			if (WriteMem) {
				d_.T2L_L = Data;
			} else {
				clrInterruptFlag(kIntT2);
				doTimer2Check();
				Data = (d_.T2C_F & 0x00FF0000) >> 16;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kT2_L", Data, WriteMem);
#endif
			break;
		case kT2_H  :
			if (WriteMem) {
				d_.T2C_F = (Data << 24) + (d_.T2L_L << 16);
				clrInterruptFlag(kIntT2);
				T2_Active = 1;

				if ((d_.T2C_F < (128UL << 16))
					&& (d_.T2C_F != 0))
				{
#if VIA1_dolog
					dbglog_StartLine();
					dbglog_writeCStr("VIA1_T2C_ShortTime ");
					dbglog_writeHex(d_.T2C_F);
					dbglog_writeCStr(", IER ");
					dbglog_writeHex(d_.IER);
					dbglog_writeCStr(", T2Running ");
					dbglog_writeHex(T2Running);
					dbglog_writeCStr(", T2C_ShortTime ");
					dbglog_writeHex(T2C_ShortTime);
					dbglog_writeReturn();
#endif
					T2C_ShortTime = true;
				}
				T2LastTime = GetCuriCount();
				doTimer2Check();
			} else {
				doTimer2Check();
				Data = (d_.T2C_F & 0xFF000000) >> 24;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kT2_H", Data, WriteMem);
#endif
			break;
		case kSR:
#ifdef _VIA_Debug
			fprintf(stderr, "d_.SR: %d, %d, %d\n",
				WriteMem, ((d_.ACR & 0x1C) >> 2), Data);
#endif
			if (WriteMem) {
				d_.SR = Data;
			}
			clrInterruptFlag(kIntSR);
			switch ((d_.ACR & 0x1C) >> 2) {
				case 3 : /* Shifting In */
					break;
				case 6 : /* shift out under o2 clock */
					if ((! WriteMem) || (d_.SR != 0)) {
						ReportAbnormalID(0x0405,
							"VIA shift mode 6, non zero");
					} else {
#ifdef _VIA_Debug
						fprintf(stderr, "posting Foo2Task\n");
#endif
						g_wires.set(Wire_VIA1_iCB2_ADB_Data, 0);
					}
#if 0
					setInterruptFlag(kIntSR);
#endif
					break;
				case 7 : /* Shifting Out */
					break;
			}
			if (! WriteMem) {
				Data = d_.SR;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kSR", Data, WriteMem);
#endif
			break;
		case kACR:
			if (WriteMem) {
#if 1
				if ((d_.ACR & 0x10) != ((uint8_t)Data & 0x10)) {
					if ((Data & 0x10) == 0) {
						g_wires.set(Wire_VIA1_iCB2_ADB_Data, 1);
					}
				}
#endif
				d_.ACR = Data;
				if ((d_.ACR & 0x20) != 0) {
					ReportAbnormalID(0x0406,
						"Set d_.ACR T2 Timer pulse counting");
				}
				switch ((d_.ACR & 0xC0) >> 6) {
					case 2:
						ReportAbnormalID(0x0407,
							"Set d_.ACR T1 Timer mode 2");
						break;
				}
				checkT1IntReady();
				switch ((d_.ACR & 0x1C) >> 2) {
					case 0:
						clrInterruptFlag(kIntSR);
						break;
					case 1:
					case 2:
					case 4:
					case 5:
						ReportAbnormalID(0x0408,
							"Set d_.ACR shift mode 1,2,4,5");
						break;
					default:
						break;
				}
				if ((d_.ACR & 0x03) != 0) {
					ReportAbnormalID(0x0409,
						"Set d_.ACR T2 Timer latching enabled");
				}
			} else {
				Data = d_.ACR;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kACR", Data, WriteMem);
#endif
			break;
		case kPCR:
			if (WriteMem) {
				d_.PCR = Data;
#define Ui3rSetContains(s, i) (((s) & (1 << (i))) != 0)
				if (! Ui3rSetContains(VIA1_CB2modesAllowed,
					(d_.PCR >> 5) & 0x07))
				{
					ReportAbnormalID(0x040A,
						"Set d_.PCR CB2 Control mode?");
				}
				if ((d_.PCR & 0x10) != 0) {
					ReportAbnormalID(0x040B,
						"Set d_.PCR CB1 INTERRUPT CONTROL?");
				}
				if (! Ui3rSetContains(VIA1_CA2modesAllowed,
					(d_.PCR >> 1) & 0x07))
				{
					ReportAbnormalID(0x040C,
						"Set d_.PCR CA2 INTERRUPT CONTROL?");
				}
				if ((d_.PCR & 0x01) != 0) {
					ReportAbnormalID(0x040D,
						"Set d_.PCR CA1 INTERRUPT CONTROL?");
				}
			} else {
				Data = d_.PCR;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kPCR", Data, WriteMem);
#endif
			break;
		case kIFR:
			if (WriteMem) {
				d_.IFR = d_.IFR & ((~ Data) & 0x7F);
				checkInterruptFlag();
				checkT1IntReady();
			} else {
				Data = d_.IFR;
				if ((d_.IFR & d_.IER) != 0) {
					Data |= 0x80;
				}
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kIFR", Data, WriteMem);
#endif
			break;
		case kIER   :
			if (WriteMem) {
				if ((Data & 0x80) == 0) {
					d_.IER = d_.IER & ((~ Data) & 0x7F);
#if 0 != VIA1_IER_Never0
					if ((Data & VIA1_IER_Never0) != 0) {
						ReportAbnormalID(0x040E, "IER Never0 clr");
					}
#endif
				} else {
					d_.IER = d_.IER | (Data & 0x7F);
#if 0 != VIA1_IER_Never1
					if ((d_.IER & VIA1_IER_Never1) != 0) {
						ReportAbnormalID(0x040F, "IER Never1 set");
					}
#endif
				}
				checkInterruptFlag();
			} else {
				Data = d_.IER | 0x80;
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kIER", Data, WriteMem);
#endif
			break;
		case kORA   :
		case kORA_H :
			if ((d_.PCR & 0xE) == 0) {
				clrInterruptFlag(kIntCA2);
			}
			clrInterruptFlag(kIntCA1);
			if (WriteMem) {
				d_.ORA = Data;
				putORA(d_.DDR_A, d_.ORA);
			} else {
				Data = (d_.ORA & d_.DDR_A)
					| getORA(~ d_.DDR_A);
			}
#if VIA1_dolog && 1
			dbglog_Access("VIA1_Access kORA", Data, WriteMem);
#endif
			break;
	}
	return Data;
}

void VIA1Device::extraTimeBegin()
{
#if VIA1_dolog
	dbglog_WriteNote("VIA1_ExtraTimeBegin");
#endif
	if (T1Running) {
		doTimer1Check();
		T1Running = false;
	}
	if (T2Running) {
		doTimer2Check();
		T2Running = false;
	}
}

void VIA1Device::extraTimeEnd()
{
#if VIA1_dolog
	dbglog_WriteNote("VIA1_ExtraTimeEnd");
#endif
	if (! T1Running) {
		T1Running = true;
		T1LastTime = GetCuriCount();
		doTimer1Check();
	}
	if (! T2Running) {
		T2Running = true;
		if (! T2C_ShortTime) {
			T2LastTime = GetCuriCount();
		}
		doTimer2Check();
	}
}

void VIA1Device::iCA1_PulseNtfy()
{
	setInterruptFlag(kIntCA1);
}

void VIA1Device::iCA2_PulseNtfy()
{
	setInterruptFlag(kIntCA2);
}

void VIA1Device::iCB1_PulseNtfy()
{
	setInterruptFlag(kIntCB1);
}

void VIA1Device::iCB2_PulseNtfy()
{
	setInterruptFlag(kIntCB2);
}


/* ===== Backward-compatible forwarding functions ===== */

static VIA1Device& ensureVIA1()
{
	if (!g_via1) {
		static VIA1Device instance;
		g_via1 = &instance;
	}
	return *g_via1;
}

void VIA1_Zap(void)
{
	ensureVIA1().zap();
}

void VIA1_Reset(void)
{
	ensureVIA1().reset();
}

uint32_t VIA1_Access(uint32_t Data, bool WriteMem, uint32_t addr)
{
	return g_via1->access(Data, WriteMem, addr);
}

void VIA1_ExtraTimeBegin(void)
{
	g_via1->extraTimeBegin();
}

void VIA1_ExtraTimeEnd(void)
{
	g_via1->extraTimeEnd();
}

void VIA1_DoTimer1Check(void)
{
	g_via1->doTimer1Check();
}

void VIA1_DoTimer2Check(void)
{
	g_via1->doTimer2Check();
}

uint16_t VIA1_GetT1InvertTime(void)
{
	return g_via1->getT1InvertTime();
}

void VIA1_ShiftInData(uint8_t v)
{
	g_via1->shiftInData(v);
}

uint8_t VIA1_ShiftOutData(void)
{
	return g_via1->shiftOutData();
}

/* Pulse notifications - called through #define aliases in CNFUDPIC.h */

void VIA1_iCA1_Sixtieth_PulseNtfy(void)
{
	g_via1->iCA1_PulseNtfy();
}

void VIA1_iCA2_RTC_OneSecond_PulseNtfy(void)
{
	g_via1->iCA2_PulseNtfy();
}

