/*
	machine — Legacy global hardware definitions

	Model constants, memory globals (g_ram, g_rom, g_vidMem), address
	translation table (ATT), device memory dispatch, interrupt
	scheduling, wires, and extension definitions.
*/
#pragma once

/* --- Mac model IDs --- */

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

/* --- Memory globals --- */

extern uint8_t * g_ram;
	/*
		allocated by OSGLUxxx to be at least
			kRAM_Size + RAMSafetyMarginFudge
		bytes. Because of shortcuts taken in GLOBGLUE.c, it is in theory
		possible for the emulator to write up to 3 bytes past kRAM_Size.
	*/

extern uint8_t * g_vidROM;
extern uint8_t * g_vidMem;

// Rebuild the address translation table after overlay/32-bit mode change.
extern void MemOverlay_ChangeNtfy();

extern void Addr32_ChangeNtfy();

/*
	representation of pointer into memory of emulated computer:
	uint32_t (was CPTR typedef)
*/

/*
	mapping of address space to real memory
*/

// Map a guest address to a host pointer.  Returns nullptr on failure.
extern uint8_t * get_real_address0(uint32_t L, bool WritableMem, uint32_t addr,
	uint32_t *actL);

/*
	memory access routines that can use when have address
	that is known to be in g_ram (and that is in the first
	copy of the ram, not the duplicates, i.e. < kRAM_Size).
*/

#ifndef ln2mtb

#define get_ram_byte(addr) do_get_mem_byte((addr) + g_ram)
#define get_ram_word(addr) do_get_mem_word((addr) + g_ram)
#define get_ram_long(addr) do_get_mem_long((addr) + g_ram)

#define put_ram_byte(addr, b) do_put_mem_byte((addr) + g_ram, (b))
#define put_ram_word(addr, w) do_put_mem_word((addr) + g_ram, (w))
#define put_ram_long(addr, l) do_put_mem_long((addr) + g_ram, (l))

#else

#define get_ram_byte get_vm_byte
#define get_ram_word get_vm_word
#define get_ram_long get_vm_long

#define put_ram_byte put_vm_byte
#define put_ram_word put_vm_word
#define put_ram_long put_vm_long

#endif

#define get_ram_address(addr) ((addr) + g_ram)

/*
	accessing addresses that don't map to
	real memory, i.e. memory mapped devices
*/

// Initialize the address space (build ATT from machine config).
extern bool AddrSpac_Init();



#define ui5r_FromUByte(x) ((uint32_t)(uint8_t)(x))
#define ui5r_FromUWord(x) ((uint32_t)(uint16_t)(x))
#define ui5r_FromULong(x) ((uint32_t)(uint32_t)(x))


/* Global instruction counter – defined in m68k.cpp */
extern uint32_t g_InstructionCount;

/* Logging range [g_LogStart, g_LogEnd).  Set from --log-start / --log-count.
   Default 0,0 = no logging.  exit(0) when g_InstructionCount reaches g_LogEnd. */
extern uint32_t g_LogStart;
extern uint32_t g_LogEnd;

extern void dbglog_StartLine();

#if dbglog_HAVE
extern void dbglog_WriteMemArrow(bool writeMem);

extern void dbglog_WriteNote(char *s);
extern void dbglog_WriteSetBool(char *s, bool v);
extern void dbglog_AddrAccess(char *s,
	uint32_t data, bool writeMem, uint32_t addr);
extern void dbglog_Access(char *s, uint32_t data, bool writeMem);
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

extern void VIAorSCCinterruptChngNtfy();

extern bool g_interruptButton;
extern void SetInterruptButton(bool v);

/* --- Interrupt scheduling (ICT) --- */

enum {
	kICT_SubTick,
	kICT_Kybd_ReceiveCommand,
	kICT_Kybd_ReceiveEndCommand,
	kICT_ADB_NewState,
	kICT_PMU_Task,
	kICT_VIA1_Timer1Check,
	kICT_VIA1_Timer2Check,
	kICT_VIA2_Timer1Check,
	kICT_VIA2_Timer2Check,

	kNumICTs
};

using iCountt = uint32_t;

extern uint8_t* g_wiresData;

#define kLn2CycleScale 6
#define kCycleScale (1 << kLn2CycleScale)

#if WANT_CYC_BY_PRI_OP
#define RdAvgXtraCyc /* 0 */ (kCycleScale + kCycleScale / 4)
#define WrAvgXtraCyc /* 0 */ (kCycleScale + kCycleScale / 4)
#endif

#define kNumSubTicks 16


extern uint16_t MasterEvtQLock;
extern bool FindKeyEvent(int *VirtualKey, bool *KeyDown);


/* maxivmac extensions */

/* --- Extension IDs (guest-to-host trap interface) --- */

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
	kExtnVideo, /* must stay at index 3 to match reference VidROM */
#if INCLUDE_EXTN_PBUFS
	kExtnParamBuffers,
#endif
#if INCLUDE_EXTN_HOST_TEXT_CLIP_EXCHANGE
	kExtnHostTextClipExchange,
#endif

	kNumExtns
};

#define kcom_callcheck 0x5B17

extern uint32_t g_diskIconAddr;

extern void memoryReset();

extern void extnReset();

extern void customreset();

/*
	Address Translation Table entry.
	Maps a guest address range to host memory or a device handler.
*/
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
using ATTep = ATTer *;

constexpr int kATTA_readreadybit  = 0;
constexpr int kATTA_writereadybit = 1;
constexpr int kATTA_mmdvbit       = 2;
constexpr int kATTA_ntfybit       = 3;

constexpr int kATTA_readwritereadymask =
	(1 << kATTA_readreadybit) | (1 << kATTA_writereadybit);
constexpr int kATTA_readreadymask  = (1 << kATTA_readreadybit);
constexpr int kATTA_writereadymask = (1 << kATTA_writereadybit);
constexpr int kATTA_mmdvmask       = (1 << kATTA_mmdvbit);
constexpr int kATTA_ntfymask       = (1 << kATTA_ntfybit);

// Dispatch a memory-mapped device access through the ATT entry.
extern uint32_t MMDV_Access(ATTep p, uint32_t data,
	bool writeMem, bool byteSize, uint32_t addr);

// Notify callback when a memory region is first accessed.
extern bool MemAccessNtfy(ATTep pT);
