/*
	GLOBGLUE.c

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

/*
	GLOBal GLUE (or GLOB of GLUE)

	Holds the program together.

	Some code here adapted from "custom.c" in vMac by Philip Cummins,
	in turn descended from code in the Un*x Amiga Emulator by
	Bernd Schmidt.
*/

#include "core/common.h"

/* Device headers for ATT Device* dispatch */
#include "devices/device.h"
#include "devices/via.h"
#include "devices/via2.h"
#include "devices/scc.h"
#include "devices/scsi.h"
#include "devices/iwm.h"
#include "devices/asc.h"
#include "devices/sony.h"
#include "devices/video.h"
#include "devices/adb.h"
#include "devices/rtc.h"
#include "core/wire_bus.h"
#include "cpu/cpu.h"
#include "core/machine_obj.h"

/*
	ReportAbnormalID unused 0x111D - 0x11FF
*/

/*
	ReportAbnormalID ranges unused 0x12xx - 0xFFxx
*/

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

extern void put_vm_byte(uint32_t addr, uint8_t b);
extern void put_vm_word(uint32_t addr, uint16_t w);
extern void put_vm_long(uint32_t addr, uint32_t l);

uint32_t my_disk_icon_addr;

void customreset(void)
{
	if (auto* d = g_machine->findDevice<IWMDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCCDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCSIDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->reset();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->reset();
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->reset();
	Extn_Reset();
	if (g_machine->config().isCompactMac()) {
		WantMacReset = true;
		/*
			kludge, code in Finder appears
			to do RESET and not expect
			to come back. Maybe asserting
			the RESET somehow causes
			other hardware compenents to
			later reset the 68000.
		*/
	}
}

uint8_t * RAM = nullptr;
uint8_t * VidROM = nullptr;
uint8_t * VidMem = nullptr;

uint8_t* Wires = nullptr;


#if WantDisasm
extern void m68k_WantDisasmContext(void);
#endif

#if WantDisasm
void dbglog_StartLine(void)
{
	m68k_WantDisasmContext();
	dbglog_writeCStr(" ");
}
#endif

#if dbglog_HAVE
void dbglog_WriteMemArrow(bool WriteMem)
{
	if (WriteMem) {
		dbglog_writeCStr(" <- ");
	} else {
		dbglog_writeCStr(" -> ");
	}
}
#endif

#if dbglog_HAVE
void dbglog_AddrAccess(char *s, uint32_t Data,
	bool WriteMem, uint32_t addr)
{
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_writeCStr("[");
	dbglog_writeHex(addr);
	dbglog_writeCStr("]");
	dbglog_WriteMemArrow(WriteMem);
	dbglog_writeHex(Data);
	dbglog_writeReturn();
}
#endif

#if dbglog_HAVE
void dbglog_Access(char *s, uint32_t Data, bool WriteMem)
{
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_WriteMemArrow(WriteMem);
	dbglog_writeHex(Data);
	dbglog_writeReturn();
}
#endif

#if dbglog_HAVE
void dbglog_WriteNote(char *s)
{
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}
#endif

#if dbglog_HAVE
void dbglog_WriteSetBool(char *s, bool v)
{
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_writeCStr(" <- ");
	if (v) {
		dbglog_writeCStr("1");
	} else {
		dbglog_writeCStr("0");
	}
	dbglog_writeReturn();
}
#endif

#if WantAbnormalReports
static bool GotOneAbnormal = false;
#endif

#ifndef ReportAbnormalInterrupt
#define ReportAbnormalInterrupt 0
#endif

#if WantAbnormalReports
void DoReportAbnormalID(uint16_t id
#if dbglog_HAVE
	, char *s
#endif
	)
{
#if dbglog_HAVE
	dbglog_StartLine();
	dbglog_writeCStr("*** abnormal : ");
	dbglog_writeCStr(s);
	dbglog_writeReturn();
#endif

	if (! GotOneAbnormal) {
		WarnMsgAbnormalID(id);
#if ReportAbnormalInterrupt
		SetInterruptButton(true);
#endif
		GotOneAbnormal = true;
	}
}
#endif

/* map of address space — addresses vary per model */

#define kRAM_Base 0x00000000 /* when overlay off */

static uint32_t addrmap_kRAM_ln2Spc() {
	auto m = g_machine->config().model;
	if (m == MacModel::PB100 || g_machine->config().isIIFamily())
		return 23;
	return 22;
}
#define kRAM_ln2Spc addrmap_kRAM_ln2Spc()

static uint32_t addrmap_kVidMem_Base() {
	if (g_machine->config().model == MacModel::PB100) return 0x00FA0000;
	return 0x00540000;
}
static uint32_t addrmap_kVidMem_ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 18;
}
#define kVidMem_Base addrmap_kVidMem_Base()
#define kVidMem_ln2Spc addrmap_kVidMem_ln2Spc()

static uint32_t addrmap_kSCSI_Block_Base() {
	if (g_machine->config().model == MacModel::PB100) return 0x00F90000;
	return 0x00580000;
}
static uint32_t addrmap_kSCSI_ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 19;
}
#define kSCSI_Block_Base addrmap_kSCSI_Block_Base()
#define kSCSI_ln2Spc addrmap_kSCSI_ln2Spc()

#define kRAM_Overlay_Base 0x00600000 /* when overlay on */
#define kRAM_Overlay_Top  0x00800000

static uint32_t addrmap_kSCCRd_Block_Base() {
	if (g_machine->config().model == MacModel::PB100) return 0x00FD0000;
	return 0x00800000;
}
static uint32_t addrmap_kSCC_ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 22;
}
#define kSCCRd_Block_Base addrmap_kSCCRd_Block_Base()
#define kSCC_ln2Spc addrmap_kSCC_ln2Spc()

/* SCC write block: not present on PB100 (combined read/write) */
#define kSCCWr_Block_Base 0x00A00000
#define kSCCWr_Block_Top  0x00C00000

static uint32_t addrmap_kIWM_Block_Base() {
	if (g_machine->config().model == MacModel::PB100) return 0x00F60000;
	return 0x00C00000;
}
static uint32_t addrmap_kIWM_ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 21;
}
#define kIWM_Block_Base addrmap_kIWM_Block_Base()
#define kIWM_ln2Spc addrmap_kIWM_ln2Spc()

