/*
	GLOBal GLUE (or GLOB of GLUE)

	Holds the program together.

	Some code here adapted from "custom.c" in vMac by Philip Cummins,
	in turn descended from code in the Un*x Amiga Emulator by
	Bernd Schmidt.
*/

#include "core/common.h"
#include "core/state_recorder.hpp"
#include "core/abnormal_ids.h"

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
#include "devices/keyboard.h"
#include "devices/rtc.h"
#include "devices/pmu.h"
#include "core/wire_bus.h"
#include "cpu/cpu.h"
#include "core/machine_obj.h"

#include <cstdio>
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

uint32_t g_diskIconAddr;

// Reset all emulated devices to their power-on state.
void customreset()
{
	if (auto* d = g_machine->findDevice<IWMDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCCDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCSIDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->reset();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->reset();
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->reset();
	extnReset();
	if (g_machine->config().isCompactMac()) {
		g_wantMacReset = true;
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

uint8_t * g_ram = nullptr;
uint8_t * g_vidROM = nullptr;
uint8_t * g_vidMem = nullptr;


extern void m68k_WantDisasmContext();

void dbglog_StartLine()
{
	m68k_WantDisasmContext();
	dbglog_writeCStr(" ");
}

void dbglog_WriteMemArrow(bool writeMem)
{
	if (writeMem) {
		dbglog_writeCStr(" <- ");
	} else {
		dbglog_writeCStr(" -> ");
	}
}

void dbglog_AddrAccess(char *s, uint32_t data,
	bool writeMem, uint32_t addr)
{
	if (g_LogEnd == 0 || g_InstructionCount < g_LogStart || g_InstructionCount >= g_LogEnd) { return; }
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_writeCStr("[");
	dbglog_writeHex(addr);
	dbglog_writeCStr("]");
	dbglog_WriteMemArrow(writeMem);
	dbglog_writeHex(data);
	dbglog_writeReturn();
}

void dbglog_Access(char *s, uint32_t data, bool writeMem)
{
	if (g_LogEnd == 0 || g_InstructionCount < g_LogStart || g_InstructionCount >= g_LogEnd) { return; }
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_WriteMemArrow(writeMem);
	dbglog_writeHex(data);
	dbglog_writeReturn();
}

void dbglog_WriteNote(char *s)
{
	if (g_LogEnd == 0 || g_InstructionCount < g_LogStart || g_InstructionCount >= g_LogEnd) { return; }
	dbglog_StartLine();
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}

void dbglog_WriteSetBool(char *s, bool v)
{
	if (g_LogEnd == 0 || g_InstructionCount < g_LogStart || g_InstructionCount >= g_LogEnd) { return; }
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

#if WantAbnormalReports
static bool s_gotOneAbnormal = false;
#endif

#ifndef ReportAbnormalInterrupt
#define ReportAbnormalInterrupt 0
#endif

#if WantAbnormalReports
void DoReportAbnormalID(uint16_t id
	, char *s
	)
{
	dbglog_StartLine();
	dbglog_writeCStr("*** abnormal : ");
	dbglog_writeCStr(s);
	dbglog_writeReturn();

	if (! s_gotOneAbnormal) {
		WarnMsgAbnormalID(id);
#if ReportAbnormalInterrupt
		SetInterruptButton(true);
#endif
		s_gotOneAbnormal = true;
	}
}
#endif

/* map of address space — addresses vary per model */

static constexpr uint32_t kRAM_Base = 0x00000000; /* when overlay off */

static uint32_t GetRAMLn2Spc() {
	auto m = g_machine->config().model;
	if (m == MacModel::PB100 || g_machine->config().isIIFamily())
		return 23;
	return 22;
}

static uint32_t GetVidMemBase() {
	if (g_machine->config().model == MacModel::PB100) return 0x00FA0000;
	return 0x00540000;
}
static uint32_t GetVidMemLn2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 18;
}

static uint32_t GetSCSIBlockBase() {
	if (g_machine->config().model == MacModel::PB100) return 0x00F90000;
	return 0x00580000;
}
static uint32_t GetSCSILn2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 19;
}

static constexpr uint32_t kRAM_Overlay_Base = 0x00600000; /* when overlay on */
#define kRAM_Overlay_Top  0x00800000

static uint32_t GetSCCRdBlockBase() {
	if (g_machine->config().model == MacModel::PB100) return 0x00FD0000;
	return 0x00800000;
}
static uint32_t GetSCCLn2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 22;
}

/* SCC write block: not present on PB100 (combined read/write) */
static constexpr uint32_t kSCCWr_Block_Base = 0x00A00000;
#define kSCCWr_Block_Top  0x00C00000

static uint32_t GetIWMBlockBase() {
	if (g_machine->config().model == MacModel::PB100) return 0x00F60000;
	return 0x00C00000;
}
static uint32_t GetIWMLn2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 21;
}

static uint32_t GetVIA1BlockBase() {
	if (g_machine->config().model == MacModel::PB100) return 0x00F70000;
	return 0x00E80000;
}
static uint32_t GetVIA1Ln2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 19;
}

/* ASC: on PB100, at a different address; other models use EmASC guard */
static uint32_t GetASCBlockBase() {
	if (g_machine->config().model == MacModel::PB100) return 0x00FB0000;
	return 0x50F00000; /* Mac II 32-bit ASC base */
}
static uint32_t GetASCLn2Spc() {
	if (g_machine->config().model == MacModel::PB100) return 16;
	return 26; /* Mac II ASC space */
}
static constexpr uint32_t kASC_Mask = 0x00000FFF;


