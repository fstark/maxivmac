#!/usr/bin/env python3
"""Write via2.h and via2.cpp with VIA2Device class wrapper."""
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

via2_h = r"""/*
	VIA2EMDV.h

	Copyright (C) 2004 Philip Cummins, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

#pragma once

#include "devices/device.h"
#include <cstdint>

// VIA2 Device class wrapping the original VIA2 emulation
class VIA2Device : public Device {
public:
	// Device interface
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override;
	void reset() override;
	const char* name() const override { return "VIA2"; }

	// Timer ICT callbacks
	void doTimer1Check();
	void doTimer2Check();

	// Extra time (pause/resume timers during extra emulation cycles)
	void extraTimeBegin();
	void extraTimeEnd();

	// Pulse notifications (interrupt sources)
	void iCA1_PulseNtfy();
	void iCA2_PulseNtfy();
	void iCB1_PulseNtfy();
	void iCB2_PulseNtfy();

	// Shift register
	void shiftInData(uint8_t v);
	uint8_t shiftOutData();

	// Timer invert time
	uint16_t getT1InvertTime();

	// Internal state - public for backward compatibility during migration
	struct VIA_Ty {
		uint32_t T1C_F;  /* Timer 1 Counter Fixed Point */
		uint32_t T2C_F;  /* Timer 2 Counter Fixed Point */
		uint8_t ORB;     /* Buffer B */
		uint8_t DDR_B;   /* Data Direction Register B */
		uint8_t DDR_A;   /* Data Direction Register A */
		uint8_t T1L_L;   /* Timer 1 Latch Low */
		uint8_t T1L_H;   /* Timer 1 Latch High */
		uint8_t T2L_L;   /* Timer 2 Latch Low */
		uint8_t SR;      /* Shift Register */
		uint8_t ACR;     /* Auxiliary Control Register */
		uint8_t PCR;     /* Peripheral Control Register */
		uint8_t IFR;     /* Interrupt Flag Register */
		uint8_t IER;     /* Interrupt Enable Register */
		uint8_t ORA;     /* Buffer A */
	};

	VIA_Ty d_{};

	uint8_t T1_Active = 0;
	uint8_t T2_Active = 0;
	bool T1IntReady = false;
	bool T1Running = true;
	uint32_t T1LastTime = 0;
	bool T2Running = true;
	bool T2C_ShortTime = false;
	uint32_t T2LastTime = 0;

private:
	uint8_t getORA(uint8_t selection);
	uint8_t getORB(uint8_t selection);
	void putORA(uint8_t selection, uint8_t data);
	void putORB(uint8_t selection, uint8_t data);
	void setDDR_A(uint8_t data);
	void setDDR_B(uint8_t data);
	void checkInterruptFlag();
	void setInterruptFlag(uint8_t viaInt);
	void clrInterruptFlag(uint8_t viaInt);
	void clear();
	void checkT1IntReady();
};

// Global singleton pointer (for backward compatibility during migration)
extern VIA2Device* g_via2;

// Backward-compatible free function API (forwards to g_via2)
extern void VIA2_Zap(void);
extern void VIA2_Reset(void);
extern uint32_t VIA2_Access(uint32_t Data, bool WriteMem, uint32_t addr);
extern void VIA2_ExtraTimeBegin(void);
extern void VIA2_ExtraTimeEnd(void);
#ifdef VIA2_iCA1_PulseNtfy
extern void VIA2_iCA1_PulseNtfy(void);
#endif
#ifdef VIA2_iCA2_PulseNtfy
extern void VIA2_iCA2_PulseNtfy(void);
#endif
#ifdef VIA2_iCB1_PulseNtfy
extern void VIA2_iCB1_PulseNtfy(void);
#endif
#ifdef VIA2_iCB2_PulseNtfy
extern void VIA2_iCB2_PulseNtfy(void);
#endif
extern void VIA2_DoTimer1Check(void);
extern void VIA2_DoTimer2Check(void);
extern uint16_t VIA2_GetT1InvertTime(void);
extern void VIA2_ShiftInData(uint8_t v);
extern uint8_t VIA2_ShiftOutData(void);
"""

