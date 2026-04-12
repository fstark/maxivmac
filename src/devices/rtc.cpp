/*
	Real Time Clock EMulated DEVice

	Emulates the RTC found in the Mac Plus.

	This code adapted from "s_rtc.c" in vMac by Philip Cummins.
*/

#include "core/common.h"

#include "devices/via.h"

/* define _RTC_Debug */
#ifdef _RTC_Debug
#include <stdio.h>
#endif

#include "devices/rtc.h"
#include "core/wire_bus.h"
#include "core/machine_obj.h"
#include "core/abnormal_ids.h"

/* Global singleton */

/* All supported models (Plus through IIx) have XPRAM */
#define PARAMRAMSize 256

#define Group1Base 0x10
#define Group2Base 0x08

struct RTCState
{
	/* RTC VIA Flags */
	uint8_t WrProtect;
	uint8_t DataOut;
	uint8_t DataNextOut;

	/* RTC Data */
	uint8_t ShiftData;
	uint8_t Counter;
	uint8_t Mode;
	uint8_t SavedCmd;
	uint8_t Sector;

	/* RTC Registers */
	uint8_t Seconds_1[4];
	uint8_t PARAMRAM[PARAMRAMSize];
};

static RTCState s_rtc;

/* RTC Functions */

static uint32_t s_lastRealDate;


#ifndef TrackSpeed /* in 0..4 */
#define TrackSpeed 0
#endif

#ifndef AlarmOn /* in 0..1 */
#define AlarmOn 0
#endif

#ifndef DiskCacheSz /* in 1,2,3,4,6,8,12 */
/* actual cache size is DiskCacheSz * 32k */
/* 4 for compact Macs, 1 for Mac II (matches reference) */
#define DiskCacheSz (g_machine->config().isIIFamily() ? 1 : 4)
#endif

#ifndef StartUpDisk /* in 0..1 */
#define StartUpDisk 0
#endif

#ifndef DiskCacheOn /* in 0..1 */
#define DiskCacheOn 0
#endif

#ifndef MouseScalingOn /* in 0..1 */
#define MouseScalingOn 0
#endif

/* PRAM defaults — only used in this file */
#define SpeakerVol 0x07
#define MenuBlink 0x03
#define AutoKeyThresh 0x06
#define AutoKeyRate 0x03
#define pr_HilColRed 0x0000
#define pr_HilColGreen 0x0000
#define pr_HilColBlue 0x0000

/* CaretBlinkTime: 8 for Mac II family, 3 for compact Macs (matches reference) */
#ifndef CaretBlinkTime
#define CaretBlinkTime (g_machine->config().isIIFamily() ? 0x08 : 0x03)
#endif

/* DoubleClickTime: 8 for Mac II family, 5 for compact Macs (matches reference) */
#ifndef DoubleClickTime
#define DoubleClickTime (g_machine->config().isIIFamily() ? 0x08 : 0x05)
#endif

#define prb_fontHi 0
#define prb_fontLo 2
#define prb_kbdPrintHi (AutoKeyRate + (AutoKeyThresh << 4))
#define prb_kbdPrintLo 0
#define prb_volClickHi (SpeakerVol + (TrackSpeed << 3) + (AlarmOn << 7))
#define prb_volClickLo (CaretBlinkTime + (DoubleClickTime << 4))
#define prb_miscHi DiskCacheSz
#define prb_miscLo                                                                                 \
	((MenuBlink << 2) + (StartUpDisk << 4) + (DiskCacheOn << 5) + (MouseScalingOn << 6))

extern void DumpRTC();

