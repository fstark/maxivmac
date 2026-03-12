/*
	GLOBGLUE.h

	Copyright (C) 2003 Bernd Schmidt, Philip Cummins, Paul C. Pratt

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


#define kEmMd_Twig43      0
#define kEmMd_Twiggy      1
#define kEmMd_128K        2
#define kEmMd_512Ke       3
#define kEmMd_Kanji       4
#define kEmMd_Plus        5
#define kEmMd_SE          6
#define kEmMd_SEFDHD      7
#define kEmMd_Classic     8
#define kEmMd_PB100       9
#define kEmMd_II         10
#define kEmMd_IIx        11

#define RAMSafetyMarginFudge 4

#define kRAM_Size (kRAMa_Size + kRAMb_Size)
extern uint8_t * RAM;
	/*
		allocated by OSGLUxxx to be at least
			kRAM_Size + RAMSafetyMarginFudge
		bytes. Because of shortcuts taken in GLOBGLUE.c, it is in theory
		possible for the emulator to write up to 3 bytes past kRAM_Size.
	*/

#if EmVidCard
extern uint8_t * VidROM;
#endif

#if IncludeVidMem
extern uint8_t * VidMem;
#endif

extern void MemOverlay_ChangeNtfy(void);

#if (CurEmMd == kEmMd_II) || (CurEmMd == kEmMd_IIx)
extern void Addr32_ChangeNtfy(void);
#endif

/*
	representation of pointer into memory of emulated computer:
	uint32_t (was CPTR typedef)
*/

/*
	mapping of address space to real memory
*/

extern uint8_t * get_real_address0(uint32_t L, bool WritableMem, uint32_t addr,
	uint32_t *actL);

/*
	memory access routines that can use when have address
	that is known to be in RAM (and that is in the first
	copy of the ram, not the duplicates, i.e. < kRAM_Size).
*/

#ifndef ln2mtb

#define get_ram_byte(addr) do_get_mem_byte((addr) + RAM)
#define get_ram_word(addr) do_get_mem_word((addr) + RAM)
#define get_ram_long(addr) do_get_mem_long((addr) + RAM)

#define put_ram_byte(addr, b) do_put_mem_byte((addr) + RAM, (b))
#define put_ram_word(addr, w) do_put_mem_word((addr) + RAM, (w))
#define put_ram_long(addr, l) do_put_mem_long((addr) + RAM, (l))

#else

#define get_ram_byte get_vm_byte
#define get_ram_word get_vm_word
#define get_ram_long get_vm_long

#define put_ram_byte put_vm_byte
#define put_ram_word put_vm_word
#define put_ram_long put_vm_long

#endif

#define get_ram_address(addr) ((addr) + RAM)

/*
	accessing addresses that don't map to
	real memory, i.e. memory mapped devices
*/

extern bool AddrSpac_Init(void);


#define ui5r_FromSByte(x) ((uint32_t)(int32_t)(int8_t)(uint8_t)(x))
#define ui5r_FromSWord(x) ((uint32_t)(int32_t)(int16_t)(uint16_t)(x))
#define ui5r_FromSLong(x) ((uint32_t)(int32_t)(int32_t)(uint32_t)(x))

#define ui5r_FromUByte(x) ((uint32_t)(uint8_t)(x))
#define ui5r_FromUWord(x) ((uint32_t)(uint16_t)(x))
#define ui5r_FromULong(x) ((uint32_t)(uint32_t)(x))


#if WantDisasm
extern void dbglog_StartLine(void);
#else
#define dbglog_StartLine()
#endif

#if dbglog_HAVE
extern void dbglog_WriteMemArrow(bool WriteMem);

extern void dbglog_WriteNote(char *s);
extern void dbglog_WriteSetBool(char *s, bool v);
extern void dbglog_AddrAccess(char *s,
	uint32_t Data, bool WriteMem, uint32_t addr);
extern void dbglog_Access(char *s, uint32_t Data, bool WriteMem);
#endif