static uint32_t addrmap_kVIA1_Block_Base() {
	if (g_machine->config().model == MacModel::PB100) return 0x00F70000;
	return 0x00E80000;
}
static uint32_t addrmap_kVIA1_ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 19;
}
#define kVIA1_Block_Base addrmap_kVIA1_Block_Base()
#define kVIA1_ln2Spc addrmap_kVIA1_ln2Spc()

/* ASC: on PB100, at a different address; other models use EmASC guard */
static uint32_t addrmap_kASC_Block_Base() {
	if (g_machine->config().model == MacModel::PB100) return 0x00FB0000;
	return 0x50F00000; /* Mac II 32-bit ASC base */
}
static uint32_t addrmap_kASC_ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 26; /* Mac II ASC space */
}
#define kASC_Block_Base addrmap_kASC_Block_Base()
#define kASC_ln2Spc addrmap_kASC_ln2Spc()
#define kASC_Mask 0x00000FFF


#if IncludeExtnPbufs
static tMacErr PbufTransferVM(uint32_t Buffera,
	tPbuf i, uint32_t offset, uint32_t count, bool IsWrite)
{
	tMacErr result;
	uint32_t contig;
	uint8_t * Buffer;

label_1:
	if (0 == count) {
		result = mnvm_noErr;
	} else {
		Buffer = get_real_address0(count, ! IsWrite, Buffera, &contig);
		if (0 == contig) {
			result = mnvm_miscErr;
		} else {
			PbufTransfer(Buffer, i, offset, contig, IsWrite);
			offset += contig;
			Buffera += contig;
			count -= contig;
			goto label_1;
		}
	}

	return result;
}
#endif

/* extension mechanism */

#if IncludeExtnPbufs
#define kCmndPbufFeatures 1
#define kCmndPbufNew 2
#define kCmndPbufDispose 3
#define kCmndPbufGetSize 4
#define kCmndPbufTransfer 5
#endif

#if IncludeExtnPbufs
static void ExtnParamBuffers_Access(uint32_t p)
{
	tMacErr result = mnvm_controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 1);
			result = mnvm_noErr;
			break;
		case kCmndPbufFeatures:
			put_vm_long(p + ExtnDat_params + 0, 0);
			result = mnvm_noErr;
			break;
		case kCmndPbufNew:
			{
				tPbuf Pbuf_No;
				uint32_t count = get_vm_long(p + ExtnDat_params + 4);
				/* reserved word at offset 2, should be zero */
				result = PbufNew(count, &Pbuf_No);
				put_vm_word(p + ExtnDat_params + 0, Pbuf_No);
			}
			break;
		case kCmndPbufDispose:
			{
				tPbuf Pbuf_No = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */
				result = CheckPbuf(Pbuf_No);
				if (mnvm_noErr == result) {
					PbufDispose(Pbuf_No);
				}
			}
			break;
		case kCmndPbufGetSize:
			{
				uint32_t Count;
				tPbuf Pbuf_No = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */

				result = PbufGetSize(Pbuf_No, &Count);
				if (mnvm_noErr == result) {
					put_vm_long(p + ExtnDat_params + 4, Count);
				}
			}
			break;
		case kCmndPbufTransfer:
			{
				uint32_t PbufCount;
				tPbuf Pbuf_No = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */
				uint32_t offset = get_vm_long(p + ExtnDat_params + 4);
				uint32_t count = get_vm_long(p + ExtnDat_params + 8);
				uint32_t Buffera = get_vm_long(p + ExtnDat_params + 12);
				bool IsWrite =
					(get_vm_word(p + ExtnDat_params + 16) != 0);
				result = PbufGetSize(Pbuf_No, &PbufCount);
				if (mnvm_noErr == result) {
					uint32_t endoff = offset + count;
					if ((endoff < offset) /* overflow */
						|| (endoff > PbufCount))
					{
						result = mnvm_eofErr;
					} else {
						result = PbufTransferVM(Buffera,
							Pbuf_No, offset, count, IsWrite);
					}
				}
			}
			break;
	}

	put_vm_word(p + ExtnDat_result, result);
}
#endif

#if IncludeExtnHostTextClipExchange
#define kCmndHTCEFeatures 1
#define kCmndHTCEExport 2
#define kCmndHTCEImport 3
#endif

#if IncludeExtnHostTextClipExchange
static void ExtnHostTextClipExchange_Access(uint32_t p)
{
	tMacErr result = mnvm_controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 1);
			result = mnvm_noErr;
			break;
		case kCmndHTCEFeatures:
			put_vm_long(p + ExtnDat_params + 0, 0);
			result = mnvm_noErr;
			break;
		case kCmndHTCEExport:
			{
				tPbuf Pbuf_No = get_vm_word(p + ExtnDat_params + 0);

				result = CheckPbuf(Pbuf_No);
				if (mnvm_noErr == result) {
					result = HTCEexport(Pbuf_No);
				}
			}
			break;
		case kCmndHTCEImport:
			{
				tPbuf Pbuf_No;
				result = HTCEimport(&Pbuf_No);
				put_vm_word(p + ExtnDat_params + 0, Pbuf_No);
			}
			break;
	}

	put_vm_word(p + ExtnDat_result, result);
}
#endif

#define kFindExtnExtension 0x64E1F58A
#define kDiskDriverExtension 0x4C9219E6
#if IncludeExtnPbufs
#define kHostParamBuffersExtension 0x314C87BF
#endif
#if IncludeExtnHostTextClipExchange
#define kHostClipExchangeExtension 0x27B130CA
#endif

#define kCmndFindExtnFind 1
#define kCmndFindExtnId2Code 2
#define kCmndFindExtnCount 3

#define kParamFindExtnTheExtn 8
#define kParamFindExtnTheId 12

static void ExtnFind_Access(uint32_t p)
{
	tMacErr result = mnvm_controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 1);
			result = mnvm_noErr;
			break;
		case kCmndFindExtnFind:
			{
				uint32_t extn = get_vm_long(p + kParamFindExtnTheExtn);

				if (extn == kDiskDriverExtension) {
					put_vm_word(p + kParamFindExtnTheId, kExtnDisk);
					result = mnvm_noErr;
				} else
#if IncludeExtnPbufs
				if (extn == kHostParamBuffersExtension) {
					put_vm_word(p + kParamFindExtnTheId,
						kExtnParamBuffers);
					result = mnvm_noErr;
				} else
#endif
#if IncludeExtnHostTextClipExchange
				if (extn == kHostClipExchangeExtension) {
					put_vm_word(p + kParamFindExtnTheId,
						kExtnHostTextClipExchange);
					result = mnvm_noErr;
				} else
#endif
				if (extn == kFindExtnExtension) {
					put_vm_word(p + kParamFindExtnTheId,
						kExtnFindExtn);
					result = mnvm_noErr;
				} else
				{
					/* not found */
				}
			}
			break;
		case kCmndFindExtnId2Code:
			{
				uint16_t extn = get_vm_word(p + kParamFindExtnTheId);

				if (extn == kExtnDisk) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kDiskDriverExtension);
					result = mnvm_noErr;
				} else