void DumpRTC()
{
	int Counter;

	dbglog_writeln("RTC Parameter RAM");
	for (Counter = 0; Counter < PARAMRAMSize; Counter++)
	{
		dbglog_writeNum(Counter);
		dbglog_writeCStr(", ");
		dbglog_writeHex(s_rtc.PARAMRAM[Counter]);
		dbglog_writeReturn();
	}
	dbglog_writeCStr("RTC Seconds: ");
	dbglog_writeHex((s_rtc.Seconds_1[3] << 24) | (s_rtc.Seconds_1[2] << 16) |
					(s_rtc.Seconds_1[1] << 8) | s_rtc.Seconds_1[0]);
	dbglog_writeReturn();
}

/*
   Initialize PRAM with model-specific defaults and
   set the clock from the host system date.
*/
bool RTCDevice::init()
{
	int Counter;
	uint32_t secs;

	s_rtc.Mode = s_rtc.ShiftData = s_rtc.Counter = 0;
	s_rtc.DataOut = s_rtc.DataNextOut = 0;
	s_rtc.WrProtect = false;

	secs = g_curMacDateInSeconds;
	s_lastRealDate = secs;

	s_rtc.Seconds_1[0] = secs & 0xFF;
	s_rtc.Seconds_1[1] = (secs & 0xFF00) >> 8;
	s_rtc.Seconds_1[2] = (secs & 0xFF0000) >> 16;
	s_rtc.Seconds_1[3] = (secs & 0xFF000000) >> 24;

	for (Counter = 0; Counter < PARAMRAMSize; Counter++)
	{
		s_rtc.PARAMRAM[Counter] = 0;
	}

	s_rtc.PARAMRAM[0 + Group1Base] = 168; /* valid */

#if EmLocalTalk
	s_rtc.PARAMRAM[2 + Group1Base] = g_ltNodeHint;
	/* set to constant instead for testing collisions */
#else
	if (g_machine->config().isIIFamily())
	{
		s_rtc.PARAMRAM[2 + Group1Base] = 1;
		/* node id hint for printer port (AppleTalk) */
	}
#endif

#if EmLocalTalk
	s_rtc.PARAMRAM[3 + Group1Base] = 0x21;
#else
	s_rtc.PARAMRAM[3 + Group1Base] = 0x22;
#endif
	/*
		serial ports config bits: 4-7 A, 0-3 B
			useFree   0 Use undefined
			useATalk  1 AppleTalk
			useAsync  2 Async
			useExtClk 3 externally clocked
	*/

	s_rtc.PARAMRAM[4 + Group1Base] = 204; /* portA, high */
	s_rtc.PARAMRAM[5 + Group1Base] = 10;  /* portA, low */
	s_rtc.PARAMRAM[6 + Group1Base] = 204; /* portB, high */
	s_rtc.PARAMRAM[7 + Group1Base] = 10;  /* portB, low */
	s_rtc.PARAMRAM[13 + Group1Base] = prb_fontLo;
	s_rtc.PARAMRAM[14 + Group1Base] = prb_kbdPrintHi;
	if (g_machine->config().isIIFamily() || EmLocalTalk)
	{
		s_rtc.PARAMRAM[15 + Group1Base] = 1;
		/*
			printer, if any, connected to modem port
			because printer port used for appletalk.
		*/
	}

#if prb_volClickHi != 0
	s_rtc.PARAMRAM[0 + Group2Base] = prb_volClickHi;
#endif
	s_rtc.PARAMRAM[1 + Group2Base] = prb_volClickLo;
	s_rtc.PARAMRAM[2 + Group2Base] = prb_miscHi;
	s_rtc.PARAMRAM[3 + Group2Base] = prb_miscLo | ((0 != vMacScreenDepth) ? 0x80 : 0x00);

	/* XPRAM: extended parameter ram signature */
	if (g_machine->config().isIIFamily())
	{
		s_rtc.PARAMRAM[12] = 0x4e;
		s_rtc.PARAMRAM[13] = 0x75;
		s_rtc.PARAMRAM[14] = 0x4d;
		s_rtc.PARAMRAM[15] = 0x63;
	}
	else
	{
		s_rtc.PARAMRAM[12] = 0x42;
		s_rtc.PARAMRAM[13] = 0x75;
		s_rtc.PARAMRAM[14] = 0x67;
		s_rtc.PARAMRAM[15] = 0x73;
	}

	if (g_machine->config().isSEFamily() || g_machine->config().isIIFamily())
	{
		s_rtc.PARAMRAM[0x01] = 0x80;
		s_rtc.PARAMRAM[0x02] = 0x4F;
	}
	if (g_machine->config().isIIFamily())
	{
		s_rtc.PARAMRAM[0x03] = 0x48;

		/* video board id */
		s_rtc.PARAMRAM[0x46] = /* 0x42 */ 0x76; /* 'v' */
		s_rtc.PARAMRAM[0x47] = /* 0x32 */ 0x4D; /* 'M' */
		/* boot mode = 0x80 + boot depth (cap at 8 bpp for direct modes) */
		uint8_t bootDepth = (vMacScreenDepth >= 4) ? 3 : (uint8_t)vMacScreenDepth;
		s_rtc.PARAMRAM[0x48] = 0x80 + bootDepth;
	}

	if (g_machine->config().isIIFamily())
	{
		s_rtc.PARAMRAM[0x77] = 0x01;
	}

	if (g_machine->config().isSEFamily() || g_machine->config().isIIFamily())
	{
		/* start up disk (encoded how?) */
		s_rtc.PARAMRAM[0x78] = 0x00;
		s_rtc.PARAMRAM[0x79] = 0x01;
		s_rtc.PARAMRAM[0x7A] = 0xFF;
		s_rtc.PARAMRAM[0x7B] = 0xFE;
	}

	if (g_machine->config().isIIFamily())
	{
		s_rtc.PARAMRAM[0x80] = 0x09;
		s_rtc.PARAMRAM[0x81] = 0x80;
	}

	if (g_machine->config().isIIFamily())
	{

#define pr_HilColRedHi (pr_HilColRed >> 8)
#if 0 != pr_HilColRedHi
		s_rtc.PARAMRAM[0x82] = pr_HilColRedHi;
#endif
#define pr_HilColRedLo (pr_HilColRed & 0xFF)
#if 0 != pr_HilColRedLo
		s_rtc.PARAMRAM[0x83] = pr_HilColRedLo;
#endif

#define pr_HilColGreenHi (pr_HilColGreen >> 8)
#if 0 != pr_HilColGreenHi
		s_rtc.PARAMRAM[0x84] = pr_HilColGreenHi;
#endif
#define pr_HilColGreenLo (pr_HilColGreen & 0xFF)
#if 0 != pr_HilColGreenLo
		s_rtc.PARAMRAM[0x85] = pr_HilColGreenLo;
#endif

#define pr_HilColBlueHi (pr_HilColBlue >> 8)
#if 0 != pr_HilColBlueHi
		s_rtc.PARAMRAM[0x86] = pr_HilColBlueHi;
#endif
#define pr_HilColBlueLo (pr_HilColBlue & 0xFF)
#if 0 != pr_HilColBlueLo
		s_rtc.PARAMRAM[0x87] = pr_HilColBlueLo;
#endif
	}

	/* XPRAM: location data */
	do_put_mem_long(&s_rtc.PARAMRAM[0xE4], g_curMacLatitude);
	do_put_mem_long(&s_rtc.PARAMRAM[0xE8], g_curMacLongitude);
	do_put_mem_long(&s_rtc.PARAMRAM[0xEC], g_curMacDelta);

	DumpRTC();
	/* Dump PRAM to stderr for comparison */
	{
		int i;
		fprintf(stderr, "PRAM_DUMP ");
		for (i = 0; i < PARAMRAMSize; i++)
		{
			fprintf(stderr, "%02X", s_rtc.PARAMRAM[i]);
		}
		fprintf(stderr, " SEC=%02X%02X%02X%02X\n", s_rtc.Seconds_1[3], s_rtc.Seconds_1[2],
				s_rtc.Seconds_1[1], s_rtc.Seconds_1[0]);
	}

	return true;
}