static tMacErr PbufTransferVM(uint32_t buffera,
	PbufIndex i, uint32_t offset, uint32_t count, bool isWrite)
{
	uint32_t contig;
	uint8_t * buffer;

	while (count != 0) {
		buffer = get_real_address0(count, ! isWrite, buffera, &contig);
		if (0 == contig) {
			return tMacErr::miscErr;
		}
		PbufTransfer(buffer, i, offset, contig, isWrite);
		offset += contig;
		buffera += contig;
		count -= contig;
	}

	return tMacErr::noErr;
}

/* extension mechanism */

static constexpr int kCmndPbufFeatures = 1;
static constexpr int kCmndPbufNew = 2;
static constexpr int kCmndPbufDispose = 3;
static constexpr int kCmndPbufGetSize = 4;
static constexpr int kCmndPbufTransfer = 5;

/*
	Handle extension parameter-buffer commands from guest.
	Dispatches New, Dispose, GetSize, and Transfer operations
	on host-side parameter buffers.
*/
static void ExtnParamBuffers_Access(uint32_t p)
{
	tMacErr result = tMacErr::controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 1);
			result = tMacErr::noErr;
			break;
		case kCmndPbufFeatures:
			put_vm_long(p + ExtnDat_params + 0, 0);
			result = tMacErr::noErr;
			break;
		case kCmndPbufNew:
			{
				PbufIndex pbufNo;
				uint32_t count = get_vm_long(p + ExtnDat_params + 4);
				/* reserved word at offset 2, should be zero */
				result = PbufNew(count, &pbufNo);
				put_vm_word(p + ExtnDat_params + 0, pbufNo);
			}
			break;
		case kCmndPbufDispose:
			{
				PbufIndex pbufNo = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */
				result = CheckPbuf(pbufNo);
				if (tMacErr::noErr == result) {
					PbufDispose(pbufNo);
				}
			}
			break;
		case kCmndPbufGetSize:
			{
				uint32_t count;
				PbufIndex pbufNo = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */

				result = PbufGetSize(pbufNo, &count);
				if (tMacErr::noErr == result) {
					put_vm_long(p + ExtnDat_params + 4, count);
				}
			}
			break;
		case kCmndPbufTransfer:
			{
				uint32_t pbufCount;
				PbufIndex pbufNo = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */
				uint32_t offset = get_vm_long(p + ExtnDat_params + 4);
				uint32_t count = get_vm_long(p + ExtnDat_params + 8);
				uint32_t buffera = get_vm_long(p + ExtnDat_params + 12);
				bool isWrite =
					(get_vm_word(p + ExtnDat_params + 16) != 0);
				result = PbufGetSize(pbufNo, &pbufCount);
				if (tMacErr::noErr == result) {
					uint32_t endoff = offset + count;
					if ((endoff < offset) /* overflow */
						|| (endoff > pbufCount))
					{
						result = tMacErr::eofErr;
					} else {
						result = PbufTransferVM(buffera,
							pbufNo, offset, count, isWrite);
					}
				}
			}
			break;
	}

	put_vm_word(p + ExtnDat_result, static_cast<uint16_t>(result));
}

static constexpr int kCmndHTCEFeatures = 1;
static constexpr int kCmndHTCEExport = 2;
static constexpr int kCmndHTCEImport = 3;

static void ExtnHostTextClipExchange_Access(uint32_t p)
{
	tMacErr result = tMacErr::controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 1);
			result = tMacErr::noErr;
			break;
		case kCmndHTCEFeatures:
			put_vm_long(p + ExtnDat_params + 0, 0);
			result = tMacErr::noErr;
			break;
		case kCmndHTCEExport:
			{
				PbufIndex Pbuf_No = get_vm_word(p + ExtnDat_params + 0);

				result = CheckPbuf(Pbuf_No);
				if (tMacErr::noErr == result) {
					result = HTCEexport(Pbuf_No);
				}
			}
			break;
		case kCmndHTCEImport:
			{
				PbufIndex Pbuf_No;
				result = HTCEimport(&Pbuf_No);
				put_vm_word(p + ExtnDat_params + 0, Pbuf_No);
			}
			break;
	}

	put_vm_word(p + ExtnDat_result, static_cast<uint16_t>(result));
}

static constexpr uint32_t kFindExtnExtension = 0x64E1F58A;
static constexpr uint32_t kDiskDriverExtension = 0x4C9219E6;
static constexpr uint32_t kHostParamBuffersExtension = 0x314C87BF;
static constexpr uint32_t kHostClipExchangeExtension = 0x27B130CA;

static constexpr int kCmndFindExtnFind = 1;
static constexpr int kCmndFindExtnId2Code = 2;
static constexpr int kCmndFindExtnCount = 3;

static constexpr int kParamFindExtnTheExtn = 8;
static constexpr int kParamFindExtnTheId = 12;

/*
	Look up a g_rom extension by its four-byte signature.
	Returns the extension slot index to the guest.
*/
static void ExtnFind_Access(uint32_t p)
{
	tMacErr result = tMacErr::controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 1);
			result = tMacErr::noErr;
			break;
		case kCmndFindExtnFind:
			{
				uint32_t extn = get_vm_long(p + kParamFindExtnTheExtn);

				if (extn == kDiskDriverExtension) {
					put_vm_word(p + kParamFindExtnTheId, kExtnDisk);
					result = tMacErr::noErr;
				} else
				if (extn == kHostParamBuffersExtension) {
					put_vm_word(p + kParamFindExtnTheId,
						kExtnParamBuffers);
					result = tMacErr::noErr;
				} else
				if (extn == kHostClipExchangeExtension) {
					put_vm_word(p + kParamFindExtnTheId,
						kExtnHostTextClipExchange);
					result = tMacErr::noErr;
				} else
				if (extn == kFindExtnExtension) {
					put_vm_word(p + kParamFindExtnTheId,
						kExtnFindExtn);
					result = tMacErr::noErr;
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
					result = tMacErr::noErr;
				} else
				if (extn == kExtnParamBuffers) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kHostParamBuffersExtension);
					result = tMacErr::noErr;
				} else
				if (extn == kExtnHostTextClipExchange) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kHostClipExchangeExtension);
					result = tMacErr::noErr;
				} else
				if (extn == kExtnFindExtn) {
					put_vm_long(p + kParamFindExtnTheExtn,
						kFindExtnExtension);
					result = tMacErr::noErr;
				} else
				{
					/* not found */
				}
			}
			break;
		case kCmndFindExtnCount:
			{
				/* Report the number of extensions visible to the guest.
				   kExtnVideo is always in the enum for stable IDs,
				   but only counts when the model has a video card. */
				uint16_t n = kNumExtns;
				if (!g_machine->config().emVidCard) {
					--n;  /* don't count kExtnVideo (it's still
					         present in the enum for stable values) */
				}
				put_vm_word(p + kParamFindExtnTheId, n);
			}
			result = tMacErr::noErr;
			break;
	}

	put_vm_word(p + ExtnDat_result, static_cast<uint16_t>(result));
}