#if IncludeExtnPbufs
				if (extn == kExtnParamBuffers) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kHostParamBuffersExtension);
					result = mnvm_noErr;
				} else
#endif
#if IncludeExtnHostTextClipExchange
				if (extn == kExtnHostTextClipExchange) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kHostClipExchangeExtension);
					result = mnvm_noErr;
				} else
#endif
				if (extn == kExtnFindExtn) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kFindExtnExtension);
					result = mnvm_noErr;
				} else
				{
					/* not found */
				}
			}
			break;
		case kCmndFindExtnCount:
			put_vm_word(p + kParamFindExtnTheId, kNumExtns);
			result = mnvm_noErr;
			break;
	}

	put_vm_word(p + ExtnDat_result, result);
}

#define kDSK_Params_Hi 0
#define kDSK_Params_Lo 1
#define kDSK_QuitOnEject 3 /* obsolete */

static uint16_t ParamAddrHi;

static void Extn_Access(uint32_t Data, uint32_t addr)
{
	switch (addr) {
		case kDSK_Params_Hi:
			ParamAddrHi = Data;
			break;
		case kDSK_Params_Lo:
			{
				uint32_t p = ParamAddrHi << 16 | Data;

				ParamAddrHi = (uint16_t) - 1;
				if (kcom_callcheck == get_vm_word(p + ExtnDat_checkval))
				{
					put_vm_word(p + ExtnDat_checkval, 0);

					switch (get_vm_word(p + ExtnDat_extension)) {
						case kExtnFindExtn:
							ExtnFind_Access(p);
							break;
						case kExtnVideo:
							if (auto* d = g_machine->findDevice<VideoDevice>()) d->extnVideoAccess(p);
							break;
#if IncludeExtnPbufs
						case kExtnParamBuffers:
							ExtnParamBuffers_Access(p);
							break;
#endif
#if IncludeExtnHostTextClipExchange
						case kExtnHostTextClipExchange:
							ExtnHostTextClipExchange_Access(p);
							break;
#endif
						case kExtnDisk:
							if (auto* d = g_machine->findDevice<SonyDevice>()) d->extnDiskAccess(p);
							break;
						case kExtnSony:
							if (auto* d = g_machine->findDevice<SonyDevice>()) d->extnSonyAccess(p);
							break;
						default:
							put_vm_word(p + ExtnDat_result,
								mnvm_controlErr);
							break;
					}
				}
			}
			break;
		case kDSK_QuitOnEject:
			/* obsolete, kept for compatibility */
			if (auto* d = g_machine->findDevice<SonyDevice>()) d->setQuitOnEject();
			break;
	}
}

/*
	ExtnDevice: wraps the ROM extension slot mechanism (Sony, Video, etc.)
	as a Device for ATT dispatch.
*/
class ExtnDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override {
		Extn_Access(data, addr);
		return data;
	}
	void zap() override {}
	void reset() override {}
	const char* name() const override { return "Extn"; }
};
static ExtnDevice g_extnDevice;

void Extn_Reset(void)
{
	ParamAddrHi = (uint16_t) - 1;
}

/* implementation of read/write for everything but RAM and ROM */

#define kSCC_Mask 0x03

#define kVIA1_Mask 0x00000F
#define kVIA2_Mask 0x00000F

#define kIWM_Mask 0x00000F /* Allocated Memory Bandwidth for IWM */

static uint32_t addrmap_ROM_CmpZeroMask() {
	const auto& cfg = g_machine->config();
	auto m = static_cast<int>(cfg.model);
	if (m <= static_cast<int>(MacModel::Mac512Ke)) return 0;
	if (m <= static_cast<int>(MacModel::Plus)) {
		return (cfg.romSize > 0x00020000) ? 0 : 0x00020000;
	}
	/* SE, Classic, PB100, II, IIx all use 0 */
	return 0;
}
#define ROM_CmpZeroMask addrmap_ROM_CmpZeroMask()

#define kROM_cmpmask (0x00F00000 | ROM_CmpZeroMask)

static uint32_t addrmap_Overlay_ROM_CmpZeroMask() {
	auto m = static_cast<int>(g_machine->config().model);
	if (m <= static_cast<int>(MacModel::Mac512Ke)) return 0x00100000;
	if (m <= static_cast<int>(MacModel::Plus))     return 0x00020000;
	if (m <= static_cast<int>(MacModel::Classic))   return 0x00300000;
	/* PB100, II, IIx */
	return 0;
}
#define Overlay_ROM_CmpZeroMask addrmap_Overlay_ROM_CmpZeroMask()

enum {
	kMMDV_VIA1,
	kMMDV_VIA2,
	kMMDV_SCC,
	kMMDV_Extn,
	kMMDV_ASC,
	kMMDV_SCSI,
	kMMDV_IWM,

	kNumMMDVs
};

enum {
	kMAN_OverlayOff, /* present on SE and later */

	kNumMANs
};


/* Max ATT entries — generous fixed size, checked at runtime */
#define kATTListMax 64
static ATTer ATTListA[kATTListMax];
static uint16_t LastATTel;


static void AddToATTList(ATTep p)
{
	uint16_t NewLast = LastATTel + 1;
	if (NewLast >= kATTListMax) {
		ReportAbnormalID(0x1101, "ATT list not big enough");
	} else {
		ATTListA[LastATTel] = *p;
		LastATTel = NewLast;
	}
}

static void InitATTList(void)
{
	LastATTel = 0;
}