// Advance the RTC seconds counter by the host-time delta.
void RTCDevice::interrupt()
{
	uint32_t Seconds = 0;
	uint32_t NewRealDate = g_curMacDateInSeconds;
	uint32_t DateDelta = NewRealDate - s_lastRealDate;

	if (DateDelta != 0)
	{
		Seconds = (s_rtc.Seconds_1[3] << 24) + (s_rtc.Seconds_1[2] << 16) +
				  (s_rtc.Seconds_1[1] << 8) + s_rtc.Seconds_1[0];
		Seconds += DateDelta;
		s_rtc.Seconds_1[0] = Seconds & 0xFF;
		s_rtc.Seconds_1[1] = (Seconds & 0xFF00) >> 8;
		s_rtc.Seconds_1[2] = (Seconds & 0xFF0000) >> 16;
		s_rtc.Seconds_1[3] = (Seconds & 0xFF000000) >> 24;

		s_lastRealDate = NewRealDate;

		if (auto *via1 = machine_->findDevice<VIA1Device>()) via1->iCA2_PulseNtfy();
	}
}

static uint8_t RTC_Access_PRAM_Reg(uint8_t Data, bool WriteReg, uint8_t t)
{
	if (WriteReg)
	{
		if (!s_rtc.WrProtect)
		{
			s_rtc.PARAMRAM[t] = Data;
#ifdef _RTC_Debug
			printf("Writing Address %2x, Data %2x\n", t, Data);
#endif
		}
	}
	else
	{
		Data = s_rtc.PARAMRAM[t];
	}
	return Data;
}