static constexpr int kDSK_Params_Hi = 0;
static constexpr int kDSK_Params_Lo = 1;
static constexpr int kDSK_QuitOnEject = 3; /* obsolete */

static uint16_t s_paramAddrHi;

/* --- New register block state (offsets 16–31, byte $20–$3F) --- */

static uint16_t s_regResult;
static uint32_t s_regParam[7];   /* p0–p6 */

/* Forward declaration — implemented in extn_clip.cpp */
#include "core/extn_clip.h"

static void regDispatch(uint16_t cmd)
{
	if (cmd >= 0x100 && cmd <= 0x1FF) {
		extnClipDispatch(cmd, s_regParam, s_regResult);
	} else {
		s_regResult = 0xFFFF; /* unimplemented */
	}
}

static uint32_t regBlockAccess(uint32_t data, bool writeMem, uint32_t regOff)
{
	if (regOff == 0) {
		/* command register — write triggers dispatch, read returns 0 */
		if (writeMem) {
			regDispatch(static_cast<uint16_t>(data));
		}
		return 0;
	}
	if (regOff == 1) {
		/* result register — read-only */
		return s_regResult;
	}
	/* regOff 2..15 → param words (p0 high, p0 low, p1 high, ...) */
	if (regOff >= 2 && regOff < 16) {
		uint32_t paramIdx = (regOff - 2) / 2;
		bool isLow = ((regOff - 2) % 2) != 0;
		if (writeMem) {
			if (isLow) {
				s_regParam[paramIdx] = (s_regParam[paramIdx] & 0xFFFF0000u)
					| (data & 0xFFFF);
			} else {
				s_regParam[paramIdx] = (s_regParam[paramIdx] & 0x0000FFFFu)
					| ((data & 0xFFFF) << 16);
			}
		}
		if (isLow) {
			return s_regParam[paramIdx] & 0xFFFF;
		} else {
			return (s_regParam[paramIdx] >> 16) & 0xFFFF;
		}
	}
	return 0;
}

/*
	Main extension dispatch.  Called when the guest writes the
	parameter-block address to the extension I/O ports (offsets 0–15),
	or accesses the new register block (offsets 16–31).
	Routes to the handler for the extension ID stored in
	the parameter block.
*/
static uint32_t extnAccess(uint32_t Data, bool writeMem, uint32_t addr)
{
	/* New register block: word offsets 16–31 */
	if (addr >= 16) {
		return regBlockAccess(Data, writeMem, addr - 16);
	}

	/* Legacy extension dispatch: word offsets 0–15 (write-only) */
	switch (addr) {
		case kDSK_Params_Hi:
			s_paramAddrHi = Data;
			break;
		case kDSK_Params_Lo:
			{
				uint32_t p = s_paramAddrHi << 16 | Data;

				s_paramAddrHi = (uint16_t) - 1;
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
						case kExtnParamBuffers:
							ExtnParamBuffers_Access(p);
							break;
						case kExtnHostTextClipExchange:
							ExtnHostTextClipExchange_Access(p);
							break;
						case kExtnDisk:
							if (auto* d = g_machine->findDevice<SonyDevice>()) d->extnDiskAccess(p);
							break;
						case kExtnSony:
							if (auto* d = g_machine->findDevice<SonyDevice>()) d->extnSonyAccess(p);
							break;
						default:
							put_vm_word(p + ExtnDat_result,
								static_cast<uint16_t>(tMacErr::controlErr));
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
	return Data;
}

/*
	ExtnDevice: wraps the g_rom extension slot mechanism (Sony, Video, etc.)
	as a Device for ATT dispatch.
*/
class ExtnDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override {
		return extnAccess(data, writeMem, addr);
	}
	void zap() override {}
	void reset() override {}
	const char* name() const override { return "Extn"; }
};
static ExtnDevice g_extnDevice;

void extnReset()
{
	s_paramAddrHi = (uint16_t) - 1;
}

/* implementation of read/write for everything but RAM and ROM */

static constexpr int kSCC_Mask = 0x03;

static constexpr uint32_t kVIA1_Mask = 0x00000F;
static constexpr uint32_t kVIA2_Mask = 0x00000F;

static constexpr uint32_t kIWM_Mask = 0x00000F; /* Allocated Memory Bandwidth for IWM */

static uint32_t GetROMCmpZeroMask() {
	const auto& cfg = g_machine->config();
	auto m = cfg.model;
	if (m <= MacModel::Mac512Ke) return 0;
	if (m <= MacModel::Plus) {
		return (cfg.romSize > 0x00020000) ? 0 : 0x00020000;
	}
	/* SE, Classic, PB100, II, IIx all use 0 */
	return 0;
}

#define kROM_cmpmask (0x00F00000 | GetROMCmpZeroMask())

static uint32_t GetOverlayROMCmpZeroMask() {
	auto m = g_machine->config().model;
	if (m <= MacModel::Mac512Ke) return 0x00100000;
	if (m <= MacModel::Plus)     return 0x00020000;
	if (m <= MacModel::Classic)   return 0x00300000;
	/* PB100, II, IIx */
	return 0;
}

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

static const char* mmdv_name(uint8_t mmdv) {
	switch (mmdv) {
		case kMMDV_VIA1: return "VIA1";
		case kMMDV_VIA2: return "VIA2";
		case kMMDV_SCC:  return "SCC";
		case kMMDV_Extn: return "EXTN";
		case kMMDV_ASC:  return "ASC";
		case kMMDV_SCSI: return "SCSI";
		case kMMDV_IWM:  return "IWM";
		default:         return "???";
	}
}

enum {
	kMAN_OverlayOff, /* present on SE and later */

	kNumMANs
};


/* Max ATT entries — generous fixed size, checked at runtime */
static constexpr int kATTListMax = 64;
static ATTer ATTListA[kATTListMax];
static uint16_t s_lastATTel;


// Append an entry to the address translation table.
static void AddToATTList(ATTep p)
{
	uint16_t NewLast = s_lastATTel + 1;
	if (NewLast >= kATTListMax) {
		ReportAbnormalID(AbnormalID::kMACH_ATT_list_not_big_enough, "ATT list not big enough");
	} else {
		ATTListA[s_lastATTel] = *p;
		s_lastATTel = NewLast;
	}
}

static void InitATTList()
{
	s_lastATTel = 0;
}

static void FinishATTList()
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
		uint16_t i = s_lastATTel;
		ATTep p = &ATTListA[s_lastATTel];
		ATTep h = nullptr;

		while (0 != i) {
			--i;
			--p;
			p->Next = h;
			h = p;
		}


		g_cpu.setHeadATTel(h);
	}
}

