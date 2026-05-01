/*
	via_base.cpp

	Unified VIA (Versatile Interface Adapter) emulation.
	Shared implementation for both VIA1 and VIA2.

	This code adapted from vMac by Philip Cummins.
*/

#include "core/common.h"
#include "core/ict_scheduler.h"

#include "devices/via_base.h"
#include "core/wire_bus.h"
#include "core/rig.h"

#define BIT_MASK(p) (1 << (p))
#define TEST_BIT(i, p) (((i) & BIT_MASK(p)) != 0)

static constexpr int kIntCA2 = 0;
static constexpr int kIntCA1 = 1;
static constexpr int kIntSR = 2;
static constexpr int kIntCB2 = 3;
static constexpr int kIntCB1 = 4;
static constexpr int kIntT2 = 5;
static constexpr int kIntT1 = 6;

VIABase::VIABase(int viaNum, uint16_t abnormalBase, int ictTimer1, int ictTimer2)
	: viaNum_(viaNum), abnormalBase_(abnormalBase), ictTimer1_(ictTimer1), ictTimer2_(ictTimer2)
{
}

/* ===== VIABase method implementations ===== */

uint8_t VIABase::getORA(uint8_t Selection)
{
	const auto &cfg = viaConfig();
	uint8_t Value = (~cfg.oraCanIn) & Selection & cfg.oraFloatVal;

	for (int bit = 0; bit < 8; bit++)
	{
		if (TEST_BIT(cfg.oraCanIn, bit) && TEST_BIT(Selection, bit))
		{
			int wireId = cfg.portAWires[bit];
			if (wireId >= 0)
			{
				Value |= (g_wires.get(wireId) << bit);
			}
		}
	}

	return Value;
}

uint8_t VIABase::getORB(uint8_t Selection)
{
	const auto &cfg = viaConfig();
	uint8_t Value = (~cfg.orbCanIn) & Selection & cfg.orbFloatVal;

	for (int bit = 0; bit < 8; bit++)
	{
		if (TEST_BIT(cfg.orbCanIn, bit) && TEST_BIT(Selection, bit))
		{
			int wireId = cfg.portBWires[bit];
			if (wireId >= 0)
			{
				Value |= (g_wires.get(wireId) << bit);
			}
		}
	}

	return Value;
}

void VIABase::putORA(uint8_t selection, uint8_t data)
{
	const auto &cfg = viaConfig();

	/* Iterate bits 7→0 to match the reference build's callback ordering */
	for (int bit = 7; bit >= 0; bit--)
	{
		if (TEST_BIT(cfg.oraCanOut, bit) && TEST_BIT(selection, bit))
		{
			int wireId = cfg.portAWires[bit];
			if (wireId >= 0)
			{
				g_wires.set(wireId, (data >> bit) & 1);
			}
		}
	}
}

void VIABase::putORB(uint8_t selection, uint8_t data)
{
	const auto &cfg = viaConfig();

	/* Iterate bits 7→0 to match the reference build's callback ordering */
	for (int bit = 7; bit >= 0; bit--)
	{
		if (TEST_BIT(cfg.orbCanOut, bit) && TEST_BIT(selection, bit))
		{
			int wireId = cfg.portBWires[bit];
			if (wireId >= 0)
			{
				g_wires.set(wireId, (data >> bit) & 1);
			}
		}
	}
}

void VIABase::setDDR_A(uint8_t data)
{
	const auto &cfg = viaConfig();
	uint8_t floatbits = d_.DDR_A & ~data;
	uint8_t unfloatbits = data & ~d_.DDR_A;

	if (floatbits != 0)
	{
		putORA(floatbits, cfg.oraFloatVal);
	}
	d_.DDR_A = data;
	if (unfloatbits != 0)
	{
		putORA(unfloatbits, d_.ORA);
	}
	if ((data & ~cfg.oraCanOut) != 0)
	{
		REPORT_ABNORMAL_ID(abnormalBase_ | 0x01, "Set d_.DDR_A unexpected direction");
	}
}

void VIABase::setDDR_B(uint8_t data)
{
	const auto &cfg = viaConfig();
	uint8_t floatbits = d_.DDR_B & ~data;
	uint8_t unfloatbits = data & ~d_.DDR_B;

	if (floatbits != 0)
	{
		putORB(floatbits, cfg.orbFloatVal);
	}
	d_.DDR_B = data;
	if (unfloatbits != 0)
	{
		putORB(unfloatbits, d_.ORB);
	}
	if ((data & ~cfg.orbCanOut) != 0)
	{
		REPORT_ABNORMAL_ID(abnormalBase_ | 0x02, "Set d_.DDR_B unexpected direction");
	}
}

