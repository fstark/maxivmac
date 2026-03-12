#!/usr/bin/env python3
"""Write via.h and via.cpp for VIA1Device class (Step 4.6)"""
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ===== via.h =====
via_h = '''\
/*
\tVIAEMDEV.h

\tCopyright (C) 2004 Philip Cummins, Paul C. Pratt

\tYou can redistribute this file and/or modify it under the terms
\tof version 2 of the GNU General Public License as published by
\tthe Free Software Foundation.  You should have received a copy
\tof the license along with this file; see the file COPYING.

\tThis file is distributed in the hope that it will be useful,
\tbut WITHOUT ANY WARRANTY; without even the implied warranty of
\tMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
\tlicense for more details.
*/

#pragma once

#include "devices/device.h"
#include <cstdint>

// VIA1 Device class wrapping the original VIA1 emulation
class VIA1Device : public Device {
public:
\t// Device interface
\tuint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
\tvoid zap() override;
\tvoid reset() override;
\tconst char* name() const override { return "VIA1"; }

\t// Timer ICT callbacks
\tvoid doTimer1Check();
\tvoid doTimer2Check();

\t// Extra time (pause/resume timers during extra emulation cycles)
\tvoid extraTimeBegin();
\tvoid extraTimeEnd();

\t// Pulse notifications (interrupt sources)
\tvoid iCA1_PulseNtfy();
\tvoid iCA2_PulseNtfy();
\tvoid iCB1_PulseNtfy();
\tvoid iCB2_PulseNtfy();

\t// Shift register
\tvoid shiftInData(uint8_t v);
\tuint8_t shiftOutData();

\t// Timer invert time
\tuint16_t getT1InvertTime();

\t// Internal state - public for backward compatibility during migration
\tstruct VIA_Ty {
\t\tuint32_t T1C_F;  /* Timer 1 Counter Fixed Point */
\t\tuint32_t T2C_F;  /* Timer 2 Counter Fixed Point */
\t\tuint8_t ORB;     /* Buffer B */
\t\tuint8_t DDR_B;   /* Data Direction Register B */
\t\tuint8_t DDR_A;   /* Data Direction Register A */
\t\tuint8_t T1L_L;   /* Timer 1 Latch Low */
\t\tuint8_t T1L_H;   /* Timer 1 Latch High */
\t\tuint8_t T2L_L;   /* Timer 2 Latch Low */
\t\tuint8_t SR;      /* Shift Register */
\t\tuint8_t ACR;     /* Auxiliary Control Register */
\t\tuint8_t PCR;     /* Peripheral Control Register */
\t\tuint8_t IFR;     /* Interrupt Flag Register */
\t\tuint8_t IER;     /* Interrupt Enable Register */
\t\tuint8_t ORA;     /* Buffer A */
\t};

\tVIA_Ty d_{};

\tuint8_t T1_Active = 0;
\tuint8_t T2_Active = 0;
\tbool T1IntReady = false;
\tbool T1Running = true;
\tuint32_t T1LastTime = 0;
\tbool T2Running = true;
\tbool T2C_ShortTime = false;
\tuint32_t T2LastTime = 0;

private:
\tuint8_t getORA(uint8_t selection);
\tuint8_t getORB(uint8_t selection);
\tvoid putORA(uint8_t selection, uint8_t data);
\tvoid putORB(uint8_t selection, uint8_t data);
\tvoid setDDR_A(uint8_t data);
\tvoid setDDR_B(uint8_t data);
\tvoid checkInterruptFlag();
\tvoid setInterruptFlag(uint8_t viaInt);
\tvoid clrInterruptFlag(uint8_t viaInt);
\tvoid clear();
\tvoid checkT1IntReady();
};

// Global singleton pointer (for backward compatibility during migration)
extern VIA1Device* g_via1;

// Backward-compatible free function API (forwards to g_via1)
extern void VIA1_Zap(void);
extern void VIA1_Reset(void);
extern uint32_t VIA1_Access(uint32_t Data, bool WriteMem, uint32_t addr);
extern void VIA1_ExtraTimeBegin(void);
extern void VIA1_ExtraTimeEnd(void);
#ifdef VIA1_iCA1_PulseNtfy
extern void VIA1_iCA1_PulseNtfy(void);
#endif
#ifdef VIA1_iCA2_PulseNtfy
extern void VIA1_iCA2_PulseNtfy(void);
#endif
#ifdef VIA1_iCB1_PulseNtfy
extern void VIA1_iCB1_PulseNtfy(void);
#endif
#ifdef VIA1_iCB2_PulseNtfy
extern void VIA1_iCB2_PulseNtfy(void);
#endif
extern void VIA1_DoTimer1Check(void);
extern void VIA1_DoTimer2Check(void);
extern uint16_t VIA1_GetT1InvertTime(void);
extern void VIA1_ShiftInData(uint8_t v);
extern uint8_t VIA1_ShiftOutData(void);
'''