/* Mac II/IIx: RAM24 setup with VIA2 bank select */
static void SetUp_RAM24()
{
	const auto& cfg = g_machine->config();
	ATTer r{};
	uint32_t bankbit = 0x00100000 << (((VIA2_iA7 << 1) | VIA2_iA6) << 1);

	if (cfg.ramASize == cfg.ramBSize) {
		if (cfg.ramASize == bankbit) {
			/* properly set up balanced RAM */
			r.cmpmask = 0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1);
			r.cmpvalu = 0;
			r.usemask = ((1 << GetRAMLn2Spc()) - 1) & (cfg.ramSize() - 1);
			r.usebase = g_ram;
			r.Access = kATTA_readwritereadymask;
			AddToATTList(&r);
		} else
		{
			bankbit &= 0x00FFFFFF; /* if too large, always use RAMa */

			if (0 != bankbit) {
				if (cfg.ramBSize != 0) {
					r.cmpmask = bankbit
						| (0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1));
					r.cmpvalu = bankbit;
					r.usemask = ((1 << GetRAMLn2Spc()) - 1) & (cfg.ramBSize - 1);
					r.usebase = cfg.ramASize + g_ram;
					r.Access = kATTA_readwritereadymask;
					AddToATTList(&r);
				}
			}

			{
				r.cmpmask = bankbit
					| (0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1));
				r.cmpvalu = 0;
				r.usemask = ((1 << GetRAMLn2Spc()) - 1) & (cfg.ramASize - 1);
				r.usebase = g_ram;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}
		}
	} else {
		bankbit &= 0x00FFFFFF;

		if (0 != bankbit) {
			if (cfg.ramBSize != 0) {
				r.cmpmask = bankbit
					| (0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1));
				r.cmpvalu = bankbit;
				r.usemask = ((1 << GetRAMLn2Spc()) - 1) & (cfg.ramBSize - 1);
				r.usebase = cfg.ramASize + g_ram;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}
		}

		{
			r.cmpmask = bankbit
				| (0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1));
			r.cmpvalu = 0;
			r.usemask = ((1 << GetRAMLn2Spc()) - 1) & (cfg.ramASize - 1);
			r.usebase = g_ram;
			r.Access = kATTA_readwritereadymask;
			AddToATTList(&r);
		}
	}
}

/* Mac II/IIx I/O address space */
namespace io32 {
	constexpr uint32_t kBase  = 0x50000000;
	constexpr uint32_t kMask  = 0xFF01E000;
	constexpr uint32_t kVIA1  = 0x00000;
	constexpr uint32_t kVIA2  = 0x02000;
	constexpr uint32_t kSCC   = 0x04000;
	constexpr uint32_t kExtn  = 0x0C000;
	constexpr uint32_t kSCSI  = 0x10000;
	constexpr uint32_t kASC   = 0x14000;
	constexpr uint32_t kIWM   = 0x16000;
}
namespace io24 {
	constexpr uint32_t kBase  = 0x00F00000;
	constexpr uint32_t kMask  = 0x00F1E000;
}

/* Mac II/IIx: I/O space setup */
static void SetUp_io()
{
	ATTer r{};

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kVIA1;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kVIA1;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_VIA1;
	r.device = g_machine->findDevice<VIA1Device>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kVIA2;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kVIA2;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_VIA2;
	r.device = g_machine->findDevice<VIA2Device>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kSCC;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kSCC;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCC;
	r.device = g_machine->findDevice<SCCDevice>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kExtn;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kExtn;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_Extn;
	r.device = &g_extnDevice;
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kSCSI;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kSCSI;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCSI;
	r.device = g_machine->findDevice<SCSIDevice>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kASC;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kASC;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_ASC;
	r.device = g_machine->findDevice<ASCDevice>();
	AddToATTList(&r);

	if (Addr32) {
		r.cmpmask = io32::kMask;
		r.cmpvalu = io32::kBase | io32::kIWM;
	} else {
		r.cmpmask = io24::kMask;
		r.cmpvalu = io24::kBase | io32::kIWM;
	}
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_IWM;
	r.device = g_machine->findDevice<IWMDevice>();
	AddToATTList(&r);

}