void VIABase::checkInterruptFlag()
{
	const auto &cfg = viaConfig();
	uint8_t NewInterruptRequest = ((d_.IFR & d_.IER) != 0) ? 1 : 0;

	if (cfg.interruptWire >= 0)
	{
		g_wires.set(cfg.interruptWire, NewInterruptRequest);
	}
}

void VIABase::setInterruptFlag(uint8_t viaInt)
{
	d_.IFR |= ((uint8_t)1 << viaInt);
	checkInterruptFlag();
}

void VIABase::clrInterruptFlag(uint8_t viaInt)
{
	d_.IFR &= ~((uint8_t)1 << viaInt);
	checkInterruptFlag();
}

void VIABase::clear()
{
	d_.ORA = 0;
	d_.DDR_A = 0;
	d_.ORB = 0;
	d_.DDR_B = 0;
	d_.T1L_L = d_.T1L_H = 0x00;
	d_.T2L_L = 0x00;
	d_.T1C_F = 0;
	d_.T2C_F = 0;
	d_.SR = d_.ACR = 0x00;
	d_.PCR = d_.IFR = d_.IER = 0x00;
	T1_Active = T2_Active = 0x00;
	T1IntReady = false;
}

void VIABase::zap()
{
	const auto &cfg = viaConfig();
	clear();
	if (cfg.interruptWire >= 0)
	{
		g_wires.set(cfg.interruptWire, 0);
	}
}

void VIABase::reset()
{
	setDDR_A(0);
	setDDR_B(0);
	clear();
	checkInterruptFlag();
}

void VIABase::shiftInData(uint8_t v)
{
	uint8_t ShiftMode = (d_.ACR & 0x1C) >> 2;

	if (ShiftMode != 3)
	{
	}
	else
	{
		d_.SR = v;
		setInterruptFlag(kIntSR);
		setInterruptFlag(kIntCB1);
	}
}

uint8_t VIABase::shiftOutData()
{
	const auto &cfg = viaConfig();
	if (((d_.ACR & 0x1C) >> 2) != 7)
	{
		REPORT_ABNORMAL_ID(abnormalBase_ | 0x04, "VIA Not ready to shift out");
		return 0;
	}
	else
	{
		setInterruptFlag(kIntSR);
		setInterruptFlag(kIntCB1);
		if (cfg.cb2Wire >= 0)
		{
			g_wires.set(cfg.cb2Wire, d_.SR & 1);
		}
		return d_.SR;
	}
}

#define CYCLES_PER_VIA_TIME (10 * machine_->config().clockMult)
#define CYCLES_SCALED_PER_VIA_TIME (kCycleScale * CYCLES_PER_VIA_TIME)

/*
	Update Timer 1: subtract elapsed cycles, fire interrupt
	if countdown expires.  In free-running mode, reload from
	latch and optionally toggle port B bit 7.
*/
void VIABase::doTimer1Check()
{
	if (T1Running)
	{
		InstructionCount NewTime = g_ict.getCurrent();
		InstructionCount deltaTime = (NewTime - T1LastTime);
		if (deltaTime != 0)
		{
			uint32_t Temp = d_.T1C_F;
			uint32_t deltaTemp = (deltaTime / CYCLES_PER_VIA_TIME) << (16 - kLn2CycleScale);
			uint32_t NewTemp = Temp - deltaTemp;
			if ((deltaTime > (0x00010000UL * CYCLES_SCALED_PER_VIA_TIME)) ||
				((Temp <= deltaTemp) && (Temp != 0)))
			{
				if ((d_.ACR & 0x40) != 0)
				{ /* Free Running? */
					const auto &t1cfg = viaConfig();
					uint16_t v = (d_.T1L_H << 8) + d_.T1L_L;
					uint16_t ntrans = 1 + ((v == 0) ? 0 : (((deltaTemp - Temp) / v) >> 16));
					NewTemp += (((uint32_t)v * ntrans) << 16);
					if (TEST_BIT(t1cfg.orbCanOut, 7))
					{
						if ((d_.ACR & 0x80) != 0)
						{ /* invert ? */
							if ((ntrans & 1) != 0)
							{
								int b7Wire = t1cfg.portBWires[7];
								if (b7Wire >= 0)
								{
									g_wires.set(b7Wire, g_wires.get(b7Wire) ^ 1);
								}
							}
						}
					}
					setInterruptFlag(kIntT1);
				}
				else
				{
					if (T1_Active == 1)
					{
						T1_Active = 0;
						setInterruptFlag(kIntT1);
					}
				}
			}

			d_.T1C_F = NewTemp;
			T1LastTime = NewTime;
		}

		T1IntReady = false;
		if ((d_.IFR & (1 << kIntT1)) == 0)
		{
			if (((d_.ACR & 0x40) != 0) || (T1_Active == 1))
			{
				uint32_t NewTemp = d_.T1C_F;
				uint32_t NewTimer;
				if (NewTemp == 0)
				{
					NewTimer = (0x00010000UL * CYCLES_SCALED_PER_VIA_TIME);
				}
				else
				{
					NewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale))) * CYCLES_PER_VIA_TIME;
				}
				g_ict.add(ictTimer1_, NewTimer);
				T1IntReady = true;
			}
		}
	}
}

