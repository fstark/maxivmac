/*
	Motorola 68K Instructions TABle
*/

#include "core/common.h"
#include "core/machine_config.h"

#include "cpu/m68k_tables.h"

struct WorkR {
	/* expected size : 8 bytes */
	uint32_t opcode;
	uint32_t opsize;
	uint16_t MainClass;
	uint16_t Cycles;
	DecOpR DecOp;
};

#define b76(p) ((p->opcode >> 6) & 3)
#define b8(p) ((p->opcode >> 8) & 1)
#define mode(p) ((p->opcode >> 3) & 7)
#define reg(p) (p->opcode & 7)
#define md6(p) ((p->opcode >> 6) & 7)
#define rg9(p) ((p->opcode >> 9) & 7)

enum {
	kAddrValidAny,
	kAddrValidData,
	kAddrValidDataAlt,
	kAddrValidControl,
	kAddrValidControlAlt,
	kAddrValidAltMem,
	kAddrValidDataNoCn, /* no constants (immediate data) */

	kNumAddrValids
};

#define kAddrValidMaskAny        (1 << kAddrValidAny)
#define kAddrValidMaskData       (1 << kAddrValidData)
#define kAddrValidMaskDataAlt    (1 << kAddrValidDataAlt)
#define kAddrValidMaskControl    (1 << kAddrValidControl)
#define kAddrValidMaskControlAlt (1 << kAddrValidControlAlt)
#define kAddrValidMaskAltMem     (1 << kAddrValidAltMem)
#define kAddrValidMaskDataNoCn   (1 << kAddrValidDataNoCn)

#define CheckInSet(v, m) (0 != ((1 << (v)) & (m)))

#define kMyAvgCycPerInstr (10 * kCycleScale + (40 * kCycleScale / 64))

// Map operation size (1/2/4) to the register addressing mode.
static uint8_t GetAMdRegSz(WorkR *p)
{
	uint8_t CurAMd;

	switch (p->opsize) {
		case 1:
			CurAMd = kAMdRegB;
			break;
		case 2:
		default: /* keep compiler happy */
			CurAMd = kAMdRegW;
			break;
		case 4:
			CurAMd = kAMdRegL;
			break;
	}

	return CurAMd;
}

// Map operation size (1/2/4) to the indirect addressing mode.
static uint8_t GetAMdIndirectSz(WorkR *p)
{
	uint8_t CurAMd;

	switch (p->opsize) {
		case 1:
			CurAMd = kAMdIndirectB;
			break;
		case 2:
		default: /* keep compiler happy */
			CurAMd = kAMdIndirectW;
			break;
		case 4:
			CurAMd = kAMdIndirectL;
			break;
	}

	return CurAMd;
}

/* Calculate the read cycle penalty for the given effective
   address mode and register combination. */
static uint16_t OpEACalcCyc(WorkR *p, uint8_t m, uint8_t r)
{
	uint16_t v;

	switch (m) {
		case 0:
		case 1:
			v = 0;
			break;
		case 2:
			v = ((4 == p->opsize)
				? (8 * kCycleScale + 2 * RdAvgXtraCyc)
				: (4 * kCycleScale + RdAvgXtraCyc));
			break;
		case 3:
			v = ((4 == p->opsize)
				? (8 * kCycleScale + 2 * RdAvgXtraCyc)
				: (4 * kCycleScale + RdAvgXtraCyc));
			break;
		case 4:
			v = ((4 == p->opsize)
				? (10 * kCycleScale + 2 * RdAvgXtraCyc)
				: (6 * kCycleScale + RdAvgXtraCyc));
			break;
		case 5:
			v = ((4 == p->opsize)
				? (12 * kCycleScale + 3 * RdAvgXtraCyc)
				: (8 * kCycleScale + 2 * RdAvgXtraCyc));
			break;
		case 6:
			v = ((4 == p->opsize)
				? (14 * kCycleScale + 3 * RdAvgXtraCyc)
				: (10 * kCycleScale + 2 * RdAvgXtraCyc));
			break;
		case 7:
			switch (r) {
				case 0:
					v = ((4 == p->opsize)
						? (12 * kCycleScale + 3 * RdAvgXtraCyc)
						: (8 * kCycleScale + 2 * RdAvgXtraCyc));
					break;
				case 1:
					v = ((4 == p->opsize)
						? (16 * kCycleScale + 4 * RdAvgXtraCyc)
						: (12 * kCycleScale + 3 * RdAvgXtraCyc));
					break;
				case 2:
					v = ((4 == p->opsize)
						? (12 * kCycleScale + 3 * RdAvgXtraCyc)
						: (8 * kCycleScale + 2 * RdAvgXtraCyc));
					break;
				case 3:
					v = ((4 == p->opsize)
						? (14 * kCycleScale + 3 * RdAvgXtraCyc)
						: (10 * kCycleScale + 2 * RdAvgXtraCyc));
					break;
				case 4:
					v = ((4 == p->opsize)
						? (8 * kCycleScale + 2 * RdAvgXtraCyc)
						: (4 * kCycleScale + RdAvgXtraCyc));
					break;
				default:
					v = 0;
					break;
			}
			break;
		default: /* keep compiler happy */
			v = 0;
			break;
	}

	return v;
}