static uint8_t RTC_Access_Reg(uint8_t Data, bool WriteReg, uint8_t TheCmd)
{
	uint8_t t = (TheCmd & 0x7C) >> 2;
	if (t < 8)
	{
		if (WriteReg)
		{
			if (!s_rtc.WrProtect)
			{
				s_rtc.Seconds_1[t & 0x03] = Data;
			}
		}
		else
		{
			Data = s_rtc.Seconds_1[t & 0x03];
		}
	}
	else if (t < 12)
	{
		Data = RTC_Access_PRAM_Reg(Data, WriteReg, (t & 0x03) + Group2Base);
	}
	else if (t < 16)
	{
		if (WriteReg)
		{
			switch (t)
			{
				case 12:
					break; /* Test Write, do nothing */
				case 13:
					s_rtc.WrProtect = (Data & 0x80) != 0;
					break; /* Write_Protect Register */
				default:
					REPORT_ABNORMAL_ID(AbnormalID::kRTC_Write_RTC_Reg_unknown,
									   "Write RTC Reg unknown");
					break;
			}
		}
		else
		{
			REPORT_ABNORMAL_ID(AbnormalID::kRTC_Read_RTC_Reg_unknown, "Read RTC Reg unknown");
		}
	}
	else
	{
		Data = RTC_Access_PRAM_Reg(Data, WriteReg, (t & 0x0F) + Group1Base);
	}
	return Data;
}