#if ! WantAbnormalReports
#define ReportAbnormalID(id, s)
#else
#if dbglog_HAVE
#define ReportAbnormalID DoReportAbnormalID
#else
#define ReportAbnormalID(id, s) DoReportAbnormalID(id)
#endif
extern void DoReportAbnormalID(uint16_t id
#if dbglog_HAVE
	, char *s
#endif
	);
#endif /* WantAbnormalReports */

extern void VIAorSCCinterruptChngNtfy(void);

extern bool InterruptButton;
extern void SetInterruptButton(bool v);

enum {
	kICT_SubTick,
#if EmClassicKbrd
	kICT_Kybd_ReceiveCommand,
	kICT_Kybd_ReceiveEndCommand,
#endif
#if EmADB
	kICT_ADB_NewState,
#endif
#if EmPMU
	kICT_PMU_Task,
#endif
#if EmVIA1
	kICT_VIA1_Timer1Check,
	kICT_VIA1_Timer2Check,
#endif
#if EmVIA2
	kICT_VIA2_Timer1Check,
	kICT_VIA2_Timer2Check,
#endif

	kNumICTs
};

extern void ICT_add(int taskid, uint32_t n);

using iCountt = uint32_t;
extern iCountt GetCuriCount(void);
extern void ICT_Zap(void);

extern uint32_t ICTactive;
extern iCountt ICTwhen[kNumICTs];
extern iCountt NextiCount;

extern uint8_t Wires[kNumWires];

#define kLn2CycleScale 6
#define kCycleScale (1 << kLn2CycleScale)

#if WantCycByPriOp
#define RdAvgXtraCyc /* 0 */ (kCycleScale + kCycleScale / 4)
#define WrAvgXtraCyc /* 0 */ (kCycleScale + kCycleScale / 4)
#endif

#define kNumSubTicks 16


#define HaveMasterMyEvtQLock EmClassicKbrd
#if HaveMasterMyEvtQLock
extern uint16_t MasterMyEvtQLock;
#endif
extern bool FindKeyEvent(int *VirtualKey, bool *KeyDown);


/* minivmac extensions */

#define ExtnDat_checkval 0
#define ExtnDat_extension 2
#define ExtnDat_commnd 4
#define ExtnDat_result 6
#define ExtnDat_params 8

#define kCmndVersion 0
#define ExtnDat_version 8

enum {
	kExtnFindExtn, /* must be first */

	kExtnDisk,
	kExtnSony,
#if EmVidCard
	kExtnVideo,
#endif
#if IncludeExtnPbufs
	kExtnParamBuffers,
#endif
#if IncludeExtnHostTextClipExchange
	kExtnHostTextClipExchange,
#endif

	kNumExtns
};

#define kcom_callcheck 0x5B17

extern uint32_t my_disk_icon_addr;

extern void Memory_Reset(void);

extern void Extn_Reset(void);

extern void customreset(void);

class Device; // forward declaration for ATT Device* dispatch

struct ATTer {
	struct ATTer *Next;
	uint32_t cmpmask;
	uint32_t cmpvalu;
	uint32_t Access;
	uint32_t usemask; /* Should be one less than a power of two. */
	uint8_t * usebase;
	Device * device;  /* Device for MMDV dispatch (nullptr for RAM/ROM) */
	uint8_t MMDV;
	uint8_t Ntfy;
	uint16_t Pad0;
};
typedef struct ATTer ATTer;
typedef ATTer *ATTep;

#define kATTA_readreadybit 0
#define kATTA_writereadybit 1
#define kATTA_mmdvbit 2
#define kATTA_ntfybit 3

#define kATTA_readwritereadymask \
	((1 << kATTA_readreadybit) | (1 << kATTA_writereadybit))
#define kATTA_readreadymask (1 << kATTA_readreadybit)
#define kATTA_writereadymask (1 << kATTA_writereadybit)
#define kATTA_mmdvmask (1 << kATTA_mmdvbit)
#define kATTA_ntfymask (1 << kATTA_ntfybit)

extern uint32_t MMDV_Access(ATTep p, uint32_t Data,
	bool WriteMem, bool ByteSize, uint32_t addr);
extern bool MemAccessNtfy(ATTep pT);