via2_cpp = r"""/*
	VIA2EMDV.c

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

	Emulates the VIA found in the Mac II (VIA2).
	Wrapped in VIA2Device class (Phase 4).

	This code adapted from vMac by Philip Cummins.
*/

#include "core/common.h"

#if EmVIA2

#include "devices/via2.h"

/*
	ReportAbnormalID unused 0x0510 - 0x05FF
*/

/* ChangeNtfy externs - these are #define aliases in CNFUDPIC.h */

#ifdef VIA2_iA0_ChangeNtfy
extern void VIA2_iA0_ChangeNtfy(void);
#endif
#ifdef VIA2_iA1_ChangeNtfy
extern void VIA2_iA1_ChangeNtfy(void);
#endif
#ifdef VIA2_iA2_ChangeNtfy
extern void VIA2_iA2_ChangeNtfy(void);
#endif
#ifdef VIA2_iA3_ChangeNtfy
extern void VIA2_iA3_ChangeNtfy(void);
#endif
#ifdef VIA2_iA4_ChangeNtfy
extern void VIA2_iA4_ChangeNtfy(void);
#endif
#ifdef VIA2_iA5_ChangeNtfy
extern void VIA2_iA5_ChangeNtfy(void);
#endif
#ifdef VIA2_iA6_ChangeNtfy
extern void VIA2_iA6_ChangeNtfy(void);
#endif
#ifdef VIA2_iA7_ChangeNtfy
extern void VIA2_iA7_ChangeNtfy(void);
#endif
#ifdef VIA2_iB0_ChangeNtfy
extern void VIA2_iB0_ChangeNtfy(void);
#endif
#ifdef VIA2_iB1_ChangeNtfy
extern void VIA2_iB1_ChangeNtfy(void);
#endif
#ifdef VIA2_iB2_ChangeNtfy
extern void VIA2_iB2_ChangeNtfy(void);
#endif
#ifdef VIA2_iB3_ChangeNtfy
extern void VIA2_iB3_ChangeNtfy(void);
#endif
#ifdef VIA2_iB4_ChangeNtfy
extern void VIA2_iB4_ChangeNtfy(void);
#endif
#ifdef VIA2_iB5_ChangeNtfy
extern void VIA2_iB5_ChangeNtfy(void);
#endif
#ifdef VIA2_iB6_ChangeNtfy
extern void VIA2_iB6_ChangeNtfy(void);
#endif
#ifdef VIA2_iB7_ChangeNtfy
extern void VIA2_iB7_ChangeNtfy(void);
#endif
#ifdef VIA2_iCB2_ChangeNtfy
extern void VIA2_iCB2_ChangeNtfy(void);
#endif

#define Ui3rPowOf2(p) (1 << (p))
#define Ui3rTestBit(i, p) (((i) & Ui3rPowOf2(p)) != 0)

#define VIA2_ORA_CanInOrOut (VIA2_ORA_CanIn | VIA2_ORA_CanOut)
#define VIA2_ORB_CanInOrOut (VIA2_ORB_CanIn | VIA2_ORB_CanOut)

#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 7)
#ifdef VIA2_iA7
#error "VIA2_iA7 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 6)
#ifdef VIA2_iA6
#error "VIA2_iA6 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 5)
#ifdef VIA2_iA5
#error "VIA2_iA5 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 4)
#ifdef VIA2_iA4
#error "VIA2_iA4 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 3)
#ifdef VIA2_iA3
#error "VIA2_iA3 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 2)
#ifdef VIA2_iA2
#error "VIA2_iA2 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 1)
#ifdef VIA2_iA1
#error "VIA2_iA1 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORA_CanInOrOut, 0)
#ifdef VIA2_iA0
#error "VIA2_iA0 defined but not used"
#endif
#endif

#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 7)
#ifdef VIA2_iB7
#error "VIA2_iB7 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 6)
#ifdef VIA2_iB6
#error "VIA2_iB6 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 5)
#ifdef VIA2_iB5
#error "VIA2_iB5 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 4)
#ifdef VIA2_iB4
#error "VIA2_iB4 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 3)
#ifdef VIA2_iB3
#error "VIA2_iB3 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 2)
#ifdef VIA2_iB2
#error "VIA2_iB2 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 1)
#ifdef VIA2_iB1
#error "VIA2_iB1 defined but not used"
#endif
#endif
#if ! Ui3rTestBit(VIA2_ORB_CanInOrOut, 0)
#ifdef VIA2_iB0
#error "VIA2_iB0 defined but not used"
#endif
#endif

#define kIntCA2 0 /* One_Second */
#define kIntCA1 1 /* Vertical_Blanking */
#define kIntSR 2 /* Keyboard_Data_Ready */
#define kIntCB2 3 /* Keyboard_Data */
#define kIntCB1 4 /* Keyboard_Clock */
#define kIntT2 5 /* Timer_2 */
#define kIntT1 6 /* Timer_1 */

#define VIA2_dolog (dbglog_HAVE && 0)

/* Global singleton */
VIA2Device* g_via2 = nullptr;

/* ===== VIA2Device method implementations ===== */

uint8_t VIA2Device::getORA(uint8_t Selection)
{
	uint8_t Value = (~ VIA2_ORA_CanIn) & Selection & VIA2_ORA_FloatVal;

#if Ui3rTestBit(VIA2_ORA_CanIn, 7)
	if (Ui3rTestBit(Selection, 7)) {
		Value |= (VIA2_iA7 << 7);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 6)
	if (Ui3rTestBit(Selection, 6)) {
		Value |= (VIA2_iA6 << 6);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 5)
	if (Ui3rTestBit(Selection, 5)) {
		Value |= (VIA2_iA5 << 5);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 4)
	if (Ui3rTestBit(Selection, 4)) {
		Value |= (VIA2_iA4 << 4);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 3)
	if (Ui3rTestBit(Selection, 3)) {
		Value |= (VIA2_iA3 << 3);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 2)
	if (Ui3rTestBit(Selection, 2)) {
		Value |= (VIA2_iA2 << 2);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 1)
	if (Ui3rTestBit(Selection, 1)) {
		Value |= (VIA2_iA1 << 1);
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanIn, 0)
	if (Ui3rTestBit(Selection, 0)) {
		Value |= (VIA2_iA0 << 0);
	}
#endif

	return Value;
}

uint8_t VIA2Device::getORB(uint8_t Selection)
{
	uint8_t Value = (~ VIA2_ORB_CanIn) & Selection & VIA2_ORB_FloatVal;

#if Ui3rTestBit(VIA2_ORB_CanIn, 7)
	if (Ui3rTestBit(Selection, 7)) {
		Value |= (VIA2_iB7 << 7);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 6)
	if (Ui3rTestBit(Selection, 6)) {
		Value |= (VIA2_iB6 << 6);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 5)
	if (Ui3rTestBit(Selection, 5)) {
		Value |= (VIA2_iB5 << 5);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 4)
	if (Ui3rTestBit(Selection, 4)) {
		Value |= (VIA2_iB4 << 4);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 3)
	if (Ui3rTestBit(Selection, 3)) {
		Value |= (VIA2_iB3 << 3);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 2)
	if (Ui3rTestBit(Selection, 2)) {
		Value |= (VIA2_iB2 << 2);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 1)
	if (Ui3rTestBit(Selection, 1)) {
		Value |= (VIA2_iB1 << 1);
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanIn, 0)
	if (Ui3rTestBit(Selection, 0)) {
		Value |= (VIA2_iB0 << 0);
	}
#endif

	return Value;
}

#define ViaORcheckBit(p, x) \
	(Ui3rTestBit(Selection, p) && \
	((v = (Data >> p) & 1) != x))

void VIA2Device::putORA(uint8_t Selection, uint8_t Data)
{
#if 0 != VIA2_ORA_CanOut
	uint8_t v;
#endif

#if Ui3rTestBit(VIA2_ORA_CanOut, 7)
	if (ViaORcheckBit(7, VIA2_iA7)) {
		VIA2_iA7 = v;
#ifdef VIA2_iA7_ChangeNtfy
		VIA2_iA7_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 6)
	if (ViaORcheckBit(6, VIA2_iA6)) {
		VIA2_iA6 = v;
#ifdef VIA2_iA6_ChangeNtfy
		VIA2_iA6_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 5)
	if (ViaORcheckBit(5, VIA2_iA5)) {
		VIA2_iA5 = v;
#ifdef VIA2_iA5_ChangeNtfy
		VIA2_iA5_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 4)
	if (ViaORcheckBit(4, VIA2_iA4)) {
		VIA2_iA4 = v;
#ifdef VIA2_iA4_ChangeNtfy
		VIA2_iA4_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 3)
	if (ViaORcheckBit(3, VIA2_iA3)) {
		VIA2_iA3 = v;
#ifdef VIA2_iA3_ChangeNtfy
		VIA2_iA3_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 2)
	if (ViaORcheckBit(2, VIA2_iA2)) {
		VIA2_iA2 = v;
#ifdef VIA2_iA2_ChangeNtfy
		VIA2_iA2_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 1)
	if (ViaORcheckBit(1, VIA2_iA1)) {
		VIA2_iA1 = v;
#ifdef VIA2_iA1_ChangeNtfy
		VIA2_iA1_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORA_CanOut, 0)
	if (ViaORcheckBit(0, VIA2_iA0)) {
		VIA2_iA0 = v;
#ifdef VIA2_iA0_ChangeNtfy
		VIA2_iA0_ChangeNtfy();
#endif
	}
#endif
}

void VIA2Device::putORB(uint8_t Selection, uint8_t Data)
{
#if 0 != VIA2_ORB_CanOut
	uint8_t v;
#endif

#if Ui3rTestBit(VIA2_ORB_CanOut, 7)
	if (ViaORcheckBit(7, VIA2_iB7)) {
		VIA2_iB7 = v;
#ifdef VIA2_iB7_ChangeNtfy
		VIA2_iB7_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 6)
	if (ViaORcheckBit(6, VIA2_iB6)) {
		VIA2_iB6 = v;
#ifdef VIA2_iB6_ChangeNtfy
		VIA2_iB6_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 5)
	if (ViaORcheckBit(5, VIA2_iB5)) {
		VIA2_iB5 = v;
#ifdef VIA2_iB5_ChangeNtfy
		VIA2_iB5_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 4)
	if (ViaORcheckBit(4, VIA2_iB4)) {
		VIA2_iB4 = v;
#ifdef VIA2_iB4_ChangeNtfy
		VIA2_iB4_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 3)
	if (ViaORcheckBit(3, VIA2_iB3)) {
		VIA2_iB3 = v;
#ifdef VIA2_iB3_ChangeNtfy
		VIA2_iB3_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 2)
	if (ViaORcheckBit(2, VIA2_iB2)) {
		VIA2_iB2 = v;
#ifdef VIA2_iB2_ChangeNtfy
		VIA2_iB2_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 1)
	if (ViaORcheckBit(1, VIA2_iB1)) {
		VIA2_iB1 = v;
#ifdef VIA2_iB1_ChangeNtfy
		VIA2_iB1_ChangeNtfy();
#endif
	}
#endif
#if Ui3rTestBit(VIA2_ORB_CanOut, 0)
	if (ViaORcheckBit(0, VIA2_iB0)) {
		VIA2_iB0 = v;
#ifdef VIA2_iB0_ChangeNtfy
		VIA2_iB0_ChangeNtfy();
#endif
	}
#endif
}

void VIA2Device::setDDR_A(uint8_t Data)
{
	uint8_t floatbits = d_.DDR_A & ~ Data;
	uint8_t unfloatbits = Data & ~ d_.DDR_A;

	if (floatbits != 0) {
		putORA(floatbits, VIA2_ORA_FloatVal);
	}
	d_.DDR_A = Data;
	if (unfloatbits != 0) {
		putORA(unfloatbits, d_.ORA);
	}
	if ((Data & ~ VIA2_ORA_CanOut) != 0) {
		ReportAbnormalID(0x0501,
			"Set VIA2_D.DDR_A unexpected direction");
	}
}

void VIA2Device::setDDR_B(uint8_t Data)
{
	uint8_t floatbits = d_.DDR_B & ~ Data;
	uint8_t unfloatbits = Data & ~ d_.DDR_B;

	if (floatbits != 0) {
		putORB(floatbits, VIA2_ORB_FloatVal);
	}
	d_.DDR_B = Data;
	if (unfloatbits != 0) {
		putORB(unfloatbits, d_.ORB);
	}
	if ((Data & ~ VIA2_ORB_CanOut) != 0) {
		ReportAbnormalID(0x0502,
			"Set VIA2_D.DDR_B unexpected direction");
	}
}

void VIA2Device::checkInterruptFlag()
{
	uint8_t NewInterruptRequest =
		((d_.IFR & d_.IER) != 0) ? 1 : 0;

	if (NewInterruptRequest != VIA2_InterruptRequest) {
		VIA2_InterruptRequest = NewInterruptRequest;
#ifdef VIA2_interruptChngNtfy
		VIA2_interruptChngNtfy();
#endif
	}
}

void VIA2Device::setInterruptFlag(uint8_t VIA_Int)
{
	d_.IFR |= ((uint8_t)1 << VIA_Int);
	checkInterruptFlag();
}

void VIA2Device::clrInterruptFlag(uint8_t VIA_Int)
{
	d_.IFR &= ~ ((uint8_t)1 << VIA_Int);
	checkInterruptFlag();
}

void VIA2Device::clear()
{
	d_.ORA   = 0; d_.DDR_A = 0;
	d_.ORB   = 0; d_.DDR_B = 0;
	d_.T1L_L = d_.T1L_H = 0x00;
	d_.T2L_L = 0x00;
	d_.T1C_F = 0;
	d_.T2C_F = 0;
	d_.SR = d_.ACR = 0x00;
	d_.PCR   = d_.IFR   = d_.IER   = 0x00;
	T1_Active = T2_Active = 0x00;
	T1IntReady = false;
}

void VIA2Device::zap()
{
	clear();
	VIA2_InterruptRequest = 0;
}

void VIA2Device::reset()
{
	setDDR_A(0);
	setDDR_B(0);

	clear();

	checkInterruptFlag();
}

void VIA2Device::shiftInData(uint8_t v)
{
	uint8_t ShiftMode = (d_.ACR & 0x1C) >> 2;

	if (ShiftMode != 3) {
#if ExtraAbnormalReports
		if (ShiftMode == 0) {
			/* happens on reset */
		} else {
			ReportAbnormalID(0x0503, "VIA Not ready to shift in");
		}
#endif
	} else {
		d_.SR = v;
		setInterruptFlag(kIntSR);
		setInterruptFlag(kIntCB1);
	}
}

uint8_t VIA2Device::shiftOutData()
{
	if (((d_.ACR & 0x1C) >> 2) != 7) {
		ReportAbnormalID(0x0504, "VIA Not ready to shift out");
		return 0;
	} else {
		setInterruptFlag(kIntSR);
		setInterruptFlag(kIntCB1);
		VIA2_iCB2 = (d_.SR & 1);
		return d_.SR;
	}
}

#define CyclesPerViaTime (10 * kMyClockMult)
#define CyclesScaledPerViaTime (kCycleScale * CyclesPerViaTime)

void VIA2Device::doTimer1Check()
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
#if Ui3rTestBit(VIA2_ORB_CanOut, 7)
					if ((d_.ACR & 0x80) != 0) { /* invert ? */
						if ((ntrans & 1) != 0) {
							VIA2_iB7 ^= 1;
#ifdef VIA2_iB7_ChangeNtfy
							VIA2_iB7_ChangeNtfy();
#endif
						}
					}
#endif
					setInterruptFlag(kIntT1);
#if VIA2_dolog && 1
					dbglog_WriteNote("VIA2 Timer 1 Interrupt");
#endif
				} else {
					if (T1_Active == 1) {
						T1_Active = 0;
						setInterruptFlag(kIntT1);
#if VIA2_dolog && 1
						dbglog_WriteNote("VIA2 Timer 1 Interrupt");
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
					Temp, GetCuriCount());
#endif
				if (NewTemp == 0) {
					NewTimer = (0x00010000UL * CyclesScaledPerViaTime);
				} else {
					NewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale)))
						* CyclesPerViaTime;
				}
				ICT_add(kICT_VIA2_Timer1Check, NewTimer);
				T1IntReady = true;
			}
		}
	}
}

void VIA2Device::checkT1IntReady()
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

uint16_t VIA2Device::getT1InvertTime()
{
	uint16_t v;

	if ((d_.ACR & 0xC0) == 0xC0) {
		v = (d_.T1L_H << 8) + d_.T1L_L;
	} else {
		v = 0;
	}
	return v;
}

void VIA2Device::doTimer2Check()
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
#if VIA2_dolog && 1
				dbglog_WriteNote("VIA2 Timer 2 Interrupt");
#endif
			} else {
				uint32_t NewTimer;
#ifdef _VIA_Debug
				fprintf(stderr, "posting Timer2Check, %d, %d\n",
					Temp, GetCuriCount());
#endif
#if VIA2_dolog
				dbglog_WriteNote("VIA2 Timer 2 Later");
#endif
				if (NewTemp == 0) {
					NewTimer = (0x00010000UL * CyclesScaledPerViaTime);
				} else {
					NewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale)))
						* CyclesPerViaTime;
				}
				ICT_add(kICT_VIA2_Timer2Check, NewTimer);
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

uint32_t VIA2Device::access(uint32_t Data, bool WriteMem, uint32_t addr)
{
	switch (addr) {
		case kORB   :
#if VIA2_CB2modesAllowed != 0x01
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kORB", Data, WriteMem);
#endif
			break;
		case kDDR_B :
			if (WriteMem) {
				setDDR_B(Data);
			} else {
				Data = d_.DDR_B;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kDDR_B", Data, WriteMem);
#endif
			break;
		case kDDR_A :
			if (WriteMem) {
				setDDR_A(Data);
			} else {
				Data = d_.DDR_A;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kDDR_A", Data, WriteMem);
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kT1C_L", Data, WriteMem);
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kT1C_H", Data, WriteMem);
#endif
			break;
		case kT1L_L :
			if (WriteMem) {
				d_.T1L_L = Data;
			} else {
				Data = d_.T1L_L;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kT1L_L", Data, WriteMem);
#endif
			break;
		case kT1L_H :
			if (WriteMem) {
				d_.T1L_H = Data;
			} else {
				Data = d_.T1L_H;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kT1L_H", Data, WriteMem);
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kT2_L", Data, WriteMem);
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
#if VIA2_dolog
					dbglog_StartLine();
					dbglog_writeCStr("VIA2_T2C_ShortTime ");
					dbglog_writeHex(d_.T2C_F);
					dbglog_writeCStr(", IER ");
					dbglog_writeHex(d_.IER);
					dbglog_writeCStr(", VIA2_T2Running ");
					dbglog_writeHex(T2Running);
					dbglog_writeCStr(", VIA2_T2C_ShortTime ");
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kT2_H", Data, WriteMem);
#endif
			break;
		case kSR:
#ifdef _VIA_Debug
			fprintf(stderr, "VIA2_D.SR: %d, %d, %d\n",
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
						ReportAbnormalID(0x0505,
							"VIA shift mode 6, non zero");
					} else {
#ifdef _VIA_Debug
						fprintf(stderr, "posting Foo2Task\n");
#endif
						if (VIA2_iCB2 != 0) {
							VIA2_iCB2 = 0;
#ifdef VIA2_iCB2_ChangeNtfy
							VIA2_iCB2_ChangeNtfy();
#endif
						}
					}
#if 0 /* possibly should do this. seems not to affect anything. */
					setInterruptFlag(kIntSR); /* don't wait */
#endif
					break;
				case 7 : /* Shifting Out */
					break;
			}
			if (! WriteMem) {
				Data = d_.SR;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kSR", Data, WriteMem);
#endif
			break;
		case kACR:
			if (WriteMem) {
#if 1
				if ((d_.ACR & 0x10) != ((uint8_t)Data & 0x10)) {
					if ((Data & 0x10) == 0) {
						if (VIA2_iCB2 == 0) {
							VIA2_iCB2 = 1;
#ifdef VIA2_iCB2_ChangeNtfy
							VIA2_iCB2_ChangeNtfy();
#endif
						}
					}
				}
#endif
				d_.ACR = Data;
				if ((d_.ACR & 0x20) != 0) {
					ReportAbnormalID(0x0506,
						"Set VIA2_D.ACR T2 Timer pulse counting");
				}
				switch ((d_.ACR & 0xC0) >> 6) {
					case 2:
						ReportAbnormalID(0x0507,
							"Set VIA2_D.ACR T1 Timer mode 2");
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
						ReportAbnormalID(0x0508,
							"Set VIA2_D.ACR shift mode 1,2,4,5");
						break;
					default:
						break;
				}
				if ((d_.ACR & 0x03) != 0) {
					ReportAbnormalID(0x0509,
						"Set VIA2_D.ACR T2 Timer latching enabled");
				}
			} else {
				Data = d_.ACR;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kACR", Data, WriteMem);
#endif
			break;
		case kPCR:
			if (WriteMem) {
				d_.PCR = Data;
#define Ui3rSetContains(s, i) (((s) & (1 << (i))) != 0)
				if (! Ui3rSetContains(VIA2_CB2modesAllowed,
					(d_.PCR >> 5) & 0x07))
				{
					ReportAbnormalID(0x050A,
						"Set VIA2_D.PCR CB2 Control mode?");
				}
				if ((d_.PCR & 0x10) != 0) {
					ReportAbnormalID(0x050B,
						"Set VIA2_D.PCR CB1 INTERRUPT CONTROL?");
				}
				if (! Ui3rSetContains(VIA2_CA2modesAllowed,
					(d_.PCR >> 1) & 0x07))
				{
					ReportAbnormalID(0x050C,
						"Set VIA2_D.PCR CA2 INTERRUPT CONTROL?");
				}
				if ((d_.PCR & 0x01) != 0) {
					ReportAbnormalID(0x050D,
						"Set VIA2_D.PCR CA1 INTERRUPT CONTROL?");
				}
			} else {
				Data = d_.PCR;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kPCR", Data, WriteMem);
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kIFR", Data, WriteMem);
#endif
			break;
		case kIER   :
			if (WriteMem) {
				if ((Data & 0x80) == 0) {
					d_.IER = d_.IER & ((~ Data) & 0x7F);
#if 0 != VIA2_IER_Never0
					if ((Data & VIA2_IER_Never0) != 0) {
						ReportAbnormalID(0x050E, "IER Never0 clr");
					}
#endif
				} else {
					d_.IER = d_.IER | (Data & 0x7F);
#if 0 != VIA2_IER_Never1
					if ((d_.IER & VIA2_IER_Never1) != 0) {
						ReportAbnormalID(0x050F, "IER Never1 set");
					}
#endif
				}
				checkInterruptFlag();
			} else {
				Data = d_.IER | 0x80;
			}
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kIER", Data, WriteMem);
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
#if VIA2_dolog && 1
			dbglog_Access("VIA2_Access kORA", Data, WriteMem);
#endif
			break;
	}
	return Data;
}

void VIA2Device::extraTimeBegin()
{
#if VIA2_dolog
	dbglog_WriteNote("VIA2_ExtraTimeBegin");
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

void VIA2Device::extraTimeEnd()
{
#if VIA2_dolog
	dbglog_WriteNote("VIA2_ExtraTimeEnd");
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

/* Pulse notifications */

void VIA2Device::iCA1_PulseNtfy()
{
	setInterruptFlag(kIntCA1);
}

void VIA2Device::iCA2_PulseNtfy()
{
	setInterruptFlag(kIntCA2);
}

void VIA2Device::iCB1_PulseNtfy()
{
	setInterruptFlag(kIntCB1);
}

void VIA2Device::iCB2_PulseNtfy()
{
	setInterruptFlag(kIntCB2);
}

/* ===== Backward-compatible free function API ===== */

void VIA2_Zap(void)
{
	g_via2->zap();
}

void VIA2_Reset(void)
{
	g_via2->reset();
}

uint32_t VIA2_Access(uint32_t Data, bool WriteMem, uint32_t addr)
{
	return g_via2->access(Data, WriteMem, addr);
}

void VIA2_ExtraTimeBegin(void)
{
	g_via2->extraTimeBegin();
}

void VIA2_ExtraTimeEnd(void)
{
	g_via2->extraTimeEnd();
}

void VIA2_DoTimer1Check(void)
{
	g_via2->doTimer1Check();
}

void VIA2_DoTimer2Check(void)
{
	g_via2->doTimer2Check();
}

uint16_t VIA2_GetT1InvertTime(void)
{
	return g_via2->getT1InvertTime();
}

void VIA2_ShiftInData(uint8_t v)
{
	g_via2->shiftInData(v);
}

uint8_t VIA2_ShiftOutData(void)
{
	return g_via2->shiftOutData();
}

/* Pulse notifications - called through #define aliases in CNFUDPIC.h */

void VIA2_iCA1_Vid_VBLinterrupt_PulseNtfy(void)
{
	g_via2->iCA1_PulseNtfy();
}

void VIA2_iCB1_ASC_interrupt_PulseNtfy(void)
{
	g_via2->iCB1_PulseNtfy();
}

#endif /* EmVIA2 */
"""

with open(os.path.join(BASE, "src", "devices", "via2.h"), "w") as f:
    f.write(via2_h.lstrip("\n"))

print("via2.h written")

with open(os.path.join(BASE, "src", "devices", "via2.cpp"), "w") as f:
    f.write(via2_cpp.lstrip("\n"))

print("via2.cpp written")