/*
	Process a complete RTC serial command.  Decodes standard
	and extended commands, then reads/writes the target
	register or PRAM byte.
*/
static void RTC_DoCmd()
{
	switch (s_rtc.Mode)
	{
		case 0: /* This Byte is a RTC Command */
			if ((s_rtc.ShiftData & 0x78) == 0x38)
			{ /* Extended Command */
				s_rtc.SavedCmd = s_rtc.ShiftData;
				s_rtc.Mode = 2;
#ifdef _RTC_Debug
				printf("Extended command %2x\n", s_rtc.ShiftData);
#endif
			}
			else
			{
				if ((s_rtc.ShiftData & 0x80) != 0x00)
				{ /* Read Command */
					s_rtc.ShiftData = RTC_Access_Reg(0, false, s_rtc.ShiftData);
					s_rtc.DataNextOut = 1;
				}
				else
				{ /* Write Command */
					s_rtc.SavedCmd = s_rtc.ShiftData;
					s_rtc.Mode = 1;
				}
			}
			break;
		case 1: /* This Byte is data for RTC Write */
			(void)RTC_Access_Reg(s_rtc.ShiftData, true, s_rtc.SavedCmd);
			s_rtc.Mode = 0;
			break;
		case 2: /* This Byte is rest of Extended RTC command address */
#ifdef _RTC_Debug
			printf("Mode 2 %2x\n", s_rtc.ShiftData);
#endif
			s_rtc.Sector = ((s_rtc.SavedCmd & 0x07) << 5) | ((s_rtc.ShiftData & 0x7C) >> 2);
			if ((s_rtc.SavedCmd & 0x80) != 0x00)
			{ /* Read Command */
				s_rtc.ShiftData = s_rtc.PARAMRAM[s_rtc.Sector];
				s_rtc.DataNextOut = 1;
				s_rtc.Mode = 0;
#ifdef _RTC_Debug
				printf("Reading X Address %2x, Data  %2x\n", s_rtc.Sector, s_rtc.ShiftData);
#endif
			}
			else
			{
				s_rtc.Mode = 3;
#ifdef _RTC_Debug
				printf("Writing X Address %2x\n", s_rtc.Sector);
#endif
			}
			break;
		case 3: /* This Byte is data for an Extended RTC Write */
			(void)RTC_Access_PRAM_Reg(s_rtc.ShiftData, true, s_rtc.Sector);
			s_rtc.Mode = 0;
			break;
	}
}

void RTCDevice::unEnabledChangeNtfy()
{
	if (RTCunEnabled)
	{
		/* abort anything going on */
		if (s_rtc.Counter != 0)
		{
#ifdef _RTC_Debug
			printf("aborting, %2x\n", s_rtc.Counter);
#endif
			REPORT_ABNORMAL_ID(AbnormalID::kRTC_RTC_aborting, "RTC aborting");
		}
		s_rtc.Mode = 0;
		s_rtc.DataOut = 0;
		s_rtc.DataNextOut = 0;
		s_rtc.ShiftData = 0;
		s_rtc.Counter = 0;
	}
}

void RTCDevice::clockChangeNtfy()
{
	if (!RTCunEnabled)
	{
		if (RTCclock)
		{
			s_rtc.DataOut = s_rtc.DataNextOut;
			s_rtc.Counter = (s_rtc.Counter - 1) & 0x07;
			if (s_rtc.DataOut)
			{
				g_wires.set(Wire_VIA1_iB0_RTCdataLine, (s_rtc.ShiftData >> s_rtc.Counter) & 0x01);
				/*
					should notify VIA if changed, so can check
					data direction
				*/
				if (s_rtc.Counter == 0)
				{
					s_rtc.DataNextOut = 0;
				}
			}
			else
			{
				s_rtc.ShiftData = (s_rtc.ShiftData << 1) | RTCdataLine;
				if (s_rtc.Counter == 0)
				{
					RTC_DoCmd();
				}
			}
		}
	}
}

void RTCDevice::dataLineChangeNtfy()
{
	if (s_rtc.DataOut)
	{
		if (!s_rtc.DataNextOut)
		{
			/*
				ignore. The g_rom doesn't read from the RTC the
				way described in the Hardware Reference.
				It reads the data after setting the clock to
				one instead of before, and then immediately
				changes the VIA direction. So the RTC
				has no way of knowing to stop driving the
				data line, which certainly can't really be
				correct.
			*/
		}
		else
		{
			REPORT_ABNORMAL_ID(AbnormalID::kRTC_write_RTC_Data_unexpected_direction,
							   "write RTC Data unexpected direction");
		}
	}
}