static void FinishATTList(void)
{
	{
		/* add guard */
		ATTer r{};

		r.cmpmask = 0;
		r.cmpvalu = 0;
		r.usemask = 0;
		r.usebase = nullptr;
		r.Access = 0;
		AddToATTList(&r);
	}

	{
		uint16_t i = LastATTel;
		ATTep p = &ATTListA[LastATTel];
		ATTep h = nullptr;

		while (0 != i) {
			--i;
			--p;
			p->Next = h;
			h = p;
		}

#if 0 /* verify list. not for final version */
		{
			ATTep q1;
			ATTep q2;
			for (q1 = h; nullptr != q1->Next; q1 = q1->Next) {
				if ((q1->cmpvalu & ~ q1->cmpmask) != 0) {
					ReportAbnormalID(0x1102, "ATTListA bad entry");
				}
				for (q2 = q1->Next; nullptr != q2->Next; q2 = q2->Next) {
					uint32_t common_mask = (q1->cmpmask) & (q2->cmpmask);
					if ((q1->cmpvalu & common_mask) ==
						(q2->cmpvalu & common_mask))
					{
						ReportAbnormalID(0x1103, "ATTListA Conflict");
					}
				}
			}
		}
#endif

		g_cpu.setHeadATTel(h);
	}
}

/* Mac II/IIx: RAM24 setup with VIA2 bank select */
static void SetUp_RAM24(void)
{
	const auto& cfg = g_machine->config();
	ATTer r{};
	uint32_t bankbit = 0x00100000 << (((VIA2_iA7 << 1) | VIA2_iA6) << 1);

	if (cfg.ramASize == cfg.ramBSize) {
		if (cfg.ramASize == bankbit) {
			/* properly set up balanced RAM */
			r.cmpmask = 0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1);
			r.cmpvalu = 0;
			r.usemask = ((1 << kRAM_ln2Spc) - 1) & (cfg.ramSize() - 1);
			r.usebase = RAM;
			r.Access = kATTA_readwritereadymask;
			AddToATTList(&r);
		} else
		{
			bankbit &= 0x00FFFFFF; /* if too large, always use RAMa */

			if (0 != bankbit) {
				if (cfg.ramBSize != 0) {
					r.cmpmask = bankbit
						| (0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1));
					r.cmpvalu = bankbit;
					r.usemask = ((1 << kRAM_ln2Spc) - 1) & (cfg.ramBSize - 1);
					r.usebase = cfg.ramASize + RAM;
					r.Access = kATTA_readwritereadymask;
					AddToATTList(&r);
				}
			}

			{
				r.cmpmask = bankbit
					| (0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1));
				r.cmpvalu = 0;
				r.usemask = ((1 << kRAM_ln2Spc) - 1) & (cfg.ramASize - 1);
				r.usebase = RAM;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}
		}
	} else {
		bankbit &= 0x00FFFFFF;

		if (0 != bankbit) {
			if (cfg.ramBSize != 0) {
				r.cmpmask = bankbit
					| (0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1));
				r.cmpvalu = bankbit;
				r.usemask = ((1 << kRAM_ln2Spc) - 1) & (cfg.ramBSize - 1);
				r.usebase = cfg.ramASize + RAM;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}
		}

		{
			r.cmpmask = bankbit
				| (0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1));
			r.cmpvalu = 0;
			r.usemask = ((1 << kRAM_ln2Spc) - 1) & (cfg.ramASize - 1);
			r.usebase = RAM;
			r.Access = kATTA_readwritereadymask;
			AddToATTList(&r);
		}
	}
}

/* Mac II/IIx: I/O space setup */
static void SetUp_io(void)
{
	ATTer r{};

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_VIA1;
	r.device = g_machine->findDevice<VIA1Device>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000 | 0x2000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000 | 0x2000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_VIA2;
	r.device = g_machine->findDevice<VIA2Device>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000 | 0x4000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000 | 0x4000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCC;
	r.device = g_machine->findDevice<SCCDevice>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000 | 0x0C000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000 | 0x0C000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_Extn;
	r.device = &g_extnDevice;
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000 | 0x10000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000 | 0x10000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCSI;
	r.device = g_machine->findDevice<SCSIDevice>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000 | 0x14000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000 | 0x14000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_ASC;
	r.device = g_machine->findDevice<ASCDevice>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = 0xFF01E000;
		r.cmpvalu = 0x50000000 | 0x16000;
	} else {
		r.cmpmask = 0x00F1E000;
		r.cmpvalu = 0x00F00000 | 0x16000;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_IWM;
	r.device = g_machine->findDevice<IWMDevice>();
	AddToATTList(&r);

#if 0
		case 14:
			/*
				fail, nothing supposed to be here,
				but rom accesses it anyway
			*/
			{
				uint32_t addr2 = addr & 0x1FFFF;

				if ((addr2 != 0x1DA00) && (addr2 != 0x1DC00)) {
					ReportAbnormalID(0x1104, "another unknown access");
				}
			}
			get_fail_realblock(p);
			break;
#endif
}