static uint16_t OpEADestCalcCyc(WorkR *p, uint8_t m, uint8_t r)
{
	uint16_t v;

	switch (m) {
		case 0:
		case 1:
			v = 0;
			break;
		case 2:
			v = ((4 == p->opsize)
				? (8 * kCycleScale + 2 * WrAvgXtraCyc)
				: (4 * kCycleScale + WrAvgXtraCyc));
			break;
		case 3:
			v = ((4 == p->opsize)
				? (8 * kCycleScale + 2 * WrAvgXtraCyc)
				: (4 * kCycleScale + WrAvgXtraCyc));
			break;
		case 4:
			v = ((4 == p->opsize)
				? (8 * kCycleScale + 2 * WrAvgXtraCyc)
				: (4 * kCycleScale + WrAvgXtraCyc));
			break;
		case 5:
			v = ((4 == p->opsize)
				? (12 * kCycleScale + RdAvgXtraCyc + 2 * WrAvgXtraCyc)
				: (8 * kCycleScale + RdAvgXtraCyc + WrAvgXtraCyc));
			break;
		case 6:
			v = ((4 == p->opsize)
				? (14 * kCycleScale + RdAvgXtraCyc + 2 * WrAvgXtraCyc)
				: (10 * kCycleScale + RdAvgXtraCyc + WrAvgXtraCyc));
			break;
		case 7:
			switch (r) {
				case 0:
					v = ((4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc));
					break;
				case 1:
					v = ((4 == p->opsize)
						? (16 * kCycleScale
							+ 2 * RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (12 * kCycleScale
							+ 2 * RdAvgXtraCyc + WrAvgXtraCyc));
					break;
				default:
					v = 0;
					break;
			}
			break;
		default: /* keep compiler happy */
			v = 0;
			break;
	}

	return v;
}

static void SetDcoArgFields(WorkR *p, bool src,
	uint8_t CurAMd, uint8_t CurArgDat)
{
	if (src) {
		p->DecOp.y.v[0].AMd = CurAMd;
		p->DecOp.y.v[0].ArgDat = CurArgDat;
	} else {
		p->DecOp.y.v[1].AMd = CurAMd;
		p->DecOp.y.v[1].ArgDat = CurArgDat;
	}
}

/* Validate an addressing mode against the set of modes allowed
   by the current instruction, and populate the decoded-op fields. */
static bool CheckValidAddrMode(WorkR *p,
	uint8_t m, uint8_t r, uint8_t v, bool src)
{
	uint8_t CurAMd = 0; /* init to keep compiler happy */
	uint8_t CurArgDat = 0;
	bool IsOk;

	switch (m) {
		case 0:
			CurAMd = GetAMdRegSz(p);
			CurArgDat = r;
			IsOk = CheckInSet(v,
				kAddrValidMaskAny | kAddrValidMaskData
					| kAddrValidMaskDataAlt | kAddrValidMaskDataNoCn);
			break;
		case 1:
			CurAMd = GetAMdRegSz(p);
			CurArgDat = r + 8;
			IsOk = CheckInSet(v, kAddrValidMaskAny);
			break;
		case 2:
			CurAMd = GetAMdIndirectSz(p);
			CurArgDat = r + 8;
			IsOk = CheckInSet(v,
				kAddrValidMaskAny | kAddrValidMaskData
					| kAddrValidMaskDataAlt | kAddrValidMaskControl
					| kAddrValidMaskControlAlt | kAddrValidMaskAltMem
					| kAddrValidMaskDataNoCn);
			break;
		case 3:
			switch (p->opsize) {
				case 1:
					if (7 == r) {
						CurAMd = kAMdAPosInc7B;
					} else {
						CurAMd = kAMdAPosIncB;
					}
					break;
				case 2:
				default: /* keep compiler happy */
					CurAMd = kAMdAPosIncW;
					break;
				case 4:
					CurAMd = kAMdAPosIncL;
					break;
			}
			CurArgDat = r + 8;
			IsOk = CheckInSet(v,
				kAddrValidMaskAny | kAddrValidMaskData
					| kAddrValidMaskDataAlt | kAddrValidMaskAltMem
					| kAddrValidMaskDataNoCn);
			break;
		case 4:
			switch (p->opsize) {
				case 1:
					if (7 == r) {
						CurAMd = kAMdAPreDec7B;
					} else {
						CurAMd = kAMdAPreDecB;
					}
					break;
				case 2:
				default: /* keep compiler happy */
					CurAMd = kAMdAPreDecW;
					break;
				case 4:
					CurAMd = kAMdAPreDecL;
					break;
			}
			CurArgDat = r + 8;
			IsOk = CheckInSet(v,
				kAddrValidMaskAny | kAddrValidMaskData
					| kAddrValidMaskDataAlt | kAddrValidMaskAltMem
					| kAddrValidMaskDataNoCn);
			break;
		case 5:
			switch (p->opsize) {
				case 1:
					CurAMd = kAMdADispB;
					break;
				case 2:
				default: /* keep compiler happy */
					CurAMd = kAMdADispW;
					break;
				case 4:
					CurAMd = kAMdADispL;
					break;
			}
			CurArgDat = r + 8;
			IsOk = CheckInSet(v,
				kAddrValidMaskAny | kAddrValidMaskData
					| kAddrValidMaskDataAlt | kAddrValidMaskControl
					| kAddrValidMaskControlAlt | kAddrValidMaskAltMem
					| kAddrValidMaskDataNoCn);
			break;
		case 6:
			switch (p->opsize) {
				case 1:
					CurAMd = kAMdAIndexB;
					break;
				case 2:
				default: /* keep compiler happy */
					CurAMd = kAMdAIndexW;
					break;
				case 4:
					CurAMd = kAMdAIndexL;
					break;
			}
			CurArgDat = r + 8;
			IsOk = CheckInSet(v,
				kAddrValidMaskAny | kAddrValidMaskData
					| kAddrValidMaskDataAlt | kAddrValidMaskControl
					| kAddrValidMaskControlAlt | kAddrValidMaskAltMem
					| kAddrValidMaskDataNoCn);
			break;
		case 7:
			switch (r) {
				case 0:
					switch (p->opsize) {
						case 1:
							CurAMd = kAMdAbsWB;
							break;
						case 2:
						default: /* keep compiler happy */
							CurAMd = kAMdAbsWW;
							break;
						case 4:
							CurAMd = kAMdAbsWL;
							break;
					}
					IsOk = CheckInSet(v,
						kAddrValidMaskAny | kAddrValidMaskData
							| kAddrValidMaskDataAlt
							| kAddrValidMaskControl
							| kAddrValidMaskControlAlt
							| kAddrValidMaskAltMem
							| kAddrValidMaskDataNoCn);
					break;
				case 1:
					switch (p->opsize) {
						case 1:
							CurAMd = kAMdAbsLB;
							break;
						case 2:
						default: /* keep compiler happy */
							CurAMd = kAMdAbsLW;
							break;
						case 4:
							CurAMd = kAMdAbsLL;
							break;
					}
					IsOk = CheckInSet(v,
						kAddrValidMaskAny | kAddrValidMaskData
							| kAddrValidMaskDataAlt
							| kAddrValidMaskControl
							| kAddrValidMaskControlAlt
							| kAddrValidMaskAltMem
							| kAddrValidMaskDataNoCn);
					break;
				case 2:
					switch (p->opsize) {
						case 1:
							CurAMd = kAMdPCDispB;
							break;
						case 2:
						default: /* keep compiler happy */
							CurAMd = kAMdPCDispW;
							break;
						case 4:
							CurAMd = kAMdPCDispL;
							break;
					}
					IsOk = CheckInSet(v,
						kAddrValidMaskAny | kAddrValidMaskData
							| kAddrValidMaskControl
							| kAddrValidMaskDataNoCn);
					break;
				case 3:
					switch (p->opsize) {
						case 1:
							CurAMd = kAMdPCIndexB;
							break;
						case 2:
						default: /* keep compiler happy */
							CurAMd = kAMdPCIndexW;
							break;
						case 4:
							CurAMd = kAMdPCIndexL;
							break;
					}
					IsOk = CheckInSet(v,
						kAddrValidMaskAny | kAddrValidMaskData
							| kAddrValidMaskControl
							| kAddrValidMaskDataNoCn);
					break;
				case 4:
					switch (p->opsize) {
						case 1:
							CurAMd = kAMdImmedB;
							break;
						case 2:
						default: /* keep compiler happy */
							CurAMd = kAMdImmedW;
							break;
						case 4:
							CurAMd = kAMdImmedL;
							break;
					}
					IsOk = CheckInSet(v,
						kAddrValidMaskAny | kAddrValidMaskData);
					break;
				default:
					IsOk = false;
					break;
			}
			break;
		default: /* keep compiler happy */
			IsOk = false;
			break;
	}

	if (IsOk) {
		SetDcoArgFields(p, src, CurAMd, CurArgDat);
	}

	return IsOk;
}

static uint8_t LeaPeaEACalcCyc(WorkR *p, uint8_t m, uint8_t r)
{
	uint16_t v;

	UNUSED(p);
	switch (m) {
		case 2:
			v = 0;
			break;
		case 5:
			v = (4 * kCycleScale + RdAvgXtraCyc);
			break;
		case 6:
			v = (8 * kCycleScale + RdAvgXtraCyc);
			break;
		case 7:
			switch (r) {
				case 0:
					v = (4 * kCycleScale + RdAvgXtraCyc);
					break;
				case 1:
					v = (8 * kCycleScale + 2 * RdAvgXtraCyc);
					break;
				case 2:
					v = (4 * kCycleScale + RdAvgXtraCyc);
					break;
				case 3:
					v = (8 * kCycleScale + RdAvgXtraCyc);
					break;
				default:
					v = 0;
					break;
			}
			break;
		default: /* keep compiler happy */
			v = 0;
			break;
	}

	return v;
}

static bool IsValidAddrMode(WorkR *p)
{
	return CheckValidAddrMode(p,
		mode(p), reg(p), kAddrValidAny, false);
}

static bool CheckDataAltAddrMode(WorkR *p)
{
	return CheckValidAddrMode(p,
		mode(p), reg(p), kAddrValidDataAlt, false);
}

static bool CheckDataAddrMode(WorkR *p)
{
	return CheckValidAddrMode(p,
		mode(p), reg(p), kAddrValidData, false);
}

static bool CheckControlAddrMode(WorkR *p)
{
	return CheckValidAddrMode(p,
		mode(p), reg(p), kAddrValidControl, false);
}

static bool CheckControlAltAddrMode(WorkR *p)
{
	return CheckValidAddrMode(p,
		mode(p), reg(p), kAddrValidControlAlt, false);
}

static bool CheckAltMemAddrMode(WorkR *p)
{
	return CheckValidAddrMode(p,
		mode(p), reg(p), kAddrValidAltMem, false);
}

static void FindOpSizeFromb76(WorkR *p)
{
	p->opsize = 1 << b76(p);
}

static uint8_t OpSizeOffset(WorkR *p)
{
	uint8_t v;

	switch (p->opsize) {
		case 1 :
			v = 0;
			break;
		case 2 :
			v = 1;
			break;
		case 4 :
		default :
			v = 2;
			break;
	}

	return v;
}


static uint32_t octdat(uint32_t x)
{
	if (x == 0) {
		return 8;
	} else {
		return x;
	}
}

/* Decode an opcode class 0 instruction: bit operations, MOVEP,
   immediate arithmetic, and static/dynamic bit test/modify. */
static inline void DeCode0(WorkR *p)
{
	if (b8(p) == 1) {
		if (mode(p) == 1) {
			/* MoveP 0000ddd1mm001aaa */
			switch (b76(p)) {
				case 0:
					p->Cycles = (16 * kCycleScale + 4 * RdAvgXtraCyc);
					break;
				case 1:
					p->Cycles = (24 * kCycleScale + 6 * RdAvgXtraCyc);
					break;
				case 2:
					p->Cycles = (16 * kCycleScale
						+ 2 * RdAvgXtraCyc + 2 * WrAvgXtraCyc);
					break;
				case 3:
				default: /* keep compiler happy */
					p->Cycles = (24 * kCycleScale
						+ 2 * RdAvgXtraCyc + 4 * WrAvgXtraCyc);
					break;
			}
			if (CheckValidAddrMode(p, 1, reg(p),
				kAddrValidAny, true))
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, false))
			{
				p->MainClass = kIKindMoveP0 + b76(p);
			}
		} else {
			/* dynamic bit, Opcode = 0000ddd1ttmmmrrr */
			if (mode(p) == 0) {
				p->opsize = 4;
				switch (b76(p)) {
					case 0: /* BTst */
						p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
						break;
					case 1: /* BChg */
						p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
						break;
					case 2: /* BClr */
						p->Cycles = (10 * kCycleScale + RdAvgXtraCyc);
						break;
					case 3: /* BSet */
						p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
						break;
				}
				p->MainClass = kIKindBTstL + b76(p);
				SetDcoArgFields(p, true, kAMdRegL, rg9(p));
				SetDcoArgFields(p, false, kAMdRegL, reg(p));
			} else {
				p->opsize = 1;
				p->MainClass = kIKindBTstB + b76(p);
				SetDcoArgFields(p, true, kAMdRegB, rg9(p));
				if (b76(p) == 0) { /* BTst */
					if (CheckDataAddrMode(p)) {
						p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					}
				} else {
					if (CheckDataAltAddrMode(p)) {
						p->Cycles = (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					}
				}
			}
		}
	} else {
		if (rg9(p) == 4) {
			/* static bit 00001000ssmmmrrr */
			if (mode(p) == 0) {
				p->opsize = 4;
				switch (b76(p)) {
					case 0: /* BTst */
						p->Cycles =
							(10 * kCycleScale + 2 * RdAvgXtraCyc);
						break;
					case 1: /* BChg */
						p->Cycles =
							(12 * kCycleScale + 2 * RdAvgXtraCyc);
						break;
					case 2: /* BClr */
						p->Cycles =
							(14 * kCycleScale + 2 * RdAvgXtraCyc);
						break;
					case 3: /* BSet */
						p->Cycles =
							(12 * kCycleScale + 2 * RdAvgXtraCyc);
						break;
				}
				SetDcoArgFields(p, true, kAMdImmedB, 0);
				SetDcoArgFields(p, false, kAMdRegL, reg(p));
				p->MainClass = kIKindBTstL + b76(p);
			} else {
				p->opsize = 1;
				SetDcoArgFields(p, true, kAMdImmedB, 0);
				p->MainClass = kIKindBTstB + b76(p);
				if (b76(p) == 0) { /* BTst */
					if (CheckValidAddrMode(p,
						mode(p), reg(p), kAddrValidDataNoCn, false))
					{
						p->Cycles =
							(8 * kCycleScale + 2 * RdAvgXtraCyc);
						p->Cycles +=
							OpEACalcCyc(p, mode(p), reg(p));
					}
				} else {
					if (CheckDataAltAddrMode(p)) {
						p->Cycles = (12 * kCycleScale
							+ 2 * RdAvgXtraCyc + WrAvgXtraCyc);
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					}
				}
			}
		} else
		if (b76(p) == 3) {
			if (rg9(p) < 3) {
				/* CHK2 or CMP2 00000ss011mmmrrr */
				switch ((p->opcode >> 9) & 3) {
					case 0 :
						p->opsize = 1;
						break;
					case 1 :
						p->opsize = 2;
						break;
					case 2 :
						p->opsize = 4;
						break;
				}
				p->DecOp.y.v[0].ArgDat = p->opsize;
					/* size */
				if (CheckControlAddrMode(p)) {
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindCHK2orCMP2;
				}
			} else
			if (rg9(p) >= 5) {
				switch ((p->opcode >> 9) & 3) {
					case 1 :
						p->opsize = 1;
						break;
					case 2 :
						p->opsize = 2;
						break;
					case 3 :
						p->opsize = 4;
						break;
				}
				p->DecOp.y.v[0].ArgDat = p->opsize;
				if ((mode(p) == 7) && (reg(p) == 4)) {
					/* CAS2 00001ss011111100 */
					p->MainClass = kIKindCAS2;
				} else {
					/* CAS 00001ss011mmmrrr */
					if (CheckDataAltAddrMode(p)) {
						p->MainClass = kIKindCAS;
					}
				}
			} else
			if (rg9(p) == 3) {
				/* CALLM or RTM 0000011011mmmrrr */
				p->MainClass = kIKindCallMorRtm;
			} else
			{
				p->MainClass = kIKindIllegal;
			}
		} else
		if (rg9(p) == 6) {
			/* CMPI 00001100ssmmmrrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, 7, 4, kAddrValidAny, true))
			if (CheckValidAddrMode(p,
				mode(p), reg(p), kAddrValidDataNoCn, false))
			{
				if (0 == mode(p)) {
					p->Cycles = (4 == p->opsize)
						? (14 * kCycleScale + 3 * RdAvgXtraCyc)
						: (8 * kCycleScale + 2 * RdAvgXtraCyc);
				} else {
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale + 3 * RdAvgXtraCyc)
						: (8 * kCycleScale + 2 * RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindCmpB + OpSizeOffset(p);
			}
		} else if (rg9(p) == 7) {
			/* MoveS 00001110ssmmmrrr */
			FindOpSizeFromb76(p);
			p->DecOp.y.v[0].ArgDat = p->opsize;
			if (CheckAltMemAddrMode(p)) {
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindMoveS;
			}
		} else {
			if ((mode(p) == 7) && (reg(p) == 4)) {
				if (b76(p) >= 2) {
					p->MainClass = kIKindIllegal;
				} else {
					switch (rg9(p)) {
						case 0:
							p->Cycles =
								(20 * kCycleScale + 3 * RdAvgXtraCyc);
							p->MainClass = (0 != b76(p))
								? kIKindOrISR : kIKindOrICCR;
							break;
						case 1:
							p->Cycles =
								(20 * kCycleScale + 3 * RdAvgXtraCyc);
							p->MainClass = (0 != b76(p))
								? kIKindAndISR : kIKindAndICCR;
							break;
						case 5:
							p->Cycles =
								(20 * kCycleScale + 3 * RdAvgXtraCyc);
							p->MainClass = (0 != b76(p))
								? kIKindEorISR : kIKindEorICCR;
							break;
						default:
							p->MainClass = kIKindIllegal;
							break;
					}
				}
			} else {
				switch (rg9(p)) {
					case 0:
						FindOpSizeFromb76(p);
						if (CheckValidAddrMode(p, 7, 4,
							kAddrValidAny, true))
						if (CheckValidAddrMode(p, mode(p), reg(p),
							kAddrValidDataAlt, false))
						{
							if (0 != mode(p)) {
								p->Cycles = (4 == p->opsize)
									? (20 * kCycleScale
										+ 3 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc)
									: (12 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ WrAvgXtraCyc);
							} else {
								p->Cycles = (4 == p->opsize)
									? (16 * kCycleScale
										+ 3 * RdAvgXtraCyc)
									: (8 * kCycleScale
										+ 2 * RdAvgXtraCyc);
							}
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindOrI;
						}
						break;
					case 1:
						FindOpSizeFromb76(p);
						if (CheckValidAddrMode(p, 7, 4,
							kAddrValidAny, true))
						if (CheckValidAddrMode(p, mode(p), reg(p),
							kAddrValidDataAlt, false))
						{
							if (0 != mode(p)) {
								p->Cycles = (4 == p->opsize)
									? (20 * kCycleScale
										+ 3 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc)
									: (12 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ WrAvgXtraCyc);
							} else {
								p->Cycles = (4 == p->opsize)
									? (14 * kCycleScale
										+ 3 * RdAvgXtraCyc)
									: (8 * kCycleScale
										+ 2 * RdAvgXtraCyc);
							}
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindAndI;
						}
						break;
					case 2:
						FindOpSizeFromb76(p);
						if (CheckValidAddrMode(p, 7, 4,
							kAddrValidAny, true))
						if (CheckValidAddrMode(p, mode(p), reg(p),
							kAddrValidDataAlt, false))
						{
							if (0 != mode(p)) {
								p->Cycles = (4 == p->opsize)
									? (20 * kCycleScale
										+ 3 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc)
									: (12 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ WrAvgXtraCyc);
							} else {
								p->Cycles = (4 == p->opsize)
									? (16 * kCycleScale
										+ 3 * RdAvgXtraCyc)
									: (8 * kCycleScale
										+ 2 * RdAvgXtraCyc);
							}
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindSubB + OpSizeOffset(p);
						}
						break;
					case 3:
						FindOpSizeFromb76(p);
						if (CheckValidAddrMode(p, 7, 4,
							kAddrValidAny, true))
						if (CheckValidAddrMode(p, mode(p), reg(p),
							kAddrValidDataAlt, false))
						{
							if (0 != mode(p)) {
								p->Cycles = (4 == p->opsize)
									? (20 * kCycleScale
										+ 3 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc)
									: (12 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ WrAvgXtraCyc);
							} else {
								p->Cycles = (4 == p->opsize)
									? (16 * kCycleScale
										+ 3 * RdAvgXtraCyc)
									: (8 * kCycleScale
										+ 2 * RdAvgXtraCyc);
							}
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindAddB + OpSizeOffset(p);
						}
						break;
					case 5:
						FindOpSizeFromb76(p);
						if (CheckValidAddrMode(p, 7, 4,
							kAddrValidAny, true))
						if (CheckValidAddrMode(p, mode(p), reg(p),
							kAddrValidDataAlt, false))
						{
							if (0 != mode(p)) {
								p->Cycles = (4 == p->opsize)
									? (20 * kCycleScale
										+ 3 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc)
									: (12 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ WrAvgXtraCyc);
							} else {
								p->Cycles = (4 == p->opsize)
									? (16 * kCycleScale
										+ 3 * RdAvgXtraCyc)
									: (8 * kCycleScale
										+ 2 * RdAvgXtraCyc);
							}
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindEorI;
						}
						break;
					default:
						/* for compiler. should be 0, 1, 2, 3, or 5 */
						p->MainClass = kIKindIllegal;
						break;
				}
			}
		}
	}
}