/* Mac II/IIx: 24-bit address space setup */
static void SetUp_address24()
{
	const auto& cfg = g_machine->config();
	ATTer r{};


	if (MemOverlay) {
		r.cmpmask = GetOverlayROMCmpZeroMask() |
			(0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1));
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.romSize - 1;
		r.usebase = g_rom;
		r.Access = kATTA_readreadymask;
		AddToATTList(&r);
	} else {
		SetUp_RAM24();
	}

	r.cmpmask = kROM_cmpmask;
	r.cmpvalu = cfg.romBase;
	r.usemask = cfg.romSize - 1;
	r.usebase = g_rom;
	r.Access = kATTA_readreadymask;
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
	r.cmpvalu = 0x900000;
	r.usemask = (cfg.vidMemSize - 1) & (0x100000 - 1);
	r.usebase = g_vidMem;
	r.Access = kATTA_readwritereadymask;
	AddToATTList(&r);
	if (cfg.vidMemSize >= 0x00200000) {
		r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
		r.cmpvalu = 0xA00000;
		r.usemask = (0x100000 - 1);
		r.usebase = g_vidMem + (1 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}
	if (cfg.vidMemSize >= 0x00400000) {
		r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
		r.cmpvalu = 0xB00000;
		r.usemask = (0x100000 - 1);
		r.usebase = g_vidMem + (2 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
		r.cmpmask = 0x00FFFFFF & ~ (0x100000 - 1);
		r.cmpvalu = 0xC00000;
		r.usemask = (0x100000 - 1);
		r.usebase = g_vidMem + (3 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}

	SetUp_io();
}

/* Mac II/IIx: 32-bit address space setup */
static void SetUp_address32()
{
	const auto& cfg = g_machine->config();
	ATTer r{};

	if (MemOverlay) {
		r.cmpmask = ~ ((1 << 30) - 1);
		r.cmpvalu = 0;
		r.usemask = cfg.romSize - 1;
		r.usebase = g_rom;
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
				r.usebase = g_ram;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			} else
			{
				if (cfg.ramBSize != 0) {
					r.cmpmask = bankbit | ~ ((1 << 30) - 1);
					r.cmpvalu = bankbit;
					r.usemask = cfg.ramBSize - 1;
					r.usebase = cfg.ramASize + g_ram;
					r.Access = kATTA_readwritereadymask;
					AddToATTList(&r);
				}

				r.cmpmask = bankbit | ~ ((1 << 30) - 1);
				r.cmpvalu = 0;
				r.usemask = cfg.ramASize - 1;
				r.usebase = g_ram;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}
		} else {
			if (cfg.ramBSize != 0) {
				r.cmpmask = bankbit | ~ ((1 << 30) - 1);
				r.cmpvalu = bankbit;
				r.usemask = cfg.ramBSize - 1;
				r.usebase = cfg.ramASize + g_ram;
				r.Access = kATTA_readwritereadymask;
				AddToATTList(&r);
			}

			r.cmpmask = bankbit | ~ ((1 << 30) - 1);
			r.cmpvalu = 0;
			r.usemask = cfg.ramASize - 1;
			r.usebase = g_ram;
			r.Access = kATTA_readwritereadymask;
			AddToATTList(&r);
		}
	}

	r.cmpmask = ~ ((1 << 28) - 1);
	r.cmpvalu = 0x40000000;
	r.usemask = cfg.romSize - 1;
	r.usebase = g_rom;
	r.Access = kATTA_readreadymask;
	AddToATTList(&r);


	/* Standard NuBus space */
	r.cmpmask = ~ ((1 << 20) - 1);
	r.cmpvalu = 0xF9F00000;
	r.usemask = cfg.vidROMSize - 1;
	r.usebase = g_vidROM;
	r.Access = kATTA_readreadymask;
	AddToATTList(&r);

	r.cmpmask = ~ 0x000FFFFF;
	r.cmpvalu = 0xF9900000;
	r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
	r.usebase = g_vidMem;
	r.Access = kATTA_readwritereadymask;
	AddToATTList(&r);
/* kludge to allow more than 1M of Video Memory */
	if (cfg.vidMemSize >= 0x00200000) {
		r.cmpmask = ~ 0x000FFFFF;
		r.cmpvalu = 0xF9A00000;
		r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
		r.usebase = g_vidMem + (1 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}
	if (cfg.vidMemSize >= 0x00400000) {
		r.cmpmask = ~ 0x000FFFFF;
		r.cmpvalu = 0xF9B00000;
		r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
		r.usebase = g_vidMem + (2 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
		r.cmpmask = ~ 0x000FFFFF;
		r.cmpvalu = 0xF9C00000;
		r.usemask = 0x000FFFFF & (cfg.vidMemSize - 1);
		r.usebase = g_vidMem + (3 << 20);
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}

	SetUp_io();

}

/* Mac II/IIx: address space dispatcher */
static void SetUp_address_II()
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
static void SetUp_RAM24_compact()
{
	const auto& cfg = g_machine->config();
	ATTer r{};

	if (cfg.ramBSize == 0 || cfg.ramASize == cfg.ramBSize) {
		r.cmpmask = 0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1);
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.ramSize() - 1;
		r.usebase = g_ram;
		r.Access = kATTA_readwritereadymask;
		AddToATTListWithMTB(&r);
	} else {
		/* unbalanced memory */

		if (0 != (0x00FFFFFF & cfg.ramASize)) {
			/* condition should always be true if configuration file right */
			r.cmpmask = 0x00FFFFFF & (cfg.ramASize | ~ ((1 << GetRAMLn2Spc()) - 1));
			r.cmpvalu = kRAM_Base + cfg.ramASize;
			r.usemask = cfg.ramBSize - 1;
			r.usebase = cfg.ramASize + g_ram;
			r.Access = kATTA_readwritereadymask;
			AddToATTListWithMTB(&r);
		}

		r.cmpmask = 0x00FFFFFF & (cfg.ramASize | ~ ((1 << GetRAMLn2Spc()) - 1));
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.ramASize - 1;
		r.usebase = g_ram;
		r.Access = kATTA_readwritereadymask;
		AddToATTListWithMTB(&r);
	}
}

/* Compact Mac: 24-bit address space setup */
static void SetUp_address_compact()
{
	const auto& cfg = g_machine->config();
	ATTer r{};

	if (MemOverlay) {
		r.cmpmask = GetOverlayROMCmpZeroMask() |
			(0x00FFFFFF & ~ ((1 << GetRAMLn2Spc()) - 1));
		r.cmpvalu = kRAM_Base;
		r.usemask = cfg.romSize - 1;
		r.usebase = g_rom;
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
		r.usebase = g_rom;
		r.Access = kATTA_readreadymask;
		AddToATTListWithMTB(&r);
	}

	if (MemOverlay) {
		r.cmpmask = 0x00E00000;
		r.cmpvalu = kRAM_Overlay_Base;
		if (cfg.ramBSize == 0 || cfg.ramASize == cfg.ramBSize) {
			r.usemask = cfg.ramSize() - 1;
				/* note that cmpmask and usemask overlap for 4M */
			r.usebase = g_ram;
			r.Access = kATTA_readwritereadymask;
		} else {
			/* unbalanced memory */
			r.usemask = cfg.ramBSize - 1;
			r.usebase = cfg.ramASize + g_ram;
			r.Access = kATTA_readwritereadymask;
		}
		AddToATTListWithMTB(&r);
	}

	if (g_machine->config().includeVidMem) {
		r.cmpmask = 0x00FFFFFF & ~ ((1 << GetVidMemLn2Spc()) - 1);
		r.cmpvalu = GetVidMemBase();
		r.usemask = g_machine->config().vidMemSize - 1;
		r.usebase = g_vidMem;
		r.Access = kATTA_readwritereadymask;
		AddToATTList(&r);
	}

	r.cmpmask = 0x00FFFFFF & ~ ((1 << GetVIA1Ln2Spc()) - 1);
	r.cmpvalu = GetVIA1BlockBase();
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_VIA1;
	r.device = g_machine->findDevice<VIA1Device>();
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ ((1 << GetSCCLn2Spc()) - 1);
	r.cmpvalu = GetSCCRdBlockBase();
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
		r.cmpmask = 0x00FFFFFF & ~ ((1 << GetASCLn2Spc()) - 1);
		r.cmpvalu = GetASCBlockBase();
		r.usebase = nullptr;
		r.Access = kATTA_mmdvmask;
		r.MMDV = kMMDV_ASC;
		r.device = g_machine->findDevice<ASCDevice>();
		AddToATTList(&r);
	}

	r.cmpmask = 0x00FFFFFF & ~ ((1 << GetSCSILn2Spc()) - 1);
	r.cmpvalu = GetSCSIBlockBase();
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_SCSI;
	r.device = g_machine->findDevice<SCSIDevice>();
	AddToATTList(&r);

	r.cmpmask = 0x00FFFFFF & ~ ((1 << GetIWMLn2Spc()) - 1);
	r.cmpvalu = GetIWMBlockBase();
	r.usebase = nullptr;
	r.Access = kATTA_mmdvmask;
	r.MMDV = kMMDV_IWM;
	r.device = g_machine->findDevice<IWMDevice>();
	AddToATTList(&r);
}