/* Mac II/IIx: 24-bit address space setup */
static void SetUp_address24(void)
{
	const auto& cfg = g_machine->config();
	ATTer r{};

#if 0
	if (MemOverlay) {
		ReportAbnormalID(0x1105, "Overlay with 24 bit addressing");
	}
#endif

	if (MemOverlay) {
		r.cmpmask = Overlay_ROM_CmpZeroMask |
			(0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1));
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.romSize - 1;
		r.usebase = ROM;
		r.Access = kATTA_readreadymask;
		AddToATTList(&r);
	} else {
		SetUp_RAM24();
	}

	r.cmpmask = kROM_cmpmask;
	r.cmpvalu = cfg.romBase;
	r.usemask = cfg.romSize - 1;
	r.usebase = ROM;
	r.Access = kATTA_readreadymask;
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
	r.cmpvalu = 0x900000;
	r.usemask = (cfg.vidMemSize - 1) & (0x100000 - 1);
	r.usebase = VidMem;
	r.Access = kATTA_readwritereadymask;
	AddToATTList(&r);
	if (cfg.vidMemSize >= 0x00200000) {
		r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
		r.cmpvalu = 0xA00000;
		r.usemask = (0x100000 - 1);
		r.usebase = VidMem + (1 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}
	if (cfg.vidMemSize >= 0x00400000) {
		r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
		r.cmpvalu = 0xB00000;
		r.usemask = (0x100000 - 1);
		r.usebase = VidMem + (2 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
		r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
		r.cmpvalu = 0xC00000;
		r.usemask = (0x100000 - 1);
		r.usebase = VidMem + (3 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}

	SetUp_io();
}

/* Mac II/IIx: 32-bit address space setup */
static void SetUp_address32(void)
{
	const auto& cfg = g_machine->config();
	ATTer r{};

	if (MemOverlay) {
		r.cmpmask = ~ ((1 << 30) - 1);
		r.cmpvalu = 0;
		r.usemask = cfg.romSize - 1;
		r.usebase = ROM;
		r.Access = kATTA_readreadymask;
		AddToATTList(&r);
	} else {
		uint32_t bankbit =
			0x00100000 << (((VIA2_iA7 << 1) | VIA2_iA6) << 1);
		if (cfg.ramASize == cfg.ramBSize) {
			if (cfg.ramASize == bankbit) {
				/* properly set up balanced RAM */
				r.cmpmask = ~ ((1 << 30) - 1);
				r.cmpvalu = 0;
				r.usemask = cfg.ramSize() - 1;
				r.usebase = RAM;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			} else
			{
				if (cfg.ramBSize != 0) {
					r.cmpmask = bankbit | ~ ((1 << 30) - 1);
					r.cmpvalu = bankbit;
					r.usemask = cfg.ramBSize - 1;
					r.usebase = cfg.ramASize + RAM;
					r.Access = kATTA_readwritereadymask;
					AddToATTList(&r);
				}

				r.cmpmask = bankbit | ~ ((1 << 30) - 1);
				r.cmpvalu = 0;
				r.usemask = cfg.ramASize - 1;
				r.usebase = RAM;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}
		} else {
			if (cfg.ramBSize != 0) {
				r.cmpmask = bankbit | ~ ((1 << 30) - 1);
				r.cmpvalu = bankbit;
				r.usemask = cfg.ramBSize - 1;
				r.usebase = cfg.ramASize + RAM;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}

			r.cmpmask = bankbit | ~ ((1 << 30) - 1);
			r.cmpvalu = 0;
			r.usemask = cfg.ramASize - 1;
			r.usebase = RAM;
			r.Access = kATTA_readwritereadymask;
			AddToATTList(&r);
		}
	}

	r.cmpmask = ~ ((1 << 28) - 1);
	r.cmpvalu = 0x40000000;
	r.usemask = cfg.romSize - 1;
	r.usebase = ROM;
	r.Access = kATTA_readreadymask;
	AddToATTList(&r);

#if 0
	/* haven't persuaded emulated computer to look here yet. */
	/* NuBus super space */
	r.cmpmask = ~ ((1 << 28) - 1);
	r.cmpvalu = 0x90000000;
	r.usemask = cfg.vidMemSize - 1;
	r.usebase = VidMem;
	r.Access = kATTA_readwritereadymask;
	AddToATTList(&r);
#endif

	/* Standard NuBus space */
	r.cmpmask = ~ ((1 << 20) - 1);
	r.cmpvalu = 0xF9F00000;
	r.usemask = cfg.vidROMSize - 1;
	r.usebase = VidROM;
	r.Access = kATTA_readreadymask;
	AddToATTList(&r);
#if 0
	r.cmpmask = ~ 0x007FFFFF;
	r.cmpvalu = 0xF9000000;
	r.usemask = 0x007FFFFF & (cfg.vidMemSize - 1);
	r.usebase = VidMem;
	r.Access = kATTA_readwritereadymask;
	AddToATTList(&r);
#endif

	r.cmpmask = ~ 0x000FFFFF;
	r.cmpvalu = 0xF9900000;
	r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
	r.usebase = VidMem;
	r.Access = kATTA_readwritereadymask;
	AddToATTList(&r);
/* kludge to allow more than 1M of Video Memory */
	if (cfg.vidMemSize >= 0x00200000) {
		r.cmpmask = ~ 0x000FFFFF;
		r.cmpvalu = 0xF9A00000;
		r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
		r.usebase = VidMem + (1 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}
	if (cfg.vidMemSize >= 0x00400000) {
		r.cmpmask = ~ 0x000FFFFF;
		r.cmpvalu = 0xF9B00000;
		r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
		r.usebase = VidMem + (2 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
		r.cmpmask = ~ 0x000FFFFF;
		r.cmpvalu = 0xF9C00000;
		r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
		r.usebase = VidMem + (3 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}

	SetUp_io();

#if 0
	if ((addr >= 0x58000000) && (addr < 0x58000004)) {
		/* test hardware. fail */
	}
#endif
}

/* Mac II/IIx: address space dispatcher */
static void SetUp_address_II(void)
{
	if (Addr32) {
		SetUp_address32();
	} else {
		SetUp_address24();
	}
}

/*
	unlike in the real Mac Plus, Mini vMac
	will allow misaligned memory access,
	since it is easier to allow it than
	it is to correctly simulate a bus error
	and back out of the current instruction.
*/

#ifndef ln2mtb
#define AddToATTListWithMTB AddToATTList
#else
static void AddToATTListWithMTB(ATTep p)
{
	/*
		Test of memory mapping system.
	*/
	ATTer r{};

	r.Access = p->Access;
	r.cmpmask = p->cmpmask | (1 << ln2mtb);
	r.usemask = p->usemask & ~ (1 << ln2mtb);

	r.cmpvalu = p->cmpvalu + (1 << ln2mtb);
	r.usebase = p->usebase;
	AddToATTList(&r);

	r.cmpvalu = p->cmpvalu;
	r.usebase = p->usebase + (1 << ln2mtb);
	AddToATTList(&r);
}
#endif

/* Compact Mac: simple 24-bit RAM setup (no VIA2 bank select) */
static void SetUp_RAM24_compact(void)
{
	const auto& cfg = g_machine->config();
	ATTer r{};

	if (cfg.ramBSize == 0 || cfg.ramASize == cfg.ramBSize) {
		r.cmpmask = 0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1);
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.ramSize() - 1;
		r.usebase = RAM;
		r.Access = kATTA_readwritereadymask;
		AddToATTListWithMTB(&r);
	} else {
		/* unbalanced memory */

		if (0 != (0x00FFFFFF & cfg.ramASize)) {
			/* condition should always be true if configuration file right */
			r.cmpmask = 0x00FFFFFF & (cfg.ramASize | ~ ((1 << kRAM_ln2Spc) - 1));
			r.cmpvalu = kRAM_Base + cfg.ramASize;
			r.usemask = cfg.ramBSize - 1;
			r.usebase = cfg.ramASize + RAM;
			r.Access = kATTA_readwritereadymask;
			AddToATTListWithMTB(&r);
		}

		r.cmpmask = 0x00FFFFFF & (cfg.ramASize | ~ ((1 << kRAM_ln2Spc) - 1));
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.ramASize - 1;
		r.usebase = RAM;
		r.Access = kATTA_readwritereadymask;
		AddToATTListWithMTB(&r);
	}
}

/* Compact Mac: 24-bit address space setup */
static void SetUp_address_compact(void)
{
	const auto& cfg = g_machine->config();
	ATTer r{};

	if (MemOverlay) {
		r.cmpmask = Overlay_ROM_CmpZeroMask |
			(0x00FFFFFF & ~ ((1 << kRAM_ln2Spc) - 1));
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.romSize - 1;
		r.usebase = ROM;
		r.Access = kATTA_readreadymask;
		AddToATTListWithMTB(&r);
	} else {
		SetUp_RAM24_compact();
	}

	r.cmpmask = kROM_cmpmask;
	r.cmpvalu = cfg.romBase;
	if (cfg.isSEOrLater() && MemOverlay) {
		r.usebase = nullptr;
		r.Access = kATTA_ntfymask;
		r.Ntfy = kMAN_OverlayOff;
		AddToATTList(&r);
	} else {
		r.usemask = cfg.romSize - 1;
		r.usebase = ROM;
		r.Access = kATTA_readreadymask;
		AddToATTListWithMTB(&r);
	}

	if (MemOverlay) {
		r.cmpmask = 0x00E00000;
		r.cmpvalu = kRAM_Overlay_Base;
		if (cfg.ramBSize == 0 || cfg.ramASize == cfg.ramBSize) {
			r.usemask = cfg.ramSize() - 1;
				/* note that cmpmask and usemask overlap for 4M */
			r.usebase = RAM;
			r.Access = kATTA_readwritereadymask;
		} else {
			/* unbalanced memory */
			r.usemask = cfg.ramBSize - 1;
			r.usebase = cfg.ramASize + RAM;
			r.Access = kATTA_readwritereadymask;
		}
		AddToATTListWithMTB(&r);
	}

	if (g_machine->config().includeVidMem) {
		r.cmpmask = 0x00FFFFFF & ~ ((1 << kVidMem_ln2Spc) - 1);
		r.cmpvalu = kVidMem_Base;
		r.usemask = g_machine->config().vidMemSize - 1;
		r.usebase = VidMem;
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}

	r.cmpmask = 0x00FFFFFF & ~ ((1 << kVIA1_ln2Spc) - 1);
	r.cmpvalu = kVIA1_Block_Base;
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_VIA1;
	r.device = g_machine->findDevice<VIA1Device>();
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ ((1 << kSCC_ln2Spc) - 1);
	r.cmpvalu = kSCCRd_Block_Base;
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCC;
	r.device = g_machine->findDevice<SCCDevice>();
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ ((1 << g_machine->config().extnLn2Spc) - 1);
	r.cmpvalu = g_machine->config().extnBlockBase;
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_Extn;
	r.device = &g_extnDevice;
	AddToATTList(&r);

	if (g_machine->config().model == MacModel::PB100) {
		r.cmpmask = 0x00FFFFFF & ~ ((1 << kASC_ln2Spc) - 1);
		r.cmpvalu = kASC_Block_Base;
		r.usebase = nullptr;
		r.Access = kATTA_mmdvmask;
		r.MMDV = kMMDV_ASC;
		r.device = g_machine->findDevice<ASCDevice>();
		AddToATTList(&r);
	}

	r.cmpmask = 0x00FFFFFF & ~ ((1 << kSCSI_ln2Spc) - 1);
	r.cmpvalu = kSCSI_Block_Base;
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCSI;
	r.device = g_machine->findDevice<SCSIDevice>();
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ ((1 << kIWM_ln2Spc) - 1);
	r.cmpvalu = kIWM_Block_Base;
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_IWM;
	r.device = g_machine->findDevice<IWMDevice>();
	AddToATTList(&r);
}

/* Unified address space setup — dispatches by model family */
static void SetUp_address(void)
{
	if (g_machine->config().isIIFamily()) {
		SetUp_address_II();
	} else {
		SetUp_address_compact();
	}
}

static void SetUpMemBanks(void)
{
	InitATTList();

	SetUp_address();

	FinishATTList();
}

#if 0
static void get_fail_realblock(ATTep p)
{
	p->cmpmask = 0;
	p->cmpvalu = 0xFFFFFFFF;
	p->usemask = 0;
	p->usebase = nullptr;
	p->Access = 0;
}
#endif

 uint32_t MMDV_Access(ATTep p, uint32_t Data,
	bool WriteMem, bool ByteSize, uint32_t addr)
{
	switch (p->MMDV) {
		case kMMDV_VIA1:
			if (! ByteSize) {
				if (g_machine->config().isIIFamily()
					&& WriteMem && (addr == 0xF40006))
				{
					/* for weirdness on shutdown in System 6 */
				} else
				{
					ReportAbnormalID(0x1106, "access VIA1 word");
				}
			} else if ((addr & 1) != 0) {
				ReportAbnormalID(0x1107, "access VIA1 odd");
			} else {
				if (g_machine->config().model != MacModel::PB100) {
					bool nonStandard;
					if (g_machine->config().isIIFamily()) {
						nonStandard = (addr & 0x000001FE) != 0x00000000;
					} else {
						nonStandard = (addr & 0x000FE1FE) != 0x000FE1FE;
					}
					if (nonStandard) {
						ReportAbnormalID(0x1108,
							"access VIA1 nonstandard address");
					}
				}
				Data = p->device->access(Data, WriteMem,
					(addr >> 9) & kVIA1_Mask);
			}

			break;
		case kMMDV_VIA2:
			if (! ByteSize) {
				if ((! WriteMem)
					&& ((0x3e00 == (addr & 0x1FFFF))
						|| (0x3e02 == (addr & 0x1FFFF))))
				{
					/* for weirdness at offset 0x71E in ROM */
					Data =
						(p->device->access(Data, WriteMem,
							(addr >> 9) & kVIA2_Mask) << 8)
						| p->device->access(Data, WriteMem,
							(addr >> 9) & kVIA2_Mask);

				} else {
					ReportAbnormalID(0x1109, "access VIA2 word");
				}
			} else if ((addr & 1) != 0) {
				if (0x3FFF == (addr & 0x1FFFF)) {
					/*
						for weirdness at offset 0x7C4 in ROM.
						looks like bug.
					*/
					Data = p->device->access(Data, WriteMem,
						(addr >> 9) & kVIA2_Mask);
				} else {
					ReportAbnormalID(0x110A, "access VIA2 odd");
				}
			} else {
				if ((addr & 0x000001FE) != 0x00000000) {
					ReportAbnormalID(0x110B,
						"access VIA2 nonstandard address");
				}
				Data = p->device->access(Data, WriteMem,
					(addr >> 9) & kVIA2_Mask);
			}
			break;
		case kMMDV_SCC:

			if (g_machine->config().isSEFamily()) {
				/* SE/Classic only: check for unassigned SCC address */
				if ((addr & 0x00100000) == 0) {
					ReportAbnormalID(0x110C,
						"access SCC unassigned address");
					break;
				}
			}
			if (! ByteSize) {
				ReportAbnormalID(0x110D, "Attemped Phase Adjust");
			} else
			if (!g_machine->config().isIIFamily()
				&& WriteMem != ((addr & 1) != 0))
			{
				if (WriteMem) {
					auto m = g_machine->config().model;
					if (static_cast<int>(m) >= static_cast<int>(MacModel::Mac512Ke)
						&& m != MacModel::PB100)
					{
						ReportAbnormalID(0x110E, "access SCC even/odd");
					}
				} else {
					if (auto* d = g_machine->findDevice<SCCDevice>()) d->reset();
				}
			} else
			if (g_machine->config().model != MacModel::PB100
				&& !g_machine->config().isIIFamily()
				&& WriteMem != (addr >= kSCCWr_Block_Base))
			{
				ReportAbnormalID(0x110F, "access SCC wr/rd base wrong");
			} else
			{
				if (g_machine->config().model != MacModel::PB100) {
					bool nonStandard;
					if (g_machine->config().isIIFamily()) {
						nonStandard = (addr & 0x1FF9) != 0x00000000;
					} else {
						nonStandard = (addr & 0x001FFFF8) != 0x001FFFF8;
					}
					if (nonStandard) {
						ReportAbnormalID(0x1110,
							"access SCC nonstandard address");
					}
				}
				Data = p->device->access(Data, WriteMem,
					(addr >> 1) & kSCC_Mask);
			}
			break;
		case kMMDV_Extn:
			if (ByteSize) {
				ReportAbnormalID(0x1111, "access Sony byte");
			} else if ((addr & 1) != 0) {
				ReportAbnormalID(0x1112, "access Sony odd");
			} else if (! WriteMem) {
				ReportAbnormalID(0x1113, "access Sony read");
			} else {
				p->device->access(Data, WriteMem, (addr >> 1) & 0x0F);
			}
			break;
		case kMMDV_ASC:
			if (! ByteSize) {
				if (g_machine->config().isIIFamily()) {
					if (WriteMem) {
						(void) p->device->access((Data >> 8) & 0x00FF,
							WriteMem, addr & kASC_Mask);
						Data = p->device->access((Data) & 0x00FF,
							WriteMem, (addr + 1) & kASC_Mask);
					} else {
						Data =
							(p->device->access((Data >> 8) & 0x00FF,
								WriteMem, addr & kASC_Mask) << 8)
							| p->device->access((Data) & 0x00FF,
								WriteMem, (addr + 1) & kASC_Mask);
					}
				} else {
					ReportAbnormalID(0x1114, "access ASC word");
				}
			} else {
				Data = p->device->access(Data, WriteMem, addr & kASC_Mask);
			}
			break;
		case kMMDV_SCSI:
			if (! ByteSize) {
				ReportAbnormalID(0x1115, "access SCSI word");
			} else
			if (!g_machine->config().isIIFamily()
				&& WriteMem != ((addr & 1) != 0))
			{
				ReportAbnormalID(0x1116, "access SCSI even/odd");
			} else
			{
				if (g_machine->config().isIIFamily()) {
					if ((addr & 0x1F8F) != 0x00000000) {
						ReportAbnormalID(0x1117,
							"access SCSI nonstandard address");
					}
				}
				Data = p->device->access(Data, WriteMem, (addr >> 4) & 0x07);
			}

			break;
		case kMMDV_IWM:
			if (g_machine->config().isSEFamily()) {
				if ((addr & 0x00100000) == 0) {
					ReportAbnormalID(0x1118,
						"access IWM unassigned address");
					break;
				}
			}
			if (! ByteSize) {
#if ExtraAbnormalReports
				ReportAbnormalID(0x1119, "access IWM word");
				/*
					This happens when quitting 'Glider 3.1.2'.
					perhaps a bad handle is being disposed of.
				*/
#endif
			} else if (g_machine->config().isIIFamily()) {
				if ((addr & 1) != 0) {
					ReportAbnormalID(0x111A, "access IWM odd");
				} else {
					Data = p->device->access(Data, WriteMem,
						(addr >> 9) & kIWM_Mask);
				}
			} else {
				if ((addr & 1) == 0) {
					ReportAbnormalID(0x111B, "access IWM even");
				} else {
					if (g_machine->config().model != MacModel::PB100
						&& !g_machine->config().isIIFamily())
					{
						if ((addr & 0x001FE1FF) != 0x001FE1FF) {
							ReportAbnormalID(0x111C,
								"access IWM nonstandard address");
						}
					}
					Data = p->device->access(Data, WriteMem,
						(addr >> 9) & kIWM_Mask);
				}
			}

			break;
	}

	return Data;
}

 bool MemAccessNtfy(ATTep pT)
{
	bool v = false;

	switch (pT->Ntfy) {
		case kMAN_OverlayOff:
			if (static_cast<int>(g_machine->config().model)
				>= static_cast<int>(MacModel::SE))
			{
				pT->Access = kATTA_readreadymask;

				g_wires.set(Wire_VIA1_iA4_MemOverlay, 0);

				v = true;
			}

			break;
	}

	return v;
}

void MemOverlay_ChangeNtfy(void)
{
	/* All models rebuild memory banks when overlay changes */
	SetUpMemBanks();
}

void Addr32_ChangeNtfy(void)
{
	/* Mac II/IIx use 24/32-bit addressing mode switch */
	if (g_machine->config().isIIFamily()) {
		SetUpMemBanks();
	}
}

static ATTep get_address_realblock1(bool WriteMem, uint32_t addr)
{
	ATTep p;

Label_Retry:
	p = g_cpu.findATTel(addr);
	if (0 != (p->Access &
		(WriteMem ? kATTA_writereadymask : kATTA_readreadymask)))
	{
		/* ok */
	} else {
		if (0 != (p->Access & kATTA_ntfymask)) {
			if (MemAccessNtfy(p)) {
				goto Label_Retry;
			}
		}
		p = nullptr; /* fail */
	}

	return p;
}

 uint8_t * get_real_address0(uint32_t L, bool WritableMem, uint32_t addr,
	uint32_t *actL)
{
	uint32_t bankleft;
	uint8_t * p;
	ATTep q;

	q = get_address_realblock1(WritableMem, addr);
	if (nullptr == q) {
		*actL = 0;
		p = nullptr;
	} else {
		uint32_t m2 = q->usemask & ~ q->cmpmask;
		uint32_t m3 = m2 & ~ (m2 + 1);
		p = q->usebase + (addr & q->usemask);
		bankleft = (m3 + 1) - (addr & m3);
		if (bankleft >= L) {
			/* this block is big enough (by far the most common case) */
			*actL = L;
		} else {
			*actL = bankleft;
		}
	}

	return p;
}

bool InterruptButton = false;

void SetInterruptButton(bool v)
{
	if (InterruptButton != v) {
		InterruptButton = v;
		VIAorSCCinterruptChngNtfy();
	}
}

static uint8_t CurIPL = 0;

void VIAorSCCinterruptChngNtfy(void)
{
	uint8_t NewIPL;

	if (g_machine->config().isIIFamily()) {
		/* Mac II priority: NMI > SCC > VIA2 > VIA1 */
		if (InterruptButton) {
			NewIPL = 7;
		} else if (SCCInterruptRequest) {
			NewIPL = 4;
		} else if (VIA2_InterruptRequest) {
			NewIPL = 2;
		} else if (VIA1_InterruptRequest) {
			NewIPL = 1;
		} else {
			NewIPL = 0;
		}
	} else {
		/* Compact Mac priority encoding */
		uint8_t VIAandNotSCC = VIA1_InterruptRequest
			& ~ SCCInterruptRequest;
		NewIPL = VIAandNotSCC
			| (SCCInterruptRequest << 1)
			| (InterruptButton << 2);
	}
	if (NewIPL != CurIPL) {
		CurIPL = NewIPL;
		g_cpu.iplChangeNotify();
	}
}

 bool AddrSpac_Init(void)
{
	g_wires.init(kNumWires);
	Wires = g_wires.data();

	/* Register wire change callbacks */
	g_wires.onChange(Wire_VIA1_iA4_MemOverlay, MemOverlay_ChangeNtfy);
	if (g_machine->config().isIIFamily()) {
		extern void PowerOff_ChangeNtfy(void);
		g_wires.onChange(Wire_VIA2_iA7_unknown, Addr32_ChangeNtfy);
		g_wires.onChange(Wire_VIA2_iA6_unknown, Addr32_ChangeNtfy);
		g_wires.onChange(Wire_VIA2_iB3_Addr32, Addr32_ChangeNtfy);
		g_wires.onChange(Wire_VIA2_iB2_PowerOff, PowerOff_ChangeNtfy);
	}
	g_wires.onChange(Wire_VIA1_InterruptRequest, VIAorSCCinterruptChngNtfy);
	g_wires.onChange(Wire_VIA2_InterruptRequest, VIAorSCCinterruptChngNtfy);
	g_wires.onChange(Wire_SCCInterruptRequest, VIAorSCCinterruptChngNtfy);
	if (g_machine->config().emRTC) {
		g_wires.onChange(Wire_VIA1_iB0_RTCdataLine, [](){ if (auto* d = g_machine->findDevice<RTCDevice>()) d->dataLineChangeNtfy(); });
		g_wires.onChange(Wire_VIA1_iB1_RTCclock, [](){ if (auto* d = g_machine->findDevice<RTCDevice>()) d->clockChangeNtfy(); });
		g_wires.onChange(Wire_VIA1_iB2_RTCunEnabled, [](){ if (auto* d = g_machine->findDevice<RTCDevice>()) d->unEnabledChangeNtfy(); });
	}
	if (g_machine->config().emADB) {
		g_wires.onChange(Wire_VIA1_iB4_ADB_st0, [](){ if (auto* d = g_machine->findDevice<ADBDevice>()) d->stateChangeNtfy(); });
		g_wires.onChange(Wire_VIA1_iB5_ADB_st1, [](){ if (auto* d = g_machine->findDevice<ADBDevice>()) d->stateChangeNtfy(); });
		g_wires.onChange(Wire_VIA1_iCB2_ADB_Data, [](){ if (auto* d = g_machine->findDevice<ADBDevice>()) d->dataLineChngNtfy(); });
	}

	g_cpu.init(
		&CurIPL, &g_machine->config());
	return true;
}

void Memory_Reset(void)
{
	g_wires.set(Wire_VIA1_iA4_MemOverlay, 1);
	SetUpMemBanks();
}

/* PowerOff_ChangeNtfy: only wired on Mac II/IIx (see AddrSpac_Init) */
extern void PowerOff_ChangeNtfy(void);
void PowerOff_ChangeNtfy(void)
{
	if (! VIA2_iB2) {
		ForceMacOff = true;
	}
}

/* user event queue utilities */

uint16_t MasterMyEvtQLock = 0;
	/*
		Takes a few ticks to process button event because
		of debounce code of Mac. So have this mechanism
		to prevent processing further events meanwhile.
	*/

 bool FindKeyEvent(int *VirtualKey, bool *KeyDown)
{
	MyEvtQEl *p;

	if (
		(0 == MasterMyEvtQLock) &&
		(nullptr != (p = MyEvtQOutP())))
	{
		if (MyEvtQElKindKey == p->kind) {
			*VirtualKey = p->u.press.key;
			*KeyDown = p->u.press.down;
			MyEvtQOutDone();
			return true;
		}
	}

	return false;
}

/* task management — forwarding stubs to g_ict */

#include "core/ict_scheduler.h"

void ICT_Zap(void)
{
	g_ict.zap();
}

iCountt GetCuriCount(void)
{
	return g_ict.getCurrent();
}

void ICT_add(int taskid, uint32_t n)
{
	g_ict.add(taskid, n);
}