static inline void DeCode1(WorkR *p)
{
	p->opsize = 1;
	if (md6(p) == 1) { /* MOVEA */
		p->MainClass = kIKindIllegal;
	} else if (mode(p) == 1) {
		/* not allowed for byte sized move */
		p->MainClass = kIKindIllegal;
	} else {
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		if (CheckValidAddrMode(p, md6(p), rg9(p),
			kAddrValidDataAlt, false))
		{
			p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->Cycles += OpEADestCalcCyc(p, md6(p), rg9(p));
			p->MainClass = kIKindMoveB;
		}
	}
}

static inline void DeCode2(WorkR *p)
{
	p->opsize = 4;
	if (md6(p) == 1) { /* MOVEA */
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		if (CheckValidAddrMode(p, 1, rg9(p),
			kAddrValidAny, false))
		{
			p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->MainClass = kIKindMoveAL;
			p->DecOp.y.v[1].ArgDat = rg9(p);
		}
	} else {
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		if (CheckValidAddrMode(p, md6(p), rg9(p),
			kAddrValidDataAlt, false))
		{
			p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->Cycles += OpEADestCalcCyc(p, md6(p), rg9(p));
			p->MainClass = kIKindMoveL;
		}
	}
}

static inline void DeCode3(WorkR *p)
{
	p->opsize = 2;
	if (md6(p) == 1) { /* MOVEA */
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		if (CheckValidAddrMode(p, 1, rg9(p),
			kAddrValidAny, false))
		{
			p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->MainClass = kIKindMoveAW;
			p->DecOp.y.v[1].ArgDat = rg9(p);
		}
	} else {
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		if (CheckValidAddrMode(p, md6(p), rg9(p),
			kAddrValidDataAlt, false))
		{
			p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->Cycles += OpEADestCalcCyc(p, md6(p), rg9(p));
			p->MainClass = kIKindMoveW;
		}
	}
}