/* Unified address space setup — dispatches by model family */
static void SetUp_address()
{
	if (g_machine->config().isIIFamily()) {
		SetUp_address_II();
	} else {
		SetUp_address_compact();
	}
}

static void SetUpMemBanks()
{
	InitATTList();

	SetUp_address();

	FinishATTList();
}


/*
	Dispatch a memory-mapped device access.  Routes the
	read/write to the correct device handler based on
	the MMDV tag in the ATT entry.
*/
 uint32_t MMDV_Access(ATTep p, uint32_t data,
	bool writeMem, bool byteSize, uint32_t addr)
{
	uint32_t origData = data;
	switch (p->MMDV) {
		case kMMDV_VIA1:
			if (! byteSize) {
				if (g_machine->config().isIIFamily()
					&& writeMem && (addr == 0xF40006))
				{
					/* for weirdness on shutdown in System 6 */
				} else
				{
					ReportAbnormalID(AbnormalID::kMACH_VIA1_word, "access VIA1 word");
				}
			} else if ((addr & 1) != 0) {
				ReportAbnormalID(AbnormalID::kMACH_VIA1_odd, "access VIA1 odd");
			} else {
				if (g_machine->config().model != MacModel::PB100) {
					bool nonStandard;
					if (g_machine->config().isIIFamily()) {
						nonStandard = (addr & 0x000001FE) != 0x00000000;
					} else {
						nonStandard = (addr & 0x000FE1FE) != 0x000FE1FE;
					}
					if (nonStandard) {
						ReportAbnormalID(AbnormalID::kMACH_VIA1_nonstandard_address,
							"access VIA1 nonstandard address");
					}
				}
				data = p->device->access(data, writeMem,
					(addr >> 9) & kVIA1_Mask);
			}

			break;
		case kMMDV_VIA2:
			if (! byteSize) {
				if ((! writeMem)
					&& ((0x3e00 == (addr & 0x1FFFF))
						|| (0x3e02 == (addr & 0x1FFFF))))
				{
					/* for weirdness at offset 0x71E in ROM */
					data =
						(p->device->access(data, writeMem,
							(addr >> 9) & kVIA2_Mask) << 8)
						| p->device->access(data, writeMem,
							(addr >> 9) & kVIA2_Mask);

				} else {
					ReportAbnormalID(AbnormalID::kMACH_VIA2_word, "access VIA2 word");
				}
			} else if ((addr & 1) != 0) {
				if (0x3FFF == (addr & 0x1FFFF)) {
					/*
						for weirdness at offset 0x7C4 in g_rom.
						looks like bug.
					*/
					data = p->device->access(data, writeMem,
						(addr >> 9) & kVIA2_Mask);
				} else {
					ReportAbnormalID(AbnormalID::kMACH_VIA2_odd, "access VIA2 odd");
				}
			} else {
				if ((addr & 0x000001FE) != 0x00000000) {
					ReportAbnormalID(AbnormalID::kMACH_VIA2_nonstandard_address,
						"access VIA2 nonstandard address");
				}
				data = p->device->access(data, writeMem,
					(addr >> 9) & kVIA2_Mask);
			}
			break;
		case kMMDV_SCC:

			if (g_machine->config().isSEFamily()) {
				/* SE/Classic only: check for unassigned SCC address */
				if ((addr & 0x00100000) == 0) {
					ReportAbnormalID(AbnormalID::kMACH_SCC_unassigned_address,
						"access SCC unassigned address");
					break;
				}
			}
			if (! byteSize) {
				ReportAbnormalID(AbnormalID::kMACH_Attemped_Phase_Adjust, "Attemped Phase Adjust");
			} else
			if (!g_machine->config().isIIFamily()
				&& writeMem != ((addr & 1) != 0))
			{
				if (writeMem) {
					auto m = g_machine->config().model;
					if (m >= MacModel::Mac512Ke
						&& m != MacModel::PB100)
					{
						ReportAbnormalID(AbnormalID::kMACH_SCC_even_odd, "access SCC even/odd");
					}
				} else {
					if (auto* d = g_machine->findDevice<SCCDevice>()) d->reset();
				}
			} else
			if (g_machine->config().model != MacModel::PB100
				&& !g_machine->config().isIIFamily()
				&& writeMem != (addr >= kSCCWr_Block_Base))
			{
				ReportAbnormalID(AbnormalID::kMACH_SCC_wr_rd_base_wrong, "access SCC wr/rd base wrong");
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
						ReportAbnormalID(AbnormalID::kMACH_SCC_nonstandard_address,
							"access SCC nonstandard address");
					}
				}
				data = p->device->access(data, writeMem,
					(addr >> 1) & kSCC_Mask);
			}
			break;
		case kMMDV_Extn:
			if (byteSize) {
				ReportAbnormalID(AbnormalID::kMACH_Sony_byte, "access Sony byte");
			} else if ((addr & 1) != 0) {
				ReportAbnormalID(AbnormalID::kMACH_Sony_odd, "access Sony odd");
			} else {
				uint32_t wordOff = (addr >> 1) & 0x1F;
				if (!writeMem && wordOff < 16) {
					ReportAbnormalID(AbnormalID::kMACH_Sony_read,
						"access Sony read");
				} else {
					data = p->device->access(data, writeMem, wordOff);
				}
			}
			break;
		case kMMDV_ASC:
			if (! byteSize) {
				if (g_machine->config().isIIFamily()) {
					if (writeMem) {
						(void) p->device->access((data >> 8) & 0x00FF,
							writeMem, addr & kASC_Mask);
						data = p->device->access((data) & 0x00FF,
							writeMem, (addr + 1) & kASC_Mask);
					} else {
						data =
							(p->device->access((data >> 8) & 0x00FF,
								writeMem, addr & kASC_Mask) << 8)
							| p->device->access((data) & 0x00FF,
								writeMem, (addr + 1) & kASC_Mask);
					}
				} else {
					ReportAbnormalID(AbnormalID::kMACH_ASC_word, "access ASC word");
				}
			} else {
				data = p->device->access(data, writeMem, addr & kASC_Mask);
			}
			break;
		case kMMDV_SCSI:
			if (! byteSize) {
				ReportAbnormalID(AbnormalID::kMACH_SCSI_word, "access SCSI word");
			} else
			if (!g_machine->config().isIIFamily()
				&& writeMem != ((addr & 1) != 0))
			{
				ReportAbnormalID(AbnormalID::kMACH_SCSI_even_odd, "access SCSI even/odd");
			} else
			{
				if (g_machine->config().isIIFamily()) {
					if ((addr & 0x1F8F) != 0x00000000) {
						ReportAbnormalID(AbnormalID::kMACH_SCSI_nonstandard_address,
							"access SCSI nonstandard address");
					}
				}
				data = p->device->access(data, writeMem, (addr >> 4) & 0x07);
			}

			break;
		case kMMDV_IWM:
			if (g_machine->config().isSEFamily()) {
				if ((addr & 0x00100000) == 0) {
					ReportAbnormalID(AbnormalID::kMACH_IWM_unassigned_address,
						"access IWM unassigned address");
					break;
				}
			}
			if (! byteSize) {
			} else if (g_machine->config().isIIFamily()) {
				if ((addr & 1) != 0) {
					ReportAbnormalID(AbnormalID::kMACH_IWM_odd, "access IWM odd");
				} else {
					data = p->device->access(data, writeMem,
						(addr >> 9) & kIWM_Mask);
				}
			} else {
				if ((addr & 1) == 0) {
					ReportAbnormalID(AbnormalID::kMACH_IWM_even, "access IWM even");
				} else {
					if (g_machine->config().model != MacModel::PB100
						&& !g_machine->config().isIIFamily())
					{
						if ((addr & 0x001FE1FF) != 0x001FE1FF) {
							ReportAbnormalID(AbnormalID::kMACH_IWM_nonstandard_address,
								"access IWM nonstandard address");
						}
					}
					data = p->device->access(data, writeMem,
						(addr >> 9) & kIWM_Mask);
				}
			}

			break;
	}

	if (g_LogEnd > 0 && g_InstructionCount >= g_LogStart && g_InstructionCount < g_LogEnd) {
		if (writeMem) {
			fprintf(stderr, "%u IOW %s %08X %02X\n", (unsigned)g_InstructionCount, mmdv_name(p->MMDV), addr, origData & 0xFF);
		} else {
			fprintf(stderr, "%u IOR %s %08X %02X\n", (unsigned)g_InstructionCount, mmdv_name(p->MMDV), addr, data & 0xFF);
		}
	}

	/* StateRecorder I/O hook */
	if (g_recorder.active()) {
		g_recorder.io(g_InstructionCount, addr,
			writeMem ? origData : data,
			writeMem, byteSize, mmdv_name(p->MMDV));
	}

	return data;
}

 bool MemAccessNtfy(ATTep pT)
{
	bool v = false;

	switch (pT->Ntfy) {
		case kMAN_OverlayOff:
			if (g_machine->config().model
				>= MacModel::SE)
			{
				pT->Access = kATTA_readreadymask;

				g_wires.set(Wire_MemOverlay, 0);

				v = true;
			}

			break;
	}

	return v;
}