with open(os.path.join(BASE, 'src/devices/via.h'), 'w') as f:
    f.write(via_h)
print("via.h written")

# ===== via.cpp =====
# Read original to preserve as reference, then write the new version
via_cpp = '''\
/*
\tVIAEMDEV.c

\tCopyright (C) 2008 Philip Cummins, Paul C. Pratt

\tYou can redistribute this file and/or modify it under the terms
\tof version 2 of the GNU General Public License as published by
\tthe Free Software Foundation.  You should have received a copy
\tof the license along with this file; see the file COPYING.

\tThis file is distributed in the hope that it will be useful,
\tbut WITHOUT ANY WARRANTY; without even the implied warranty of
\tMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
\tlicense for more details.
*/

/*
\tVersatile Interface Adapter EMulated DEVice

\tEmulates the VIA found in the Mac Plus and Mac II.
\tWrapped in VIA1Device class (Phase 4).

\tThis code adapted from vMac by Philip Cummins.
*/

#include "core/common.h"

#if EmVIA1

#include "devices/via.h"

/*
\tReportAbnormalID unused 0x0410 - 0x04FF
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
\tuint8_t Value = (~ VIA1_ORA_CanIn) & Selection & VIA1_ORA_FloatVal;

#if Ui3rTestBit(VIA1_ORA_CanIn, 7)
\tif (Ui3rTestBit(Selection, 7)) {
\t\tValue |= (VIA1_iA7 << 7);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 6)
\tif (Ui3rTestBit(Selection, 6)) {
\t\tValue |= (VIA1_iA6 << 6);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 5)
\tif (Ui3rTestBit(Selection, 5)) {
\t\tValue |= (VIA1_iA5 << 5);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 4)
\tif (Ui3rTestBit(Selection, 4)) {
\t\tValue |= (VIA1_iA4 << 4);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 3)
\tif (Ui3rTestBit(Selection, 3)) {
\t\tValue |= (VIA1_iA3 << 3);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 2)
\tif (Ui3rTestBit(Selection, 2)) {
\t\tValue |= (VIA1_iA2 << 2);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 1)
\tif (Ui3rTestBit(Selection, 1)) {
\t\tValue |= (VIA1_iA1 << 1);
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 0)
\tif (Ui3rTestBit(Selection, 0)) {
\t\tValue |= (VIA1_iA0 << 0);
\t}
#endif

\treturn Value;
}

uint8_t VIA1Device::getORB(uint8_t Selection)
{
\tuint8_t Value = (~ VIA1_ORB_CanIn) & Selection & VIA1_ORB_FloatVal;

#if Ui3rTestBit(VIA1_ORB_CanIn, 7)
\tif (Ui3rTestBit(Selection, 7)) {
\t\tValue |= (VIA1_iB7 << 7);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 6)
\tif (Ui3rTestBit(Selection, 6)) {
\t\tValue |= (VIA1_iB6 << 6);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 5)
\tif (Ui3rTestBit(Selection, 5)) {
\t\tValue |= (VIA1_iB5 << 5);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 4)
\tif (Ui3rTestBit(Selection, 4)) {
\t\tValue |= (VIA1_iB4 << 4);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 3)
\tif (Ui3rTestBit(Selection, 3)) {
\t\tValue |= (VIA1_iB3 << 3);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 2)
\tif (Ui3rTestBit(Selection, 2)) {
\t\tValue |= (VIA1_iB2 << 2);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 1)
\tif (Ui3rTestBit(Selection, 1)) {
\t\tValue |= (VIA1_iB1 << 1);
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 0)
\tif (Ui3rTestBit(Selection, 0)) {
\t\tValue |= (VIA1_iB0 << 0);
\t}
#endif

\treturn Value;
}

#define ViaORcheckBit(p, x) \\
\t(Ui3rTestBit(Selection, p) && \\
\t((v = (Data >> p) & 1) != x))

void VIA1Device::putORA(uint8_t Selection, uint8_t Data)
{
#if 0 != VIA1_ORA_CanOut
\tuint8_t v;
#endif

#if Ui3rTestBit(VIA1_ORA_CanOut, 7)
\tif (ViaORcheckBit(7, VIA1_iA7)) {
\t\tVIA1_iA7 = v;
#ifdef VIA1_iA7_ChangeNtfy
\t\tVIA1_iA7_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 6)
\tif (ViaORcheckBit(6, VIA1_iA6)) {
\t\tVIA1_iA6 = v;
#ifdef VIA1_iA6_ChangeNtfy
\t\tVIA1_iA6_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 5)
\tif (ViaORcheckBit(5, VIA1_iA5)) {
\t\tVIA1_iA5 = v;
#ifdef VIA1_iA5_ChangeNtfy
\t\tVIA1_iA5_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 4)
\tif (ViaORcheckBit(4, VIA1_iA4)) {
\t\tVIA1_iA4 = v;
#ifdef VIA1_iA4_ChangeNtfy
\t\tVIA1_iA4_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 3)
\tif (ViaORcheckBit(3, VIA1_iA3)) {
\t\tVIA1_iA3 = v;
#ifdef VIA1_iA3_ChangeNtfy
\t\tVIA1_iA3_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 2)
\tif (ViaORcheckBit(2, VIA1_iA2)) {
\t\tVIA1_iA2 = v;
#ifdef VIA1_iA2_ChangeNtfy
\t\tVIA1_iA2_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 1)
\tif (ViaORcheckBit(1, VIA1_iA1)) {
\t\tVIA1_iA1 = v;
#ifdef VIA1_iA1_ChangeNtfy
\t\tVIA1_iA1_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 0)
\tif (ViaORcheckBit(0, VIA1_iA0)) {
\t\tVIA1_iA0 = v;
#ifdef VIA1_iA0_ChangeNtfy
\t\tVIA1_iA0_ChangeNtfy();
#endif
\t}
#endif
}

void VIA1Device::putORB(uint8_t Selection, uint8_t Data)
{
#if 0 != VIA1_ORB_CanOut
\tuint8_t v;
#endif

#if Ui3rTestBit(VIA1_ORB_CanOut, 7)
\tif (ViaORcheckBit(7, VIA1_iB7)) {
\t\tVIA1_iB7 = v;
#ifdef VIA1_iB7_ChangeNtfy
\t\tVIA1_iB7_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 6)
\tif (ViaORcheckBit(6, VIA1_iB6)) {
\t\tVIA1_iB6 = v;
#ifdef VIA1_iB6_ChangeNtfy
\t\tVIA1_iB6_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 5)
\tif (ViaORcheckBit(5, VIA1_iB5)) {
\t\tVIA1_iB5 = v;
#ifdef VIA1_iB5_ChangeNtfy
\t\tVIA1_iB5_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 4)
\tif (ViaORcheckBit(4, VIA1_iB4)) {
\t\tVIA1_iB4 = v;
#ifdef VIA1_iB4_ChangeNtfy
\t\tVIA1_iB4_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 3)
\tif (ViaORcheckBit(3, VIA1_iB3)) {
\t\tVIA1_iB3 = v;
#ifdef VIA1_iB3_ChangeNtfy
\t\tVIA1_iB3_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 2)
\tif (ViaORcheckBit(2, VIA1_iB2)) {
\t\tVIA1_iB2 = v;
#ifdef VIA1_iB2_ChangeNtfy
\t\tVIA1_iB2_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 1)
\tif (ViaORcheckBit(1, VIA1_iB1)) {
\t\tVIA1_iB1 = v;
#ifdef VIA1_iB1_ChangeNtfy
\t\tVIA1_iB1_ChangeNtfy();
#endif
\t}
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 0)
\tif (ViaORcheckBit(0, VIA1_iB0)) {
\t\tVIA1_iB0 = v;
#ifdef VIA1_iB0_ChangeNtfy
\t\tVIA1_iB0_ChangeNtfy();
#endif
\t}
#endif
}

void VIA1Device::setDDR_A(uint8_t Data)
{
\tuint8_t floatbits = d_.DDR_A & ~ Data;
\tuint8_t unfloatbits = Data & ~ d_.DDR_A;

\tif (floatbits != 0) {
\t\tputORA(floatbits, VIA1_ORA_FloatVal);
\t}
\td_.DDR_A = Data;
\tif (unfloatbits != 0) {
\t\tputORA(unfloatbits, d_.ORA);
\t}
\tif ((Data & ~ VIA1_ORA_CanOut) != 0) {
\t\tReportAbnormalID(0x0401, "Set d_.DDR_A unexpected direction");
\t}
}

void VIA1Device::setDDR_B(uint8_t Data)
{
\tuint8_t floatbits = d_.DDR_B & ~ Data;
\tuint8_t unfloatbits = Data & ~ d_.DDR_B;

\tif (floatbits != 0) {
\t\tputORB(floatbits, VIA1_ORB_FloatVal);
\t}
\td_.DDR_B = Data;
\tif (unfloatbits != 0) {
\t\tputORB(unfloatbits, d_.ORB);
\t}
\tif ((Data & ~ VIA1_ORB_CanOut) != 0) {
\t\tReportAbnormalID(0x0402, "Set d_.DDR_B unexpected direction");
\t}
}

void VIA1Device::checkInterruptFlag()
{
\tuint8_t NewInterruptRequest =
\t\t((d_.IFR & d_.IER) != 0) ? 1 : 0;

\tif (NewInterruptRequest != VIA1_InterruptRequest) {
\t\tVIA1_InterruptRequest = NewInterruptRequest;
#ifdef VIA1_interruptChngNtfy
\t\tVIA1_interruptChngNtfy();
#endif
\t}
}

void VIA1Device::setInterruptFlag(uint8_t VIA_Int)
{
\td_.IFR |= ((uint8_t)1 << VIA_Int);
\tcheckInterruptFlag();
}

void VIA1Device::clrInterruptFlag(uint8_t VIA_Int)
{
\td_.IFR &= ~ ((uint8_t)1 << VIA_Int);
\tcheckInterruptFlag();
}

void VIA1Device::clear()
{
\td_.ORA = 0; d_.DDR_A = 0;
\td_.ORB = 0; d_.DDR_B = 0;
\td_.T1L_L = d_.T1L_H = 0x00;
\td_.T2L_L = 0x00;
\td_.T1C_F = 0;
\td_.T2C_F = 0;
\td_.SR = d_.ACR = 0x00;
\td_.PCR = d_.IFR = d_.IER = 0x00;
\tT1_Active = T2_Active = 0x00;
\tT1IntReady = false;
}

void VIA1Device::zap()
{
\tclear();
\tVIA1_InterruptRequest = 0;
}

void VIA1Device::reset()
{
\tsetDDR_A(0);
\tsetDDR_B(0);
\tclear();
\tcheckInterruptFlag();
}

#ifdef _VIA_Debug
#include <stdio.h>
#endif

void VIA1Device::shiftInData(uint8_t v)
{
\tuint8_t ShiftMode = (d_.ACR & 0x1C) >> 2;

\tif (ShiftMode != 3) {
#if ExtraAbnormalReports
\t\tif (ShiftMode == 0) {
\t\t\t/* happens on reset */
\t\t} else {
\t\t\tReportAbnormalID(0x0403, "VIA Not ready to shift in");
\t\t}
#endif
\t} else {
\t\td_.SR = v;
\t\tsetInterruptFlag(kIntSR);
\t\tsetInterruptFlag(kIntCB1);
\t}
}

uint8_t VIA1Device::shiftOutData()
{
\tif (((d_.ACR & 0x1C) >> 2) != 7) {
\t\tReportAbnormalID(0x0404, "VIA Not ready to shift out");
\t\treturn 0;
\t} else {
\t\tsetInterruptFlag(kIntSR);
\t\tsetInterruptFlag(kIntCB1);
\t\tVIA1_iCB2 = (d_.SR & 1);
\t\treturn d_.SR;
\t}
}

#define CyclesPerViaTime (10 * kMyClockMult)
#define CyclesScaledPerViaTime (kCycleScale * CyclesPerViaTime)

void VIA1Device::doTimer1Check()
{
\tif (T1Running) {
\t\tiCountt NewTime = GetCuriCount();
\t\tiCountt deltaTime = (NewTime - T1LastTime);
\t\tif (deltaTime != 0) {
\t\t\tuint32_t Temp = d_.T1C_F;
\t\t\tuint32_t deltaTemp =
\t\t\t\t(deltaTime / CyclesPerViaTime) << (16 - kLn2CycleScale);
\t\t\tuint32_t NewTemp = Temp - deltaTemp;
\t\t\tif ((deltaTime > (0x00010000UL * CyclesScaledPerViaTime))
\t\t\t\t|| ((Temp <= deltaTemp) && (Temp != 0)))
\t\t\t{
\t\t\t\tif ((d_.ACR & 0x40) != 0) { /* Free Running? */
\t\t\t\t\tuint16_t v = (d_.T1L_H << 8) + d_.T1L_L;
\t\t\t\t\tuint16_t ntrans = 1 + ((v == 0) ? 0 :
\t\t\t\t\t\t(((deltaTemp - Temp) / v) >> 16));
\t\t\t\t\tNewTemp += (((uint32_t)v * ntrans) << 16);
#if Ui3rTestBit(VIA1_ORB_CanOut, 7)
\t\t\t\t\tif ((d_.ACR & 0x80) != 0) { /* invert ? */
\t\t\t\t\t\tif ((ntrans & 1) != 0) {
\t\t\t\t\t\t\tVIA1_iB7 ^= 1;
#ifdef VIA1_iB7_ChangeNtfy
\t\t\t\t\t\t\tVIA1_iB7_ChangeNtfy();
#endif
\t\t\t\t\t\t}
\t\t\t\t\t}
#endif
\t\t\t\t\tsetInterruptFlag(kIntT1);
#if VIA1_dolog && 1
\t\t\t\t\tdbglog_WriteNote("VIA1 Timer 1 Interrupt");
#endif
\t\t\t\t} else {
\t\t\t\t\tif (T1_Active == 1) {
\t\t\t\t\t\tT1_Active = 0;
\t\t\t\t\t\tsetInterruptFlag(kIntT1);
#if VIA1_dolog && 1
\t\t\t\t\t\tdbglog_WriteNote("VIA1 Timer 1 Interrupt");
#endif
\t\t\t\t\t}
\t\t\t\t}
\t\t\t}

\t\t\td_.T1C_F = NewTemp;
\t\t\tT1LastTime = NewTime;
\t\t}

\t\tT1IntReady = false;
\t\tif ((d_.IFR & (1 << kIntT1)) == 0) {
\t\t\tif (((d_.ACR & 0x40) != 0) || (T1_Active == 1)) {
\t\t\t\tuint32_t NewTemp = d_.T1C_F;
\t\t\t\tuint32_t NewTimer;
#ifdef _VIA_Debug
\t\t\t\tfprintf(stderr, "posting Timer1Check, %d, %d\\n",
\t\t\t\t\tNewTemp, GetCuriCount());
#endif
\t\t\t\tif (NewTemp == 0) {
\t\t\t\t\tNewTimer = (0x00010000UL * CyclesScaledPerViaTime);
\t\t\t\t} else {
\t\t\t\t\tNewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale)))
\t\t\t\t\t\t* CyclesPerViaTime;
\t\t\t\t}
\t\t\t\tICT_add(kICT_VIA1_Timer1Check, NewTimer);
\t\t\t\tT1IntReady = true;
\t\t\t}
\t\t}
\t}
}

void VIA1Device::checkT1IntReady()
{
\tif (T1Running) {
\t\tbool NewT1IntReady = false;

\t\tif ((d_.IFR & (1 << kIntT1)) == 0) {
\t\t\tif (((d_.ACR & 0x40) != 0) || (T1_Active == 1)) {
\t\t\t\tNewT1IntReady = true;
\t\t\t}
\t\t}

\t\tif (T1IntReady != NewT1IntReady) {
\t\t\tT1IntReady = NewT1IntReady;
\t\t\tif (NewT1IntReady) {
\t\t\t\tdoTimer1Check();
\t\t\t}
\t\t}
\t}
}

uint16_t VIA1Device::getT1InvertTime()
{
\tuint16_t v;

\tif ((d_.ACR & 0xC0) == 0xC0) {
\t\tv = (d_.T1L_H << 8) + d_.T1L_L;
\t} else {
\t\tv = 0;
\t}
\treturn v;
}

void VIA1Device::doTimer2Check()
{
\tif (T2Running || T2C_ShortTime) {
\t\tiCountt NewTime = GetCuriCount();
\t\tuint32_t Temp = d_.T2C_F;
\t\tiCountt deltaTime = (NewTime - T2LastTime);
\t\tuint32_t deltaTemp = (deltaTime / CyclesPerViaTime)
\t\t\t<< (16 - kLn2CycleScale);
\t\tuint32_t NewTemp = Temp - deltaTemp;
\t\tif (T2_Active == 1) {
\t\t\tif ((deltaTime > (0x00010000UL * CyclesScaledPerViaTime))
\t\t\t\t|| ((Temp <= deltaTemp) && (Temp != 0)))
\t\t\t{
\t\t\t\tT2C_ShortTime = false;
\t\t\t\tT2_Active = 0;
\t\t\t\tsetInterruptFlag(kIntT2);
#if VIA1_dolog && 1
\t\t\t\tdbglog_WriteNote("VIA1 Timer 2 Interrupt");
#endif
\t\t\t} else {
\t\t\t\tuint32_t NewTimer;
#ifdef _VIA_Debug
\t\t\t\tfprintf(stderr, "posting Timer2Check, %d, %d\\n",
\t\t\t\t\tTemp, GetCuriCount());
#endif
#if VIA1_dolog
\t\t\t\tdbglog_WriteNote("VIA1 Timer 2 Later");
#endif
\t\t\t\tif (NewTemp == 0) {
\t\t\t\t\tNewTimer = (0x00010000UL * CyclesScaledPerViaTime);
\t\t\t\t} else {
\t\t\t\t\tNewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale)))
\t\t\t\t\t\t* CyclesPerViaTime;
\t\t\t\t}
\t\t\t\tICT_add(kICT_VIA1_Timer2Check, NewTimer);
\t\t\t}
\t\t}
\t\td_.T2C_F = NewTemp;
\t\tT2LastTime = NewTime;
\t}
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
\tswitch (addr) {
\t\tcase kORB   :
#if VIA1_CB2modesAllowed != 0x01
\t\t\tif ((d_.PCR & 0xE0) == 0)
#endif
\t\t\t{
\t\t\t\tclrInterruptFlag(kIntCB2);
\t\t\t}
\t\t\tclrInterruptFlag(kIntCB1);
\t\t\tif (WriteMem) {
\t\t\t\td_.ORB = Data;
\t\t\t\tputORB(d_.DDR_B, d_.ORB);
\t\t\t} else {
\t\t\t\tData = (d_.ORB & d_.DDR_B)
\t\t\t\t\t| getORB(~ d_.DDR_B);
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kORB", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kDDR_B :
\t\t\tif (WriteMem) {
\t\t\t\tsetDDR_B(Data);
\t\t\t} else {
\t\t\t\tData = d_.DDR_B;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kDDR_B", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kDDR_A :
\t\t\tif (WriteMem) {
\t\t\t\tsetDDR_A(Data);
\t\t\t} else {
\t\t\t\tData = d_.DDR_A;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kDDR_A", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kT1C_L :
\t\t\tif (WriteMem) {
\t\t\t\td_.T1L_L = Data;
\t\t\t} else {
\t\t\t\tclrInterruptFlag(kIntT1);
\t\t\t\tdoTimer1Check();
\t\t\t\tData = (d_.T1C_F & 0x00FF0000) >> 16;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kT1C_L", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kT1C_H :
\t\t\tif (WriteMem) {
\t\t\t\td_.T1L_H = Data;
\t\t\t\tclrInterruptFlag(kIntT1);
\t\t\t\td_.T1C_F = (Data << 24) + (d_.T1L_L << 16);
\t\t\t\tif ((d_.ACR & 0x40) == 0) {
\t\t\t\t\tT1_Active = 1;
\t\t\t\t}
\t\t\t\tT1LastTime = GetCuriCount();
\t\t\t\tdoTimer1Check();
\t\t\t} else {
\t\t\t\tdoTimer1Check();
\t\t\t\tData = (d_.T1C_F & 0xFF000000) >> 24;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kT1C_H", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kT1L_L :
\t\t\tif (WriteMem) {
\t\t\t\td_.T1L_L = Data;
\t\t\t} else {
\t\t\t\tData = d_.T1L_L;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kT1L_L", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kT1L_H :
\t\t\tif (WriteMem) {
\t\t\t\td_.T1L_H = Data;
\t\t\t} else {
\t\t\t\tData = d_.T1L_H;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kT1L_H", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kT2_L  :
\t\t\tif (WriteMem) {
\t\t\t\td_.T2L_L = Data;
\t\t\t} else {
\t\t\t\tclrInterruptFlag(kIntT2);
\t\t\t\tdoTimer2Check();
\t\t\t\tData = (d_.T2C_F & 0x00FF0000) >> 16;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kT2_L", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kT2_H  :
\t\t\tif (WriteMem) {
\t\t\t\td_.T2C_F = (Data << 24) + (d_.T2L_L << 16);
\t\t\t\tclrInterruptFlag(kIntT2);
\t\t\t\tT2_Active = 1;

\t\t\t\tif ((d_.T2C_F < (128UL << 16))
\t\t\t\t\t&& (d_.T2C_F != 0))
\t\t\t\t{
#if VIA1_dolog
\t\t\t\t\tdbglog_StartLine();
\t\t\t\t\tdbglog_writeCStr("VIA1_T2C_ShortTime ");
\t\t\t\t\tdbglog_writeHex(d_.T2C_F);
\t\t\t\t\tdbglog_writeCStr(", IER ");
\t\t\t\t\tdbglog_writeHex(d_.IER);
\t\t\t\t\tdbglog_writeCStr(", T2Running ");
\t\t\t\t\tdbglog_writeHex(T2Running);
\t\t\t\t\tdbglog_writeCStr(", T2C_ShortTime ");
\t\t\t\t\tdbglog_writeHex(T2C_ShortTime);
\t\t\t\t\tdbglog_writeReturn();
#endif
\t\t\t\t\tT2C_ShortTime = true;
\t\t\t\t}
\t\t\t\tT2LastTime = GetCuriCount();
\t\t\t\tdoTimer2Check();
\t\t\t} else {
\t\t\t\tdoTimer2Check();
\t\t\t\tData = (d_.T2C_F & 0xFF000000) >> 24;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kT2_H", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kSR:
#ifdef _VIA_Debug
\t\t\tfprintf(stderr, "d_.SR: %d, %d, %d\\n",
\t\t\t\tWriteMem, ((d_.ACR & 0x1C) >> 2), Data);
#endif
\t\t\tif (WriteMem) {
\t\t\t\td_.SR = Data;
\t\t\t}
\t\t\tclrInterruptFlag(kIntSR);
\t\t\tswitch ((d_.ACR & 0x1C) >> 2) {
\t\t\t\tcase 3 : /* Shifting In */
\t\t\t\t\tbreak;
\t\t\t\tcase 6 : /* shift out under o2 clock */
\t\t\t\t\tif ((! WriteMem) || (d_.SR != 0)) {
\t\t\t\t\t\tReportAbnormalID(0x0405,
\t\t\t\t\t\t\t"VIA shift mode 6, non zero");
\t\t\t\t\t} else {
#ifdef _VIA_Debug
\t\t\t\t\t\tfprintf(stderr, "posting Foo2Task\\n");
#endif
\t\t\t\t\t\tif (VIA1_iCB2 != 0) {
\t\t\t\t\t\t\tVIA1_iCB2 = 0;
#ifdef VIA1_iCB2_ChangeNtfy
\t\t\t\t\t\t\tVIA1_iCB2_ChangeNtfy();
#endif
\t\t\t\t\t\t}
\t\t\t\t\t}
#if 0
\t\t\t\t\tsetInterruptFlag(kIntSR);
#endif
\t\t\t\t\tbreak;
\t\t\t\tcase 7 : /* Shifting Out */
\t\t\t\t\tbreak;
\t\t\t}
\t\t\tif (! WriteMem) {
\t\t\t\tData = d_.SR;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kSR", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kACR:
\t\t\tif (WriteMem) {
#if 1
\t\t\t\tif ((d_.ACR & 0x10) != ((uint8_t)Data & 0x10)) {
\t\t\t\t\tif ((Data & 0x10) == 0) {
\t\t\t\t\t\tif (VIA1_iCB2 == 0) {
\t\t\t\t\t\t\tVIA1_iCB2 = 1;
#ifdef VIA1_iCB2_ChangeNtfy
\t\t\t\t\t\t\tVIA1_iCB2_ChangeNtfy();
#endif
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t}
#endif
\t\t\t\td_.ACR = Data;
\t\t\t\tif ((d_.ACR & 0x20) != 0) {
\t\t\t\t\tReportAbnormalID(0x0406,
\t\t\t\t\t\t"Set d_.ACR T2 Timer pulse counting");
\t\t\t\t}
\t\t\t\tswitch ((d_.ACR & 0xC0) >> 6) {
\t\t\t\t\tcase 2:
\t\t\t\t\t\tReportAbnormalID(0x0407,
\t\t\t\t\t\t\t"Set d_.ACR T1 Timer mode 2");
\t\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t\tcheckT1IntReady();
\t\t\t\tswitch ((d_.ACR & 0x1C) >> 2) {
\t\t\t\t\tcase 0:
\t\t\t\t\t\tclrInterruptFlag(kIntSR);
\t\t\t\t\t\tbreak;
\t\t\t\t\tcase 1:
\t\t\t\t\tcase 2:
\t\t\t\t\tcase 4:
\t\t\t\t\tcase 5:
\t\t\t\t\t\tReportAbnormalID(0x0408,
\t\t\t\t\t\t\t"Set d_.ACR shift mode 1,2,4,5");
\t\t\t\t\t\tbreak;
\t\t\t\t\tdefault:
\t\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t\tif ((d_.ACR & 0x03) != 0) {
\t\t\t\t\tReportAbnormalID(0x0409,
\t\t\t\t\t\t"Set d_.ACR T2 Timer latching enabled");
\t\t\t\t}
\t\t\t} else {
\t\t\t\tData = d_.ACR;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kACR", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kPCR:
\t\t\tif (WriteMem) {
\t\t\t\td_.PCR = Data;
#define Ui3rSetContains(s, i) (((s) & (1 << (i))) != 0)
\t\t\t\tif (! Ui3rSetContains(VIA1_CB2modesAllowed,
\t\t\t\t\t(d_.PCR >> 5) & 0x07))
\t\t\t\t{
\t\t\t\t\tReportAbnormalID(0x040A,
\t\t\t\t\t\t"Set d_.PCR CB2 Control mode?");
\t\t\t\t}
\t\t\t\tif ((d_.PCR & 0x10) != 0) {
\t\t\t\t\tReportAbnormalID(0x040B,
\t\t\t\t\t\t"Set d_.PCR CB1 INTERRUPT CONTROL?");
\t\t\t\t}
\t\t\t\tif (! Ui3rSetContains(VIA1_CA2modesAllowed,
\t\t\t\t\t(d_.PCR >> 1) & 0x07))
\t\t\t\t{
\t\t\t\t\tReportAbnormalID(0x040C,
\t\t\t\t\t\t"Set d_.PCR CA2 INTERRUPT CONTROL?");
\t\t\t\t}
\t\t\t\tif ((d_.PCR & 0x01) != 0) {
\t\t\t\t\tReportAbnormalID(0x040D,
\t\t\t\t\t\t"Set d_.PCR CA1 INTERRUPT CONTROL?");
\t\t\t\t}
\t\t\t} else {
\t\t\t\tData = d_.PCR;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kPCR", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kIFR:
\t\t\tif (WriteMem) {
\t\t\t\td_.IFR = d_.IFR & ((~ Data) & 0x7F);
\t\t\t\tcheckInterruptFlag();
\t\t\t\tcheckT1IntReady();
\t\t\t} else {
\t\t\t\tData = d_.IFR;
\t\t\t\tif ((d_.IFR & d_.IER) != 0) {
\t\t\t\t\tData |= 0x80;
\t\t\t\t}
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kIFR", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kIER   :
\t\t\tif (WriteMem) {
\t\t\t\tif ((Data & 0x80) == 0) {
\t\t\t\t\td_.IER = d_.IER & ((~ Data) & 0x7F);
#if 0 != VIA1_IER_Never0
\t\t\t\t\tif ((Data & VIA1_IER_Never0) != 0) {
\t\t\t\t\t\tReportAbnormalID(0x040E, "IER Never0 clr");
\t\t\t\t\t}
#endif
\t\t\t\t} else {
\t\t\t\t\td_.IER = d_.IER | (Data & 0x7F);
#if 0 != VIA1_IER_Never1
\t\t\t\t\tif ((d_.IER & VIA1_IER_Never1) != 0) {
\t\t\t\t\t\tReportAbnormalID(0x040F, "IER Never1 set");
\t\t\t\t\t}
#endif
\t\t\t\t}
\t\t\t\tcheckInterruptFlag();
\t\t\t} else {
\t\t\t\tData = d_.IER | 0x80;
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kIER", Data, WriteMem);
#endif
\t\t\tbreak;
\t\tcase kORA   :
\t\tcase kORA_H :
\t\t\tif ((d_.PCR & 0xE) == 0) {
\t\t\t\tclrInterruptFlag(kIntCA2);
\t\t\t}
\t\t\tclrInterruptFlag(kIntCA1);
\t\t\tif (WriteMem) {
\t\t\t\td_.ORA = Data;
\t\t\t\tputORA(d_.DDR_A, d_.ORA);
\t\t\t} else {
\t\t\t\tData = (d_.ORA & d_.DDR_A)
\t\t\t\t\t| getORA(~ d_.DDR_A);
\t\t\t}
#if VIA1_dolog && 1
\t\t\tdbglog_Access("VIA1_Access kORA", Data, WriteMem);
#endif
\t\t\tbreak;
\t}
\treturn Data;
}

void VIA1Device::extraTimeBegin()
{
#if VIA1_dolog
\tdbglog_WriteNote("VIA1_ExtraTimeBegin");
#endif
\tif (T1Running) {
\t\tdoTimer1Check();
\t\tT1Running = false;
\t}
\tif (T2Running) {
\t\tdoTimer2Check();
\t\tT2Running = false;
\t}
}

void VIA1Device::extraTimeEnd()
{
#if VIA1_dolog
\tdbglog_WriteNote("VIA1_ExtraTimeEnd");
#endif
\tif (! T1Running) {
\t\tT1Running = true;
\t\tT1LastTime = GetCuriCount();
\t\tdoTimer1Check();
\t}
\tif (! T2Running) {
\t\tT2Running = true;
\t\tif (! T2C_ShortTime) {
\t\t\tT2LastTime = GetCuriCount();
\t\t}
\t\tdoTimer2Check();
\t}
}

void VIA1Device::iCA1_PulseNtfy()
{
\tsetInterruptFlag(kIntCA1);
}

void VIA1Device::iCA2_PulseNtfy()
{
\tsetInterruptFlag(kIntCA2);
}

void VIA1Device::iCB1_PulseNtfy()
{
\tsetInterruptFlag(kIntCB1);
}

void VIA1Device::iCB2_PulseNtfy()
{
\tsetInterruptFlag(kIntCB2);
}


/* ===== Backward-compatible forwarding functions ===== */

static VIA1Device& ensureVIA1()
{
\tif (!g_via1) {
\t\tstatic VIA1Device instance;
\t\tg_via1 = &instance;
\t}
\treturn *g_via1;
}

void VIA1_Zap(void)
{
\tensureVIA1().zap();
}

void VIA1_Reset(void)
{
\tensureVIA1().reset();
}

uint32_t VIA1_Access(uint32_t Data, bool WriteMem, uint32_t addr)
{
\treturn g_via1->access(Data, WriteMem, addr);
}

void VIA1_ExtraTimeBegin(void)
{
\tg_via1->extraTimeBegin();
}

void VIA1_ExtraTimeEnd(void)
{
\tg_via1->extraTimeEnd();
}

void VIA1_DoTimer1Check(void)
{
\tg_via1->doTimer1Check();
}

void VIA1_DoTimer2Check(void)
{
\tg_via1->doTimer2Check();
}

uint16_t VIA1_GetT1InvertTime(void)
{
\treturn g_via1->getT1InvertTime();
}

void VIA1_ShiftInData(uint8_t v)
{
\tg_via1->shiftInData(v);
}

uint8_t VIA1_ShiftOutData(void)
{
\treturn g_via1->shiftOutData();
}

/* Pulse notifications - called through #define aliases in CNFUDPIC.h */

void VIA1_iCA1_Sixtieth_PulseNtfy(void)
{
\tg_via1->iCA1_PulseNtfy();
}

void VIA1_iCA2_RTC_OneSecond_PulseNtfy(void)
{
\tg_via1->iCA2_PulseNtfy();
}

#endif /* EmVIA1 */
'''

with open(os.path.join(BASE, 'src/devices/via.cpp'), 'w') as f:
    f.write(via_cpp)
print("via.cpp written")