#define MoveAvgN 0

static uint16_t MoveMEACalcCyc(WorkR *p, uint8_t m, uint8_t r)
{
	uint16_t v;

	UNUSED(p);
	switch (m) {
		case 2:
		case 3:
		case 4:
			v = (8 * kCycleScale + 2 * RdAvgXtraCyc);
			break;
		case 5:
			v = (12 * kCycleScale + 3 * RdAvgXtraCyc);
			break;
		case 6:
			v = (14 * kCycleScale + 3 * RdAvgXtraCyc);
			break;
		case 7:
			switch (r) {
				case 0:
					v = (12 * kCycleScale + 3 * RdAvgXtraCyc);
					break;
				case 1:
					v = (16 * kCycleScale + 4 * RdAvgXtraCyc);
					break;
				default:
					v = 0;
					break;
			}
			break;
		default: /* keep compiler happy */
			v = 0;
			break;
	}

	return v;
}


static inline void DeCode4(WorkR *p)
{
	if (b8(p) != 0) {
		switch (b76(p)) {
			case 0:
				/* Chk.L 0100ddd100mmmrrr */
				p->opsize = 4;
				if (CheckValidAddrMode(p, mode(p), reg(p),
					kAddrValidData, false))
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, true))
				{
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindChkL;
				}
				break;
			case 1:
				p->MainClass = kIKindIllegal;
				break;
			case 2:
				/* Chk.W 0100ddd110mmmrrr */
				p->opsize = 2;
				if (CheckValidAddrMode(p, mode(p), reg(p),
					kAddrValidData, false))
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, true))
				{
					p->Cycles = (10 * kCycleScale + RdAvgXtraCyc);
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindChkW;
				}
				break;
			case 3:
			default: /* keep compiler happy */
				if ((0 == mode(p)) && (4 == rg9(p))) {
					/* EXTB.L */
					SetDcoArgFields(p, false,
						kAMdRegL, reg(p));
					p->MainClass = kIKindEXTBL;
				} else
				{
					/* Lea 0100aaa111mmmrrr */
					if (CheckControlAddrMode(p)) {
						p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
						p->Cycles +=
							LeaPeaEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindLea;
						p->DecOp.y.v[0].ArgDat = rg9(p);
					}
				}
				break;
		}
	} else {
		switch (rg9(p)) {
			case 0:
				if (b76(p) != 3) {
					/* NegX 01000000ssmmmrrr */
					FindOpSizeFromb76(p);
					if (CheckDataAltAddrMode(p)) {
						if (0 != mode(p)) {
							p->Cycles = (4 == p->opsize)
								? (12 * kCycleScale
									+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
								: (8 * kCycleScale
									+ RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles = (4 == p->opsize)
								? (6 * kCycleScale + RdAvgXtraCyc)
								: (4 * kCycleScale + RdAvgXtraCyc);
						}
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindNegXB + OpSizeOffset(p);
					}
				} else {
/* reference seems incorrect to say not for 68000 */
					/* Move from SR 0100000011mmmrrr */
					p->opsize = 2;
					if (CheckDataAltAddrMode(p)) {
						p->Cycles =
							(12 * kCycleScale + 2 * RdAvgXtraCyc);
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindMoveSREa;
					}
				}
				break;
			case 1:
				if (b76(p) != 3) {
					/* Clr 01000010ssmmmrrr */
					FindOpSizeFromb76(p);
					if (CheckDataAltAddrMode(p)) {
						if (0 != mode(p)) {
							p->Cycles = (4 == p->opsize)
								? (12 * kCycleScale
									+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
								: (8 * kCycleScale
									+ RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles = (4 == p->opsize)
								? (6 * kCycleScale + RdAvgXtraCyc)
								: (4 * kCycleScale + RdAvgXtraCyc);
						}
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindClr;
					}
				} else {
					/* Move from CCR 0100001011mmmrrr */
					p->opsize = 2;
					if (CheckDataAltAddrMode(p)) {
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindMoveCCREa;
					}
				}
				break;
			case 2:
				if (b76(p) != 3) {
					/* Neg 01000100ssmmmrrr */
					FindOpSizeFromb76(p);
					if (CheckDataAltAddrMode(p)) {
						if (0 != mode(p)) {
							p->Cycles = (4 == p->opsize)
								? (12 * kCycleScale
									+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
								: (8 * kCycleScale
									+ RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles = (4 == p->opsize)
								? (6 * kCycleScale + RdAvgXtraCyc)
								: (4 * kCycleScale + RdAvgXtraCyc);
						}
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindNegB + OpSizeOffset(p);
					}
				} else {
					/* Move to CCR 0100010011mmmrrr */
					p->opsize = 2;
					if (CheckDataAddrMode(p)) {
						p->Cycles = (12 * kCycleScale + RdAvgXtraCyc);
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindMoveEaCCR;
					}
				}
				break;
			case 3:
				if (b76(p) != 3) {
					/* Not 01000110ssmmmrrr */
					FindOpSizeFromb76(p);
					if (CheckDataAltAddrMode(p)) {
						if (0 != mode(p)) {
							p->Cycles = (4 == p->opsize)
								? (12 * kCycleScale
									+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
								: (8 * kCycleScale
									+ RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles = (4 == p->opsize)
								? (6 * kCycleScale + RdAvgXtraCyc)
								: (4 * kCycleScale + RdAvgXtraCyc);
						}
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindNot;
					}
				} else {
					/* Move to SR 0100011011mmmrrr */
					p->opsize = 2;
					if (CheckDataAddrMode(p)) {
						if (0 != mode(p)) {
							p->Cycles = (8 * kCycleScale
								+ RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles =
								(6 * kCycleScale + RdAvgXtraCyc);
						}
						p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
						p->MainClass = kIKindMoveEaSR;
					}
				}
				break;
			case 4:
				switch (b76(p)) {
					case 0:
						if (mode(p) == 1) {
							/* Link.L 0100100000001rrr */
							SetDcoArgFields(p, false,
								kAMdRegL, reg(p) + 8);
							p->MainClass = kIKindLinkL;
						} else
						{
							/* Nbcd 0100100000mmmrrr */
							p->opsize = 1;
							if (CheckDataAltAddrMode(p)) {
								if (0 != mode(p)) {
									p->Cycles = (8 * kCycleScale
										+ RdAvgXtraCyc + WrAvgXtraCyc);
								} else {
									p->Cycles = (6 * kCycleScale
										+ RdAvgXtraCyc);
								}
								p->Cycles +=
									OpEACalcCyc(p, mode(p), reg(p));
								p->MainClass = kIKindNbcd;
							}
						}
						break;
					case 1:
						if (mode(p) == 0) {
							/* Swap 0100100001000rrr */
							p->Cycles =
								(4 * kCycleScale + RdAvgXtraCyc);
							p->MainClass = kIKindSwap;
							SetDcoArgFields(p, false,
								kAMdRegL, reg(p));
						} else
						if (mode(p) == 1) {
							p->MainClass = kIKindBkpt;
						} else
						{
							/* PEA 0100100001mmmrrr */
							if (CheckControlAddrMode(p)) {
								p->Cycles = (12 * kCycleScale
										+ RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc);
								p->Cycles +=
									LeaPeaEACalcCyc(p, mode(p), reg(p));
								p->MainClass = kIKindPEA;
							}
						}
						break;
					case 2:
						if (mode(p) == 0) {
							/* EXT.W */
							p->Cycles =
								(4 * kCycleScale + RdAvgXtraCyc);
							SetDcoArgFields(p, false,
								kAMdRegW, reg(p));
							p->MainClass = kIKindEXTW;
						} else {
							/* MOVEM reg to mem 01001d001ssmmmrrr */
							p->opsize = 2;
							if (mode(p) == 4) {
								p->Cycles =
									MoveMEACalcCyc(p, mode(p), reg(p));
								p->Cycles += MoveAvgN * 4 * kCycleScale
									+ MoveAvgN * WrAvgXtraCyc;
								SetDcoArgFields(p, false,
									kAMdAPreDecL, reg(p) + 8);
								p->MainClass = kIKindMOVEMRmMW;
							} else {
								if (CheckControlAltAddrMode(p)) {
									p->Cycles = MoveMEACalcCyc(p,
										mode(p), reg(p));
									p->Cycles +=
										MoveAvgN * 4 * kCycleScale
											+ MoveAvgN * WrAvgXtraCyc;
									p->MainClass = kIKindMOVEMrmW;
								}
							}
						}
						break;
					case 3:
					default: /* keep compiler happy */
						if (mode(p) == 0) {
							/* EXT.L */
							p->Cycles =
								(4 * kCycleScale + RdAvgXtraCyc);
							SetDcoArgFields(p, false,
								kAMdRegL, reg(p));
							p->MainClass = kIKindEXTL;
						} else {
							/* MOVEM reg to mem 01001d001ssmmmrrr */
							p->Cycles = MoveMEACalcCyc(p,
								mode(p), reg(p));
							p->Cycles += MoveAvgN * 8 * kCycleScale
								+ MoveAvgN * 2 * WrAvgXtraCyc;
							p->opsize = 4;
							if (mode(p) == 4) {
								SetDcoArgFields(p, false,
									kAMdAPreDecL, reg(p) + 8);
								p->MainClass = kIKindMOVEMRmML;
							} else {
								if (CheckControlAltAddrMode(p)) {
									p->MainClass = kIKindMOVEMrmL;
								}
							}
						}
						break;
				}
				break;
			case 5:
				if (b76(p) == 3) {
					if ((mode(p) == 7) && (reg(p) == 4)) {
						/* the ILLEGAL instruction */
						p->MainClass = kIKindIllegal;
					} else {
						/* Tas 0100101011mmmrrr */
						p->opsize = 1;
						if (CheckDataAltAddrMode(p)) {
							if (0 != mode(p)) {
								p->Cycles = (14 * kCycleScale
									+ 2 * RdAvgXtraCyc + WrAvgXtraCyc);
							} else {
								p->Cycles = (4 * kCycleScale
									+ RdAvgXtraCyc);
							}
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindTas;
						}
					}
				} else {
					/* Tst 01001010ssmmmrrr */
					FindOpSizeFromb76(p);
					if (b76(p) == 0) {
						if (CheckDataAltAddrMode(p)) {
							p->Cycles =
								(4 * kCycleScale + RdAvgXtraCyc);
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindTst;
						}
					} else {
						if (IsValidAddrMode(p)) {
							p->Cycles =
								(4 * kCycleScale + RdAvgXtraCyc);
							p->Cycles +=
								OpEACalcCyc(p, mode(p), reg(p));
							p->MainClass = kIKindTst;
						}
					}
				}
				break;
			case 6:
				if (((p->opcode >> 7) & 1) == 1) {
					/* MOVEM mem to reg 010011001smmmrrr */
					p->opsize = 2 * b76(p) - 2;
					if (mode(p) == 3) {
						p->Cycles = 4 * kCycleScale + RdAvgXtraCyc;
						p->Cycles += MoveMEACalcCyc(p, mode(p), reg(p));
						if (4 == p->opsize) {
							p->Cycles += MoveAvgN * 8 * kCycleScale
								+ 2 * MoveAvgN * RdAvgXtraCyc;
						} else {
							p->Cycles += MoveAvgN * 4 * kCycleScale
								+ MoveAvgN * RdAvgXtraCyc;
						}
						SetDcoArgFields(p, false,
							kAMdAPosIncL, reg(p) + 8);
						if (b76(p) == 2) {
							p->MainClass = kIKindMOVEMApRW;
						} else {
							p->MainClass = kIKindMOVEMApRL;
						}
					} else {
						if (CheckControlAddrMode(p)) {
							p->Cycles = 4 * kCycleScale + RdAvgXtraCyc;
							p->Cycles += MoveMEACalcCyc(p,
								mode(p), reg(p));
							if (4 == p->opsize) {
								p->Cycles += MoveAvgN * 8 * kCycleScale
									+ 2 * MoveAvgN * RdAvgXtraCyc;
							} else {
								p->Cycles += MoveAvgN * 4 * kCycleScale
									+ MoveAvgN * RdAvgXtraCyc;
							}
							if (4 == p->opsize) {
								p->MainClass = kIKindMOVEMmrL;
							} else {
								p->MainClass = kIKindMOVEMmrW;
							}
						}
					}
				} else {
					p->opsize = 4;

					if (CheckDataAddrMode(p)) {
						if (((p->opcode >> 6) & 1) == 1) {
							/* DIVU 0100110001mmmrrr 0rrr0s0000000rrr */
							/* DIVS 0100110001mmmrrr 0rrr1s0000000rrr */
							p->MainClass = kIKindDivL;
						} else {
							/* MULU 0100110000mmmrrr 0rrr0s0000000rrr */
							/* MULS 0100110000mmmrrr 0rrr1s0000000rrr */
							p->MainClass = kIKindMulL;
						}
					}
				}
				break;
			case 7:
			default: /* keep compiler happy */
				switch (b76(p)) {
					case 0:
						p->MainClass = kIKindIllegal;
						break;
					case 1:
						switch (mode(p)) {
							case 0:
							case 1:
								/* Trap 010011100100vvvv */
								p->Cycles = (34 * kCycleScale
									+ 4 * RdAvgXtraCyc
									+ 3 * WrAvgXtraCyc);
								SetDcoArgFields(p, false,
									kAMdDat4, (p->opcode & 15) + 32);
								p->MainClass = kIKindTrap;
								break;
							case 2:
								/* Link */
								p->Cycles = (16 * kCycleScale
									+ 2 * RdAvgXtraCyc
									+ 2 * WrAvgXtraCyc);
								SetDcoArgFields(p, false,
									kAMdRegL, reg(p) + 8);
								if (reg(p) == 6) {
									p->MainClass = kIKindLinkA6;
								} else {
									p->MainClass = kIKindLink;
								}
								break;
							case 3:
								/* Unlk */
								p->Cycles = (12 * kCycleScale
									+ 3 * RdAvgXtraCyc);
								SetDcoArgFields(p, false,
									kAMdRegL, reg(p) + 8);
								if (reg(p) == 6) {
									p->MainClass = kIKindUnlkA6;
								} else {
									p->MainClass = kIKindUnlk;
								}
								break;
							case 4:
								/* MOVE USP 0100111001100aaa */
								p->Cycles =
									(4 * kCycleScale + RdAvgXtraCyc);
								SetDcoArgFields(p, false,
									kAMdRegL, reg(p) + 8);
								p->MainClass = kIKindMoveRUSP;
								break;
							case 5:
								/* MOVE USP 0100111001101aaa */
								p->Cycles =
									(4 * kCycleScale + RdAvgXtraCyc);
								SetDcoArgFields(p, false,
									kAMdRegL, reg(p) + 8);
								p->MainClass = kIKindMoveUSPR;
								break;
							case 6:
								switch (reg(p)) {
									case 0:
										/* Reset 0100111001110000 */
										p->Cycles = (132 * kCycleScale
											+ RdAvgXtraCyc);
										p->MainClass = kIKindReset;
										break;
									case 1:
										/* Nop = 0100111001110001 */
										p->Cycles = (4 * kCycleScale
											+ RdAvgXtraCyc);
										p->MainClass = kIKindNop;
										break;
									case 2:
										/* Stop 0100111001110010 */
										p->Cycles = (4 * kCycleScale);
										p->MainClass = kIKindStop;
										break;
									case 3:
										/* Rte 0100111001110011 */
										p->Cycles = (20 * kCycleScale
											+ 5 * RdAvgXtraCyc);
										p->MainClass = kIKindRte;
										break;
									case 4:
										/* Rtd 0100111001110100 */
										p->MainClass = kIKindRtd;
										break;
									case 5:
										/* Rts 0100111001110101 */
										p->Cycles = (16 * kCycleScale
											+ 4 * RdAvgXtraCyc);
										p->MainClass = kIKindRts;
										break;
									case 6:
										/* TrapV 0100111001110110 */
										p->Cycles = (4 * kCycleScale
											+ RdAvgXtraCyc);
										p->MainClass = kIKindTrapV;
										break;
									case 7:
									default: /* keep compiler happy */
										/* Rtr 0100111001110111 */
										p->Cycles = (20 * kCycleScale
											+ 2 * RdAvgXtraCyc);
										p->MainClass = kIKindRtr;
										break;
								}
								break;
							case 7:
							default: /* keep compiler happy */
								/* MOVEC 010011100111101m */
								switch (reg(p)) {
									case 2:
										p->MainClass = kIKindMoveCEa;
										break;
									case 3:
										p->MainClass = kIKindMoveEaC;
										break;
									default:
										p->MainClass = kIKindIllegal;
										break;
								}
								break;
						}
						break;
					case 2:
						/* Jsr 0100111010mmmrrr */
						if (CheckControlAddrMode(p)) {
							switch (mode(p)) {
								case 2:
									p->Cycles = (16 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc);
									break;
								case 5:
									p->Cycles = (18 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc);
									break;
								case 6:
									p->Cycles = (22 * kCycleScale
										+ 2 * RdAvgXtraCyc
										+ 2 * WrAvgXtraCyc);
									break;
								case 7:
								default: /* keep compiler happy */
									switch (reg(p)) {
										case 0:
											p->Cycles =
												(18 * kCycleScale
													+ 2 * RdAvgXtraCyc
													+ 2 * WrAvgXtraCyc);
											break;
										case 1:
											p->Cycles =
												(20 * kCycleScale
													+ 3 * RdAvgXtraCyc
													+ 2 * WrAvgXtraCyc);
											break;
										case 2:
											p->Cycles =
												(18 * kCycleScale
													+ 2 * RdAvgXtraCyc
													+ 2 * WrAvgXtraCyc);
											break;
										case 3:
										default:
											/* keep compiler happy */
											p->Cycles =
												(22 * kCycleScale
													+ 2 * RdAvgXtraCyc
													+ 2 * WrAvgXtraCyc);
											break;
									}
									break;
							}
							p->MainClass = kIKindJsr;
						}
						break;
					case 3:
					default: /* keep compiler happy */
						/* JMP 0100111011mmmrrr */
						if (CheckControlAddrMode(p)) {
							switch (mode(p)) {
								case 2:
									p->Cycles = (8 * kCycleScale
										+ 2 * RdAvgXtraCyc);
									break;
								case 5:
									p->Cycles = (10 * kCycleScale
										+ 2 * RdAvgXtraCyc);
									break;
								case 6:
									p->Cycles = (14 * kCycleScale
										+ 2 * RdAvgXtraCyc);
									break;
								case 7:
								default: /* keep compiler happy */
									switch (reg(p)) {
										case 0:
											p->Cycles =
												(10 * kCycleScale
													+ 2 * RdAvgXtraCyc);
											break;
										case 1:
											p->Cycles =
												(12 * kCycleScale
													+ 3 * RdAvgXtraCyc);
											break;
										case 2:
											p->Cycles =
												(10 * kCycleScale
													+ 2 * RdAvgXtraCyc);
											break;
										case 3:
										default:
											/* keep compiler happy */
											p->Cycles =
												(14 * kCycleScale
													+ 3 * RdAvgXtraCyc);
											break;
									}
									break;
							}
							p->MainClass = kIKindJmp;
						}
						break;
				}
				break;
		}
	}
}

static inline void DeCode5(WorkR *p)
{
	if (b76(p) == 3) {
		p->DecOp.y.v[0].ArgDat = (p->opcode >> 8) & 15;
		if (mode(p) == 1) {
			/* DBcc 0101cccc11001ddd */
			p->Cycles = 0;
			SetDcoArgFields(p, false, kAMdRegW, reg(p));
			if (1 == ((p->opcode >> 8) & 15)) {
				p->MainClass = kIKindDBF;
			} else {
				p->MainClass = kIKindDBcc;
			}
		} else {
			if ((mode(p) == 7) && (reg(p) >= 2)) {
				/* TRAPcc 0101cccc11111sss */
				p->DecOp.y.v[1].ArgDat = reg(p);
				p->MainClass = kIKindTRAPcc;
			} else
			{
				p->opsize = 1;
				/* Scc 0101cccc11mmmrrr */
				if (CheckDataAltAddrMode(p)) {
					if (0 != mode(p)) {
						p->Cycles = (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
					} else {
						p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
					}
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindScc;
				}
			}
		}
	} else {
		if (mode(p) == 1) {
			if (0 == b76(p)) {
				p->MainClass = kIKindIllegal;
			} else {
				p->opsize = b76(p) * 2 + 2;
				SetDcoArgFields(p, true, kAMdDat4,
					octdat(rg9(p)));
				SetDcoArgFields(p, false, kAMdRegL,
					reg(p) + 8);
					/* always long, regardless of opsize */
				if (b8(p) == 0) {
					/* AddQA 0101nnn0ss001rrr */
					p->Cycles = (4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (4 * kCycleScale + RdAvgXtraCyc);
					p->MainClass = kIKindAddQA;
				} else {
					/* SubQA 0101nnn1ss001rrr */
					p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
					p->MainClass = kIKindSubQA;
				}
			}
		} else {
			FindOpSizeFromb76(p);
			SetDcoArgFields(p, true, kAMdDat4,
				octdat(rg9(p)));
			if (CheckValidAddrMode(p,
				mode(p), reg(p), kAddrValidDataAlt, false))
			{
				if (0 != mode(p)) {
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
				} else {
					p->Cycles = (4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (4 * kCycleScale + RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				if (b8(p) == 0) {
					/* AddQ 0101nnn0ssmmmrrr */
					p->MainClass = kIKindAddB + OpSizeOffset(p);
				} else {
					/* SubQ 0101nnn1ssmmmrrr */
					p->MainClass = kIKindSubB + OpSizeOffset(p);
				}
			}
		}
	}
}

static inline void DeCode6(WorkR *p)
{
	uint32_t cond = (p->opcode >> 8) & 15;

	if (cond == 1) {
		/* Bsr 01100001nnnnnnnn */
		p->Cycles = (18 * kCycleScale
			+ 2 * RdAvgXtraCyc + 2 * WrAvgXtraCyc);
		if (0 == (p->opcode & 255)) {
			p->MainClass = kIKindBsrW;
		} else
		if (255 == (p->opcode & 255)) {
			p->MainClass = kIKindBsrL;
		} else
		{
			p->MainClass = kIKindBsrB;
			p->DecOp.y.v[1].ArgDat = p->opcode & 255;
		}
	} else if (cond == 0) {
		/* Bra 01100000nnnnnnnn */
		p->Cycles = (10 * kCycleScale + 2 * RdAvgXtraCyc);
		if (0 == (p->opcode & 255)) {
			p->MainClass = kIKindBraW;
		} else
		if (255 == (p->opcode & 255)) {
			p->MainClass = kIKindBraL;
		} else
		{
			p->MainClass = kIKindBraB;
			p->DecOp.y.v[1].ArgDat = p->opcode & 255;
		}
	} else {
		/* Bcc 0110ccccnnnnnnnn */
		p->DecOp.y.v[0].ArgDat = cond;
		if (0 == (p->opcode & 255)) {
			p->Cycles = 0;
			p->MainClass = kIKindBccW;
		} else
		if (255 == (p->opcode & 255)) {
			p->MainClass = kIKindBccL;
		} else
		{
			p->Cycles = 0;
			p->MainClass = kIKindBccB;
			p->DecOp.y.v[1].ArgDat = p->opcode & 255;
		}
	}
}

static inline void DeCode7(WorkR *p)
{
	if (b8(p) == 0) {
		p->opsize = 4;
		p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
		p->MainClass = kIKindMoveQ;
		p->DecOp.y.v[0].ArgDat = p->opcode & 255;
		p->DecOp.y.v[1].ArgDat = rg9(p);
	} else {
		p->MainClass = kIKindIllegal;
	}
}

static inline void DeCode8(WorkR *p)
{
	if (b76(p) == 3) {
		p->opsize = 2;
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidData, true))
		if (CheckValidAddrMode(p, 0, rg9(p),
			kAddrValidAny, false))
		{
			if (b8(p) == 0) {
				/* DivU 1000ddd011mmmrrr */
				p->Cycles = RdAvgXtraCyc;
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindDivU;
			} else {
				/* DivS 1000ddd111mmmrrr */
				p->Cycles = RdAvgXtraCyc;
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindDivS;
			}
		}
	} else {
		if (b8(p) == 0) {
			/* OR 1000ddd0ssmmmrrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, mode(p), reg(p),
				kAddrValidData, true))
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, false))
			{
				if (4 == p->opsize) {
					if ((mode(p) < 2)
						|| ((7 == mode(p)) && (reg(p) == 4)))
					{
						p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
					} else {
						p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
					}
				} else {
					p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindOrEaD;
			}
		} else {
			if (mode(p) < 2) {
				switch (b76(p)) {
					case 0:
						/* SBCD 1000xxx10000mxxx */
						if (0 != mode(p)) {
							p->Cycles = (18 * kCycleScale
								+ 3 * RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles = (6 * kCycleScale
								+ RdAvgXtraCyc);
						}
						p->opsize = 1;
						if (mode(p) == 0) {
							if (CheckValidAddrMode(p, 0, reg(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 0, rg9(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindSbcd;
							}
						} else {
							if (CheckValidAddrMode(p, 4, reg(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 4, rg9(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindSbcd;
							}
						}
						break;
					case 1:
						/* PACK 1000rrr10100mrrr */
						if (mode(p) == 0) {
							p->opsize = 2;
							if (CheckValidAddrMode(p, 0, reg(p),
								kAddrValidAny, true))
							{
								p->opsize = 1;
								if (CheckValidAddrMode(p, 0, rg9(p),
									kAddrValidAny, false))
								{
									p->MainClass = kIKindPack;
								}
							}
						} else {
							p->opsize = 2;
							if (CheckValidAddrMode(p, 4, reg(p),
								kAddrValidAny, true))
							{
								p->opsize = 1;
								if (CheckValidAddrMode(p, 4, rg9(p),
									kAddrValidAny, false))
								{
									p->MainClass = kIKindPack;
								}
							}
						}
						break;
					case 2:
						/* UNPK 1000rrr11000mrrr */
						if (mode(p) == 0) {
							p->opsize = 1;
							if (CheckValidAddrMode(p, 0, reg(p),
								kAddrValidAny, true))
							{
								p->opsize = 2;
								if (CheckValidAddrMode(p, 0, rg9(p),
									kAddrValidAny, false))
								{
									p->MainClass = kIKindUnpk;
								}
							}
						} else {
							p->opsize = 1;
							if (CheckValidAddrMode(p, 4, reg(p),
								kAddrValidAny, true))
							{
								p->opsize = 2;
								if (CheckValidAddrMode(p, 4, rg9(p),
									kAddrValidAny, false))
								{
									p->MainClass = kIKindUnpk;
								}
							}
						}
						break;
					default:
						p->MainClass = kIKindIllegal;
						break;
				}
			} else {
				/* OR 1000ddd1ssmmmrrr */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, mode(p), reg(p),
					kAddrValidAltMem, false))
				{
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindOrDEa;
				}
			}
		}
	}
}

static inline void DeCode9(WorkR *p)
{
	if (b76(p) == 3) {
		/* SUBA 1001dddm11mmmrrr */
		p->opsize = b8(p) * 2 + 2;
		SetDcoArgFields(p, false, kAMdRegL, rg9(p) + 8);
			/* always long, regardless of opsize */
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		{
			if (4 == p->opsize) {
				if ((mode(p) < 2) || ((7 == mode(p)) && (reg(p) == 4)))
				{
					p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
				} else {
					p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
				}
			} else {
				p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
			}
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->MainClass = kIKindSubA;
		}
	} else {
		if (b8(p) == 0) {
			/* SUB 1001ddd0ssmmmrrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, mode(p), reg(p),
				kAddrValidAny, true))
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, false))
			{
				if (4 == p->opsize) {
					if ((mode(p) < 2)
						|| ((7 == mode(p)) && (reg(p) == 4)))
					{
						p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
					} else {
						p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
					}
				} else {
					p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindSubB + OpSizeOffset(p);
			}
		} else {
			if (mode(p) == 0) {
				/* SUBX 1001ddd1ss000rrr */
				/* p->MainClass = kIKindSubXd; */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 0, reg(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, false))
				{
					p->Cycles = (4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (4 * kCycleScale + RdAvgXtraCyc);
					p->MainClass = kIKindSubXB + OpSizeOffset(p);
				}
			} else if (mode(p) == 1) {
				/* SUBX 1001ddd1ss001rrr */
				/* p->MainClass = kIKindSubXm; */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 4, reg(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, 4, rg9(p),
					kAddrValidAny, false))
				{
					p->Cycles = (4 == p->opsize)
						? (30 * kCycleScale
							+ 5 * RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (18 * kCycleScale
							+ 3 * RdAvgXtraCyc + 1 * WrAvgXtraCyc);
					p->MainClass = kIKindSubXB + OpSizeOffset(p);
				}
			} else {
				/* SUB 1001ddd1ssmmmrrr */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, mode(p),
					reg(p), kAddrValidAltMem, false))
				{
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindSubB + OpSizeOffset(p);
				}
			}
		}
	}
}

static inline void DeCodeA(WorkR *p)
{
	p->Cycles = (34 * kCycleScale
		+ 4 * RdAvgXtraCyc + 3 * WrAvgXtraCyc);
	p->MainClass = kIKindA;
}

static inline void DeCodeB(WorkR *p)
{
	if (b76(p) == 3) {
		/* CMPA 1011ddds11mmmrrr */
		p->opsize = b8(p) * 2 + 2;
		SetDcoArgFields(p, false, kAMdRegL, rg9(p) + 8);
			/* always long, regardless of opsize */
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		{
			p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->MainClass = kIKindCmpA;
		}
	} else if (b8(p) == 1) {
		if (mode(p) == 1) {
			/* CmpM 1011ddd1ss001rrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, 3, reg(p),
				kAddrValidAny, true))
			if (CheckValidAddrMode(p, 3, rg9(p),
				kAddrValidAny, false))
			{
				p->Cycles = (4 == p->opsize)
					? (20 * kCycleScale + 5 * RdAvgXtraCyc)
					: (12 * kCycleScale + 3 * RdAvgXtraCyc);
				p->MainClass = kIKindCmpB + OpSizeOffset(p);
			}
		} else {
			/* Eor 1011ddd1ssmmmrrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, true))
			if (CheckValidAddrMode(p, mode(p), reg(p),
				kAddrValidDataAlt, false))
			{
				if (0 != mode(p)) {
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
				} else {
					p->Cycles = (4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (4 * kCycleScale + RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindEor;
			}
		}
	} else {
		/* Cmp 1011ddd0ssmmmrrr */
		FindOpSizeFromb76(p);
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		if (CheckValidAddrMode(p, 0, rg9(p),
			kAddrValidAny, false))
		{
			p->Cycles = (4 == p->opsize)
				? (6 * kCycleScale + RdAvgXtraCyc)
				: (4 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->MainClass = kIKindCmpB + OpSizeOffset(p);
		}
	}
}

static inline void DeCodeC(WorkR *p)
{
	if (b76(p) == 3) {
		p->opsize = 2;
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidData, true))
		if (CheckValidAddrMode(p, 0, rg9(p),
			kAddrValidAny, false))
		{
			p->Cycles = (38 * kCycleScale + RdAvgXtraCyc);
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			if (b8(p) == 0) {
				/* MulU 1100ddd011mmmrrr */
				p->MainClass = kIKindMulU;
			} else {
				/* MulS 1100ddd111mmmrrr */
				p->MainClass = kIKindMulS;
			}
		}
	} else {
		if (b8(p) == 0) {
			/* And 1100ddd0ssmmmrrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, mode(p), reg(p),
				kAddrValidData, true))
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, false))
			{
				if (4 == p->opsize) {
					if ((mode(p) < 2)
						|| ((7 == mode(p)) && (reg(p) == 4)))
					{
						p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
					} else {
						p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
					}
				} else {
					p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindAndEaD;
			}
		} else {
			if (mode(p) < 2) {
				switch (b76(p)) {
					case 0:
						/* ABCD 1100ddd10000mrrr */
						if (0 != mode(p)) {
							p->Cycles = (18 * kCycleScale
								+ 3 * RdAvgXtraCyc + WrAvgXtraCyc);
						} else {
							p->Cycles = (6 * kCycleScale
								+ RdAvgXtraCyc);
						}
						p->opsize = 1;
						if (mode(p) == 0) {
							if (CheckValidAddrMode(p, 0, reg(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 0, rg9(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindAbcd;
							}
						} else {
							if (CheckValidAddrMode(p, 4, reg(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 4, rg9(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindAbcd;
							}
						}
						break;
					case 1:
						/* Exg 1100ddd10100trrr */
						p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
						p->opsize = 4;
						if (mode(p) == 0) {
							if (CheckValidAddrMode(p, 0, rg9(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 0, reg(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindExg;
							}
						} else {
							if (CheckValidAddrMode(p, 1, rg9(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 1, reg(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindExg;
							}
						}
						break;
					case 2:
					default: /* keep compiler happy */
						if (mode(p) == 0) {
							p->MainClass = kIKindIllegal;
						} else {
							/* Exg 1100ddd110001rrr */
							p->Cycles = (6 * kCycleScale
								+ RdAvgXtraCyc);
							if (CheckValidAddrMode(p, 0, rg9(p),
								kAddrValidAny, true))
							if (CheckValidAddrMode(p, 1, reg(p),
								kAddrValidAny, false))
							{
								p->MainClass = kIKindExg;
							}
						}
						break;
				}
			} else {
				/* And 1100ddd1ssmmmrrr */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, mode(p), reg(p),
					kAddrValidAltMem, false))
				{
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindAndDEa;
				}
			}
		}
	}
}

static inline void DeCodeD(WorkR *p)
{
	if (b76(p) == 3) {
		/* ADDA 1101dddm11mmmrrr */
		p->opsize = b8(p) * 2 + 2;
		SetDcoArgFields(p, false, kAMdRegL, rg9(p) + 8);
			/* always long, regardless of opsize */
		if (CheckValidAddrMode(p, mode(p), reg(p),
			kAddrValidAny, true))
		{
			if (4 == p->opsize) {
				if ((mode(p) < 2) || ((7 == mode(p)) && (reg(p) == 4)))
				{
					p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
				} else {
					p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
				}
			} else {
				p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
			}
			p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
			p->MainClass = kIKindAddA;
		}
	} else {
		if (b8(p) == 0) {
			/* ADD 1101ddd0ssmmmrrr */
			FindOpSizeFromb76(p);
			if (CheckValidAddrMode(p, mode(p), reg(p),
				kAddrValidAny, true))
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, false))
			{
				if (4 == p->opsize) {
					if ((mode(p) < 2)
						|| ((7 == mode(p)) && (reg(p) == 4)))
					{
						p->Cycles = (8 * kCycleScale + RdAvgXtraCyc);
					} else {
						p->Cycles = (6 * kCycleScale + RdAvgXtraCyc);
					}
				} else {
					p->Cycles = (4 * kCycleScale + RdAvgXtraCyc);
				}
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = kIKindAddB + OpSizeOffset(p);
			}
		} else {
			if (mode(p) == 0) {
				/* ADDX 1101ddd1ss000rrr */
				/* p->MainClass = kIKindAddXd; */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 0, reg(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, false))
				{
					p->Cycles = (4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (4 * kCycleScale + RdAvgXtraCyc);
					p->MainClass = kIKindAddXB + OpSizeOffset(p);
				}
			} else if (mode(p) == 1) {
				/* p->MainClass = kIKindAddXm; */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 4, reg(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, 4, rg9(p),
					kAddrValidAny, false))
				{
					p->Cycles = (4 == p->opsize)
						? (30 * kCycleScale
							+ 5 * RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (18 * kCycleScale
							+ 3 * RdAvgXtraCyc + 1 * WrAvgXtraCyc);
					p->MainClass = kIKindAddXB + OpSizeOffset(p);
				}
			} else {
				/* ADD 1101ddd1ssmmmrrr */
				FindOpSizeFromb76(p);
				if (CheckValidAddrMode(p, 0, rg9(p),
					kAddrValidAny, true))
				if (CheckValidAddrMode(p, mode(p), reg(p),
					kAddrValidAltMem, false))
				{
					p->Cycles = (4 == p->opsize)
						? (12 * kCycleScale
							+ RdAvgXtraCyc + 2 * WrAvgXtraCyc)
						: (8 * kCycleScale
							+ RdAvgXtraCyc + WrAvgXtraCyc);
					p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
					p->MainClass = kIKindAddB + OpSizeOffset(p);
				}
			}
		}
	}
}

static uint32_t rolops(WorkR *p, uint32_t x)
{
	uint32_t binop;

	binop = (x << 1);
	if (! b8(p)) {
		binop++; /* 'R' */
	} /* else 'L' */

	return kIKindAslB + 3 * binop + OpSizeOffset(p);
}

static inline void DeCodeE(WorkR *p)
{
	if (b76(p) == 3) {
		if ((p->opcode & 0x0800) != 0) {
			/* 11101???11mmmrrr */
			p->DecOp.y.v[0].AMd = mode(p);
			p->DecOp.y.v[0].ArgDat = (p->opcode >> 8) & 7;
			if (0 == mode(p)) {
				SetDcoArgFields(p, false, kAMdRegL, reg(p));
				p->MainClass = kIKindBitField;
			} else {
				switch ((p->opcode >> 8) & 7) {
					case 0: /* BFTST */
					case 1: /* BFEXTU */
					case 3: /* BFEXTS */
					case 5: /* BFFFO */
						if (CheckControlAddrMode(p)) {
							p->MainClass = kIKindBitField;
						}
						break;
					default: /* BFCHG, BFCLR, BFSET, BFINS */
						if (CheckControlAltAddrMode(p)) {
							p->MainClass = kIKindBitField;
						}
						break;
				}
			}
		} else {
			p->opsize = 2;
			/* 11100ttd11mmmddd */
			if (CheckAltMemAddrMode(p)) {
				p->Cycles = (6 * kCycleScale
					+ RdAvgXtraCyc + WrAvgXtraCyc);
				p->Cycles += OpEACalcCyc(p, mode(p), reg(p));
				p->MainClass = rolops(p, rg9(p));
				SetDcoArgFields(p, true, kAMdDat4, 1);
			}
		}
	} else {
		FindOpSizeFromb76(p);
		if (mode(p) < 4) {
			/* 1110cccdss0ttddd */
			if (CheckValidAddrMode(p, 0, reg(p),
				kAddrValidAny, false))
			{
				p->Cycles = ((4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (6 * kCycleScale + RdAvgXtraCyc))
					;
				p->MainClass = rolops(p, mode(p) & 3);
				SetDcoArgFields(p, true, kAMdDat4, octdat(rg9(p)));
			}
		} else {
			/* 1110rrrdss1ttddd */
			if (CheckValidAddrMode(p, 0, rg9(p),
				kAddrValidAny, true))
			if (CheckValidAddrMode(p, 0, reg(p),
				kAddrValidAny, false))
			{
				p->Cycles = ((4 == p->opsize)
						? (8 * kCycleScale + RdAvgXtraCyc)
						: (6 * kCycleScale + RdAvgXtraCyc));
				p->MainClass = rolops(p, mode(p) & 3);
			}
		}
	}
}

static inline void DeCodeF(WorkR *p)
{
	p->Cycles =
		(34 * kCycleScale + 4 * RdAvgXtraCyc + 3 * WrAvgXtraCyc);
	p->DecOp.y.v[0].AMd =    (p->opcode >> 8) & 0xFF;
	p->DecOp.y.v[0].ArgDat = (p->opcode     ) & 0xFF;
	switch (rg9(p)) {
		case 0:
			p->MainClass = kIKindMMU;
			break;
		case 1:
			switch (md6(p)) {
				case 0:
					p->MainClass = kIKindFPUmd60;
					break;
				case 1:
					if (mode(p) == 1) {
						p->MainClass = kIKindFPUDBcc;
					} else if (mode(p) == 7) {
						p->MainClass = kIKindFPUTrapcc;
					} else {
						p->MainClass = kIKindFPUScc;
					}
					break;
				case 2:
					p->MainClass = kIKindFPUFBccW;
					break;
				case 3:
					p->MainClass = kIKindFPUFBccL;
					break;
				case 4:
					p->MainClass = kIKindFPUSave;
					break;
				case 5:
					p->MainClass = kIKindFPURestore;
					break;
				default:
					p->MainClass = kIKindFPUdflt;
					break;
			}
			break;
		default:
			p->MainClass = kIKindFdflt;
			break;
	}
}

/* Dispatch opcode to one of the 16 class decoders (0-F) and
   finalise the decoded instruction record. */
static void DeCodeOneOp(WorkR *p)
{
	switch (p->opcode >> 12) {
		case 0x0:
			DeCode0(p);
			break;
		case 0x1:
			DeCode1(p);
			break;
		case 0x2:
			DeCode2(p);
			break;
		case 0x3:
			DeCode3(p);
			break;
		case 0x4:
			DeCode4(p);
			break;
		case 0x5:
			DeCode5(p);
			break;
		case 0x6:
			DeCode6(p);
			break;
		case 0x7:
			DeCode7(p);
			break;
		case 0x8:
			DeCode8(p);
			break;
		case 0x9:
			DeCode9(p);
			break;
		case 0xA:
			DeCodeA(p);
			break;
		case 0xB:
			DeCodeB(p);
			break;
		case 0xC:
			DeCodeC(p);
			break;
		case 0xD:
			DeCodeD(p);
			break;
		case 0xE:
			DeCodeE(p);
			break;
		case 0xF:
		default: /* keep compiler happy */
			DeCodeF(p);
			break;
	}

	if (kIKindIllegal == p->MainClass) {
		p->Cycles = (34 * kCycleScale
			+ 4 * RdAvgXtraCyc + 3 * WrAvgXtraCyc);
		p->DecOp.y.v[0].AMd = 0;
		p->DecOp.y.v[0].ArgDat = 0;
		p->DecOp.y.v[1].AMd = 0;
		p->DecOp.y.v[1].ArgDat = 0;
	}

	SetDcoMainClas(&(p->DecOp), p->MainClass);
	SetDcoCycles(&(p->DecOp), p->Cycles);
}

static bool is68020OnlyKind(uint8_t kind)
{
	switch (kind) {
		case kIKindCallMorRtm:
		case kIKindBraL:
		case kIKindBccL:
		case kIKindBsrL:
		case kIKindEXTBL:
		case kIKindTRAPcc:
		case kIKindChkL:
		case kIKindBkpt:
		case kIKindDivL:
		case kIKindMulL:
		case kIKindRtd:
		case kIKindMoveCCREa:
		case kIKindMoveCEa:
		case kIKindMoveEaC:
		case kIKindLinkL:
		case kIKindPack:
		case kIKindUnpk:
		case kIKindCHK2orCMP2:
		case kIKindCAS2:
		case kIKindCAS:
		case kIKindMoveS:
		case kIKindBitField:
			return true;
		default:
			return false;
	}
}

static bool isMMUKind(uint8_t kind)
{
	return kind == kIKindMMU;
}

static bool isFPUKind(uint8_t kind)
{
	switch (kind) {
		case kIKindFPUmd60:
		case kIKindFPUDBcc:
		case kIKindFPUTrapcc:
		case kIKindFPUScc:
		case kIKindFPUFBccW:
		case kIKindFPUFBccL:
		case kIKindFPUSave:
		case kIKindFPURestore:
			return true;
		default:
			return false;
	}
}

/* Build the full 64K instruction decode table, then patch out
   instructions not supported by the configured CPU model. */
void M68KITAB_setup(DecOpR *p, const MachineConfig *config)
{
	uint32_t i;
	WorkR r;

	for (i = 0; i < (uint32_t)256 * 256; ++i) {
		r.opcode = i;
		r.MainClass = kIKindIllegal;

		r.DecOp.y.v[0].AMd = 0;
		r.DecOp.y.v[0].ArgDat = 0;
		r.DecOp.y.v[1].AMd = 0;
		r.DecOp.y.v[1].ArgDat = 0;
		r.Cycles = kMyAvgCycPerInstr;

		DeCodeOneOp(&r);

		p[i] = r.DecOp;
	}

	/* Runtime fixup: disable instruction kinds not supported by the
	   configured CPU. The table was built with all features enabled
	   (USE_68020=1, EM_FPU=1, EM_MMU=1). Now patch out unsupported ones.

	   68020-only instructions → kIKindIllegal (Exception 4).
	   Coprocessor (FPU/MMU) instructions → kIKindFdflt (Exception 0xB,
	   F-line). The ROM's F-line handler deals with unimplemented
	   coprocessor instructions gracefully; an Illegal Instruction
	   exception would crash the boot.

	   IMPORTANT: we must also patch the Cycles field.  The table was
	   built with cycle costs for the *real* handler (e.g. MOVEC costs).
	   After re-classifying to kIKindIllegal / kIKindFdflt the cycle
	   cost must match what DeCodeOneOp would have assigned for that
	   kind originally, otherwise timing diverges from a build where the
	   instruction was never decoded at all. */
	if (config) {
		const uint16_t illegalCycles = (uint16_t)(34 * kCycleScale
			+ 4 * RdAvgXtraCyc + 3 * WrAvgXtraCyc);
		/* Must match DeCodeF's cycle cost so that a runtime-disabled
		   FPU/MMU instruction costs the same as when the feature was
		   compile-time disabled (i.e. the opcode fell through to
		   kIKindFdflt in DeCodeF and inherited its Cycles). */
		const uint16_t fdfltCycles = illegalCycles;
		for (i = 0; i < (uint32_t)256 * 256; ++i) {
			uint8_t kind = p[i].x.MainClas;
			if (!config->use68020 && is68020OnlyKind(kind)) {
				SetDcoMainClas(&p[i], kIKindIllegal);
				SetDcoCycles(&p[i], illegalCycles);
			}
			if (!config->emFPU && isFPUKind(kind)) {
				SetDcoMainClas(&p[i], kIKindFdflt);
				SetDcoCycles(&p[i], fdfltCycles);
			}
			if (!config->emMMU && isMMUKind(kind)) {
				SetDcoMainClas(&p[i], kIKindFdflt);
				SetDcoCycles(&p[i], fdfltCycles);
			}
		}
	}
}