void MemOverlay_ChangeNtfy()
{
	/* All models rebuild memory banks when overlay changes */
	SetUpMemBanks();
}

void Addr32_ChangeNtfy()
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

bool g_interruptButton = false;

void SetInterruptButton(bool v)
{
	if (g_interruptButton != v) {
		g_interruptButton = v;
		VIAorSCCinterruptChngNtfy();
	}
}

static uint8_t s_curIPL = 0;

/*
	Recalculate the CPU interrupt priority level from
	VIA1, VIA2, SCC, and NMI button state.
	Mac II uses a 3-bit priority scheme; compact Macs
	use a simpler two-source encoding.
*/
void VIAorSCCinterruptChngNtfy()
{
	uint8_t NewIPL;

	if (g_machine->config().isIIFamily()) {
		/* Mac II priority: NMI > SCC > VIA2 > VIA1 */
		if (g_interruptButton) {
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
			| (g_interruptButton << 2);
	}
	if (NewIPL != s_curIPL) {
		s_curIPL = NewIPL;
		g_cpu.iplChangeNotify();
	}
}

 bool AddrSpac_Init()
{
	g_wires.init(kNumWires);

	/* Register wire change callbacks */
	g_wires.onChange(Wire_MemOverlay, MemOverlay_ChangeNtfy);

	/* Wire_MemOverlay is a separate wire from Wire_VIA1_iA4 so that
	   models like Classic (SCSIOvIRQ) and PB100 (PMU bus bit 4) can
	   use VIA1 Port A bit 4 for something other than the overlay.

	   For all other models (Plus, 512Ke, 128K, SE, SEFDHD, MacII,
	   MacIIx) the two must stay in bi-directional sync — just as they
	   were when they shared the same wire index.  No recursion guard
	   is needed because WireBus::set() only fires callbacks on actual
	   value changes. */
	{
		auto m = g_machine->config().model;
		bool needsSync = (m != MacModel::Classic && m != MacModel::PB100);
		if (needsSync) {
			g_wires.onChange(Wire_VIA1_iA4, [](){
				g_wires.set(Wire_MemOverlay, g_wires.get(Wire_VIA1_iA4));
			});
			g_wires.onChange(Wire_MemOverlay, [](){
				g_wires.set(Wire_VIA1_iA4, g_wires.get(Wire_MemOverlay));
			});
		}
	}

	if (g_machine->config().isIIFamily()) {
		extern void PowerOff_ChangeNtfy();
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
	if (g_machine->config().emClassicKbrd) {
		g_wires.onChange(Wire_VIA1_iCB2, [](){ if (auto* d = g_machine->findDevice<KeyboardDevice>()) d->dataLineChngNtfy(); });
	}
	if (g_machine->config().emPMU) {
		g_wires.onChange(Wire_PMU_ToReady, [](){ if (auto* d = g_machine->findDevice<PMUDevice>()) d->toReadyChangeNtfy(); });
	}

	g_cpu.init(
		&s_curIPL, &g_machine->config());
	return true;
}

void memoryReset()
{
	g_wires.set(Wire_MemOverlay, 1);
	SetUpMemBanks();
}

/* PowerOff_ChangeNtfy: only wired on Mac II/IIx (see AddrSpac_Init) */
extern void PowerOff_ChangeNtfy();
void PowerOff_ChangeNtfy()
{
	if (! VIA2_iB2) {
		g_forceMacOff = true;
	}
}

/* user event queue utilities */

uint16_t MasterEvtQLock = 0;
	/*
		Takes a few ticks to process button event because
		of debounce code of Mac. So have this mechanism
		to prevent processing further events meanwhile.
	*/

 bool FindKeyEvent(int *VirtualKey, bool *KeyDown)
{
	EvtQEl *p;

	if (
		(0 == MasterEvtQLock) &&
		(nullptr != (p = EvtQOutP())))
	{
		if (EvtQElKind::Key == p->kind) {
			*VirtualKey = p->u.press.key;
			*KeyDown = p->u.press.down;
			EvtQOutDone();
			return true;
		}
	}

	return false;
}

/* ICT scheduler now accessed directly via g_ict (core/ict_scheduler.h) */