void VIABase::checkT1IntReady()
{
	if (T1Running)
	{
		bool NewT1IntReady = false;

		if ((d_.IFR & (1 << kIntT1)) == 0)
		{
			if (((d_.ACR & 0x40) != 0) || (T1_Active == 1))
			{
				NewT1IntReady = true;
			}
		}

		if (T1IntReady != NewT1IntReady)
		{
			T1IntReady = NewT1IntReady;
			if (NewT1IntReady)
			{
				doTimer1Check();
			}
		}
	}
}

uint16_t VIABase::getT1InvertTime()
{
	uint16_t v;

	if ((d_.ACR & 0xC0) == 0xC0)
	{
		v = (d_.T1L_H << 8) + d_.T1L_L;
	}
	else
	{
		v = 0;
	}
	return v;
}

void VIABase::doTimer2Check()
{
	if (T2Running || T2C_ShortTime)
	{
		InstructionCount NewTime = g_ict.getCurrent();
		uint32_t Temp = d_.T2C_F;
		InstructionCount deltaTime = (NewTime - T2LastTime);
		uint32_t deltaTemp = (deltaTime / CYCLES_PER_VIA_TIME) << (16 - kLn2CycleScale);
		uint32_t NewTemp = Temp - deltaTemp;
		if (T2_Active == 1)
		{
			if ((deltaTime > (0x00010000UL * CYCLES_SCALED_PER_VIA_TIME)) ||
				((Temp <= deltaTemp) && (Temp != 0)))
			{
				T2C_ShortTime = false;
				T2_Active = 0;
				setInterruptFlag(kIntT2);
			}
			else
			{
				uint32_t NewTimer;
				if (NewTemp == 0)
				{
					NewTimer = (0x00010000UL * CYCLES_SCALED_PER_VIA_TIME);
				}
				else
				{
					NewTimer = (1 + (NewTemp >> (16 - kLn2CycleScale))) * CYCLES_PER_VIA_TIME;
				}
				g_ict.add(ictTimer2_, NewTimer);
			}
		}
		d_.T2C_F = NewTemp;
		T2LastTime = NewTime;
	}
}

static constexpr int kORB = 0x00;
static constexpr int kORA_H = 0x01;
static constexpr int kDDR_B = 0x02;
static constexpr int kDDR_A = 0x03;
static constexpr int kT1C_L = 0x04;
static constexpr int kT1C_H = 0x05;
static constexpr int kT1L_L = 0x06;
static constexpr int kT1L_H = 0x07;
static constexpr int kT2_L = 0x08;
static constexpr int kT2_H = 0x09;
static constexpr int kSR = 0x0A;
static constexpr int kACR = 0x0B;
static constexpr int kPCR = 0x0C;
static constexpr int kIFR = 0x0D;
static constexpr int kIER = 0x0E;
static constexpr int kORA = 0x0F;

/*
	VIA register read/write dispatcher.
	Handles all 16 registers: data ports, DDR, timers,
	shift register, ACR, PCR, IFR, and IER.
*/
uint32_t VIABase::access(uint32_t Data, bool WriteMem, uint32_t addr)
{
	const auto &acfg = viaConfig();
	switch (addr)
	{
		case kORB:
			if (acfg.cb2ModesAllowed != 0x01)
			{
				if ((d_.PCR & 0xE0) == 0)
				{
					clrInterruptFlag(kIntCB2);
				}
			}
			else
			{
				clrInterruptFlag(kIntCB2);
			}
			clrInterruptFlag(kIntCB1);
			if (WriteMem)
			{
				d_.ORB = Data;
				putORB(d_.DDR_B, d_.ORB);
			}
			else
			{
				Data = (d_.ORB & d_.DDR_B) | getORB(~d_.DDR_B);
			}
			break;
		case kDDR_B:
			if (WriteMem)
			{
				setDDR_B(Data);
			}
			else
			{
				Data = d_.DDR_B;
			}
			break;
		case kDDR_A:
			if (WriteMem)
			{
				setDDR_A(Data);
			}
			else
			{
				Data = d_.DDR_A;
			}
			break;
		case kT1C_L:
			if (WriteMem)
			{
				d_.T1L_L = Data;
			}
			else
			{
				clrInterruptFlag(kIntT1);
				doTimer1Check();
				Data = (d_.T1C_F & 0x00FF0000) >> 16;
			}
			break;
		case kT1C_H:
			if (WriteMem)
			{
				d_.T1L_H = Data;
				clrInterruptFlag(kIntT1);
				d_.T1C_F = (Data << 24) + (d_.T1L_L << 16);
				if ((d_.ACR & 0x40) == 0)
				{
					T1_Active = 1;
				}
				T1LastTime = g_ict.getCurrent();
				doTimer1Check();
			}
			else
			{
				doTimer1Check();
				Data = (d_.T1C_F & 0xFF000000) >> 24;
			}
			break;
		case kT1L_L:
			if (WriteMem)
			{
				d_.T1L_L = Data;
			}
			else
			{
				Data = d_.T1L_L;
			}
			break;
		case kT1L_H:
			if (WriteMem)
			{
				d_.T1L_H = Data;
			}
			else
			{
				Data = d_.T1L_H;
			}
			break;
		case kT2_L:
			if (WriteMem)
			{
				d_.T2L_L = Data;
			}
			else
			{
				clrInterruptFlag(kIntT2);
				doTimer2Check();
				Data = (d_.T2C_F & 0x00FF0000) >> 16;
			}
			break;
		case kT2_H:
			if (WriteMem)
			{
				d_.T2C_F = (Data << 24) + (d_.T2L_L << 16);
				clrInterruptFlag(kIntT2);
				T2_Active = 1;

				if ((d_.T2C_F < (128UL << 16)) && (d_.T2C_F != 0))
				{
					T2C_ShortTime = true;
				}
				T2LastTime = g_ict.getCurrent();
				doTimer2Check();
			}
			else
			{
				doTimer2Check();
				Data = (d_.T2C_F & 0xFF000000) >> 24;
			}
			break;
		case kSR:
			if (WriteMem)
			{
				d_.SR = Data;
			}
			clrInterruptFlag(kIntSR);
			switch ((d_.ACR & 0x1C) >> 2)
			{
				case 3: /* Shifting In */
					break;
				case 6: /* shift out under o2 clock */
					if ((!WriteMem) || (d_.SR != 0))
					{
						REPORT_ABNORMAL_ID(abnormalBase_ | 0x05, "VIA shift mode 6, non zero");
					}
					else
					{
						if (acfg.cb2Wire >= 0)
						{
							g_wires.set(acfg.cb2Wire, 0);
						}
					}
					break;
				case 7: /* Shifting Out */
					break;
			}
			if (!WriteMem)
			{
				Data = d_.SR;
			}
			break;
		case kACR:
			if (WriteMem)
			{
				if ((d_.ACR & 0x10) != ((uint8_t)Data & 0x10))
				{
					if ((Data & 0x10) == 0)
					{
						if (acfg.cb2Wire >= 0)
						{
							g_wires.set(acfg.cb2Wire, 1);
						}
					}
				}
				d_.ACR = Data;
				if ((d_.ACR & 0x20) != 0)
				{
					REPORT_ABNORMAL_ID(abnormalBase_ | 0x06, "Set d_.ACR T2 Timer pulse counting");
				}
				switch ((d_.ACR & 0xC0) >> 6)
				{
					case 2:
						REPORT_ABNORMAL_ID(abnormalBase_ | 0x07, "Set d_.ACR T1 Timer mode 2");
						break;
				}
				checkT1IntReady();
				switch ((d_.ACR & 0x1C) >> 2)
				{
					case 0:
						clrInterruptFlag(kIntSR);
						break;
					case 1:
					case 2:
					case 4:
					case 5:
						REPORT_ABNORMAL_ID(abnormalBase_ | 0x08, "Set d_.ACR shift mode 1,2,4,5");
						break;
					default:
						break;
				}
				if ((d_.ACR & 0x03) != 0)
				{
					REPORT_ABNORMAL_ID(abnormalBase_ | 0x09,
									   "Set d_.ACR T2 Timer latching enabled");
				}
			}
			else
			{
				Data = d_.ACR;
			}
			break;
		case kPCR:
			if (WriteMem)
			{
				d_.PCR = Data;
#define SET_CONTAINS(s, i) (((s) & (1 << (i))) != 0)
				if (!SET_CONTAINS(acfg.cb2ModesAllowed, (d_.PCR >> 5) & 0x07))
				{
					REPORT_ABNORMAL_ID(abnormalBase_ | 0x0A, "Set d_.PCR CB2 Control mode?");
				}
				if ((d_.PCR & 0x10) != 0)
				{
					REPORT_ABNORMAL_ID(abnormalBase_ | 0x0B, "Set d_.PCR CB1 INTERRUPT CONTROL?");
				}
				if (!SET_CONTAINS(acfg.ca2ModesAllowed, (d_.PCR >> 1) & 0x07))
				{
					REPORT_ABNORMAL_ID(abnormalBase_ | 0x0C, "Set d_.PCR CA2 INTERRUPT CONTROL?");
				}
				if ((d_.PCR & 0x01) != 0)
				{
					REPORT_ABNORMAL_ID(abnormalBase_ | 0x0D, "Set d_.PCR CA1 INTERRUPT CONTROL?");
				}
			}
			else
			{
				Data = d_.PCR;
			}
			break;
		case kIFR:
			if (WriteMem)
			{
				d_.IFR = d_.IFR & ((~Data) & 0x7F);
				checkInterruptFlag();
				checkT1IntReady();
			}
			else
			{
				Data = d_.IFR;
				if ((d_.IFR & d_.IER) != 0)
				{
					Data |= 0x80;
				}
			}
			break;
		case kIER:
			if (WriteMem)
			{
				if ((Data & 0x80) == 0)
				{
					d_.IER = d_.IER & ((~Data) & 0x7F);
					if (acfg.ierNever0 != 0)
					{
						if ((Data & acfg.ierNever0) != 0)
						{
							REPORT_ABNORMAL_ID(abnormalBase_ | 0x0E, "IER Never0 clr");
						}
					}
				}
				else
				{
					d_.IER = d_.IER | (Data & 0x7F);
					if (acfg.ierNever1 != 0)
					{
						if ((d_.IER & acfg.ierNever1) != 0)
						{
							REPORT_ABNORMAL_ID(abnormalBase_ | 0x0F, "IER Never1 set");
						}
					}
				}
				checkInterruptFlag();
			}
			else
			{
				Data = d_.IER | 0x80;
			}
			break;
		case kORA:
		case kORA_H:
			if (acfg.ca2ModesAllowed != 0x01)
			{
				if ((d_.PCR & 0x0E) == 0)
				{
					clrInterruptFlag(kIntCA2);
				}
			}
			else
			{
				clrInterruptFlag(kIntCA2);
			}
			clrInterruptFlag(kIntCA1);
			if (WriteMem)
			{
				d_.ORA = Data;
				putORA(d_.DDR_A, d_.ORA);
			}
			else
			{
				Data = (d_.ORA & d_.DDR_A) | getORA(~d_.DDR_A);
			}
			break;
	}
	return Data;
}

void VIABase::extraTimeBegin()
{
	if (T1Running)
	{
		doTimer1Check();
		T1Running = false;
	}
	if (T2Running)
	{
		doTimer2Check();
		T2Running = false;
	}
}

void VIABase::extraTimeEnd()
{
	if (!T1Running)
	{
		T1Running = true;
		T1LastTime = g_ict.getCurrent();
		doTimer1Check();
	}
	if (!T2Running)
	{
		T2Running = true;
		if (!T2C_ShortTime)
		{
			T2LastTime = g_ict.getCurrent();
		}
		doTimer2Check();
	}
}

void VIABase::iCA1_PulseNtfy()
{
	setInterruptFlag(kIntCA1);
}

void VIABase::iCA2_PulseNtfy()
{
	setInterruptFlag(kIntCA2);
}

void VIABase::iCB1_PulseNtfy()
{
	setInterruptFlag(kIntCB1);
}

void VIABase::iCB2_PulseNtfy()
{
	setInterruptFlag(kIntCB2);
}
