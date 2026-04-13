/*
	DISAssemble Motorola 68K instructions.

	Rewritten to output to std::string instead of dbglog.
	The old DisasmOneOrSave / m68k_WantDisasmContext API is preserved
	at the bottom, implemented on top of Disassemble().
*/

#include "core/common.h"

#include "cpu/m68k_tables.h"

#include "cpu/disasm.h"

#include <string>
#include <cstdio>

/* -- output buffer ---- */

static std::string *s_out = nullptr;

static void out_str(const char *s)
{
	s_out->append(s);
}
static void out_hex(uint32_t x)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%X", x);
	s_out->append(buf);
}

/* -- PC and memory access ---- */

static uint32_t Disasm_pc;

/*
	don't use get_vm_byte/get_vm_word/get_vm_long
		so as to be sure of no side effects
		(if pc points to memory mapped device)
*/

static uint8_t *Disasm_pcp;
static uint32_t Disasm_pc_blockmask;
static uint8_t Disasm_pcp_dummy[2] = {0, 0};

extern ATTEntryPtr FindATTel(uint32_t addr);

static void Disasm_Find_pcp()
{
	ATTEntryPtr p;

	p = FindATTel(Disasm_pc);
	if (0 == (p->Access & kATTA_readreadymask))
	{
		Disasm_pcp = Disasm_pcp_dummy;
		Disasm_pc_blockmask = 0;
	}
	else
	{
		Disasm_pc_blockmask = p->usemask & ~p->cmpmask;
		Disasm_pc_blockmask = Disasm_pc_blockmask & ~(Disasm_pc_blockmask + 1);
		Disasm_pcp = p->usebase + (Disasm_pc & p->usemask);
	}
}

static uint16_t Disasm_nextiword()
{
	uint16_t r = do_get_mem_word(Disasm_pcp);
	Disasm_pcp += 2;
	Disasm_pc += 2;
	if (0 == (Disasm_pc_blockmask & Disasm_pc))
	{
		Disasm_Find_pcp();
	}
	return r;
}

static inline uint8_t Disasm_nextibyte()
{
	return (uint8_t)Disasm_nextiword();
}

static uint32_t Disasm_nextilong()
{
	uint32_t hi = Disasm_nextiword();
	uint32_t lo = Disasm_nextiword();
	uint32_t r = ((hi << 16) & 0xFFFF0000) | (lo & 0x0000FFFF);

	return r;
}

static void Disasm_setpc(uint32_t newpc)
{
	if (newpc != Disasm_pc)
	{
		Disasm_pc = newpc;

		Disasm_Find_pcp();
	}
}

/* -- opcode field extraction ---- */

static uint32_t Disasm_opcode;
static uint32_t Disasm_opsize;

#define Disasm_b76 ((Disasm_opcode >> 6) & 3)
#define Disasm_b8 ((Disasm_opcode >> 8) & 1)
#define Disasm_mode ((Disasm_opcode >> 3) & 7)
#define Disasm_reg (Disasm_opcode & 7)
#define Disasm_md6 ((Disasm_opcode >> 6) & 7)
#define Disasm_rg9 ((Disasm_opcode >> 9) & 7)

/* -- helpers ---- */

static void DisasmOpSizeFromb76()
{
	Disasm_opsize = 1 << Disasm_b76;
	switch (Disasm_opsize)
	{
		case 1:
			out_str(".B");
			break;
		case 2:
			out_str(".W");
			break;
		case 4:
			out_str(".L");
			break;
	}
}

static void DisasmModeRegister(uint32_t themode, uint32_t thereg)
{
	switch (themode)
	{
		case 0:
			out_str("D");
			out_hex(thereg);
			break;
		case 1:
			out_str("A");
			out_hex(thereg);
			break;
		case 2:
			out_str("(A");
			out_hex(thereg);
			out_str(")");
			break;
		case 3:
			out_str("(A");
			out_hex(thereg);
			out_str(")+");
			break;
		case 4:
			out_str("-(A");
			out_hex(thereg);
			out_str(")");
			break;
		case 5:
			out_hex(Disasm_nextiword());
			out_str("(A");
			out_hex(thereg);
			out_str(")");
			break;
		case 6:
			out_str("???");
			break;
		case 7:
			switch (thereg)
			{
				case 0:
					out_str("(");
					out_hex(Disasm_nextiword());
					out_str(")");
					break;
				case 1:
					out_str("(");
					out_hex(Disasm_nextilong());
					out_str(")");
					break;
				case 2:
				{
					uint32_t s = Disasm_pc;
					s += static_cast<uint32_t>(static_cast<int16_t>(Disasm_nextiword()));
					out_str("(");
					out_hex(s);
					out_str(")");
				}
				break;
				case 3:
					out_str("???");
					break;
				case 4:
					out_str("#");
					if (Disasm_opsize == 2)
					{
						out_hex(Disasm_nextiword());
					}
					else if (Disasm_opsize < 2)
					{
						out_hex(Disasm_nextibyte());
					}
					else
					{
						out_hex(Disasm_nextilong());
					}
					break;
			}
			break;
		case 8:
			out_str("#");
			out_hex(thereg);
			break;
	}
}

static void DisasmStartOne(const char *s)
{
	out_str(s);
}

/* -- common patterns ---- */

static void Disasm_xxxxxxxxssmmmrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static void DisasmEaD_xxxxdddxssmmmrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", D");
	out_hex(Disasm_rg9);
}

static void DisasmI_xxxxxxxxssmmmrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" #");
	if (Disasm_opsize == 2)
	{
		out_hex(static_cast<uint32_t>(static_cast<int16_t>(Disasm_nextiword())));
	}
	else if (Disasm_opsize < 2)
	{
		out_hex(static_cast<uint32_t>(static_cast<int8_t>(Disasm_nextibyte())));
	}
	else
	{
		out_hex(static_cast<uint32_t>(Disasm_nextilong()));
	}
	out_str(", ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static void DisasmsAA_xxxxdddxssxxxrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	DisasmModeRegister(3, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(3, Disasm_rg9);
}

static uint32_t Disasm_octdat(uint32_t x)
{
	if (x == 0)
	{
		return 8;
	}
	else
	{
		return x;
	}
}

static void Disasm_xxxxnnnxssmmmrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" #");
	out_hex(Disasm_octdat(Disasm_rg9));
	out_str(", ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static void DisasmDEa_xxxxdddxssmmmrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" D");
	out_hex(Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static void DisasmEaA_xxxxdddsxxmmmrrr(const char *s)
{
	DisasmStartOne(s);

	Disasm_opsize = Disasm_b8 * 2 + 2;
	if (Disasm_opsize == 2)
	{
		out_str(".W");
	}
	else
	{
		out_str(".L");
	}
	out_str(" ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", A");
	out_hex(Disasm_rg9);
}

static void DisasmDD_xxxxdddxssxxxrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" ");
	DisasmModeRegister(0, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(0, Disasm_rg9);
}

static void DisasmAAs_xxxxdddxssxxxrrr(const char *s)
{
	DisasmStartOne(s);
	DisasmOpSizeFromb76();
	out_str(" ");
	DisasmModeRegister(4, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(4, Disasm_rg9);
}

/* -- instruction handlers ---- */

static inline void DisasmTst()
{
	Disasm_xxxxxxxxssmmmrrr("TST");
}
static inline void DisasmCompare()
{
	DisasmEaD_xxxxdddxssmmmrrr("CMP");
}
static inline void DisasmCmpI()
{
	DisasmI_xxxxxxxxssmmmrrr("CMP");
}
static inline void DisasmCmpM()
{
	DisasmsAA_xxxxdddxssxxxrrr("CMP");
}

static void DisasmCC()
{
	static const char *const cc_names[16] = {"T",  "F",	 "HI", "LS", "CC", "CS", "NE", "EQ",
											 "VC", "VS", "P",  "MI", "GE", "LT", "GT", "LE"};
	out_str(cc_names[(Disasm_opcode >> 8) & 15]);
}

static inline void DisasmBcc()
{
	uint32_t src = ((uint32_t)Disasm_opcode) & 255;
	uint32_t s = Disasm_pc;

	if (0 == ((Disasm_opcode >> 8) & 15))
	{
		DisasmStartOne("BRA");
	}
	else
	{
		DisasmStartOne("B");
		DisasmCC();
	}
	out_str(" ");

	if (src == 0)
	{
		s += static_cast<uint32_t>(static_cast<int16_t>(Disasm_nextiword()));
	}
	else if (src == 255)
	{
		s += static_cast<uint32_t>(Disasm_nextilong());
	}
	else
	{
		s += static_cast<uint32_t>(static_cast<int8_t>(src));
	}
	out_hex(s);
}

static inline void DisasmDBcc()
{
	uint32_t s = Disasm_pc;

	DisasmStartOne("DB");
	DisasmCC();

	out_str(" D");
	out_hex(Disasm_reg);
	out_str(", ");

	s += (int32_t)(int16_t)Disasm_nextiword();
	out_hex(s);
}

static inline void DisasmSwap()
{
	DisasmStartOne("SWAP D");
	out_hex(Disasm_reg);
}

static void DisasmMove()
{
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(Disasm_md6, Disasm_rg9);
}

static inline void DisasmMoveL()
{
	DisasmStartOne("MOVE.L ");
	Disasm_opsize = 4;
	DisasmMove();
}
static inline void DisasmMoveW()
{
	DisasmStartOne("MOVE.W ");
	Disasm_opsize = 2;
	DisasmMove();
}
static inline void DisasmMoveB()
{
	DisasmStartOne("MOVE.B ");
	Disasm_opsize = 1;
	DisasmMove();
}
static inline void DisasmMoveAL()
{
	DisasmStartOne("MOVEA.L ");
	Disasm_opsize = 4;
	DisasmMove();
}
static inline void DisasmMoveAW()
{
	DisasmStartOne("MOVEA.W ");
	Disasm_opsize = 2;
	DisasmMove();
}

static inline void DisasmMoveQ()
{
	DisasmStartOne("MOVEQ #");
	out_hex(static_cast<uint32_t>(static_cast<int8_t>(Disasm_opcode)));
	out_str(", D");
	out_hex(Disasm_rg9);
}

static inline void DisasmAddEaR()
{
	DisasmEaD_xxxxdddxssmmmrrr("ADD");
}
static inline void DisasmAddQ()
{
	Disasm_xxxxnnnxssmmmrrr("ADDQ");
}
static inline void DisasmAddI()
{
	DisasmI_xxxxxxxxssmmmrrr("ADDI");
}
static inline void DisasmAddREa()
{
	DisasmDEa_xxxxdddxssmmmrrr("ADD");
}
static inline void DisasmSubEaR()
{
	DisasmEaD_xxxxdddxssmmmrrr("SUB");
}
static inline void DisasmSubQ()
{
	Disasm_xxxxnnnxssmmmrrr("SUBQ");
}
static inline void DisasmSubI()
{
	DisasmI_xxxxxxxxssmmmrrr("SUBI");
}
static inline void DisasmSubREa()
{
	DisasmDEa_xxxxdddxssmmmrrr("SUB");
}

static inline void DisasmLea()
{
	DisasmStartOne("LEA ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", A");
	out_hex(Disasm_rg9);
}

static inline void DisasmPEA()
{
	DisasmStartOne("PEA ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmALine()
{
	DisasmStartOne("$");
	out_hex(Disasm_opcode);
}

static inline void DisasmBsr()
{
	uint32_t src = ((uint32_t)Disasm_opcode) & 255;
	uint32_t s = Disasm_pc;

	DisasmStartOne("BSR ");
	if (src == 0)
	{
		s += (int32_t)(int16_t)Disasm_nextiword();
	}
	else if (src == 255)
	{
		s += (int32_t)Disasm_nextilong();
	}
	else
	{
		s += (int32_t)(int8_t)src;
	}
	out_hex(s);
}

static inline void DisasmJsr()
{
	DisasmStartOne("JSR ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmLinkA6()
{
	DisasmStartOne("LINK A6, ");
	out_hex(Disasm_nextiword());
}

static inline void DisasmMOVEMRmM()
{
	int16_t z;
	uint32_t regmask;

	DisasmStartOne("MOVEM");
	if (Disasm_b76 == 2)
	{
		out_str(".W");
	}
	else
	{
		out_str(".L");
	}
	out_str(" ");
	regmask = Disasm_nextiword();

	for (z = 16; --z >= 0;)
	{
		if ((regmask & (1 << (15 - z))) != 0)
		{
			if (z >= 8)
			{
				out_str("A");
				out_hex(z - 8);
			}
			else
			{
				out_str("D");
				out_hex(z);
			}
		}
	}
	out_str(", -(A");
	out_hex(Disasm_reg);
	out_str(")");
}

static inline void DisasmMOVEMApR()
{
	int16_t z;
	uint32_t regmask;

	regmask = Disasm_nextiword();

	DisasmStartOne("MOVEM");
	if (Disasm_b76 == 2)
	{
		out_str(".W");
	}
	else
	{
		out_str(".L");
	}
	out_str(" (A");
	out_hex(Disasm_reg);
	out_str(")+, ");

	for (z = 0; z < 16; ++z)
	{
		if ((regmask & (1 << z)) != 0)
		{
			if (z >= 8)
			{
				out_str("A");
				out_hex(z - 8);
			}
			else
			{
				out_str("D");
				out_hex(z);
			}
		}
	}
}

static inline void DisasmUnlkA6()
{
	DisasmStartOne("UNLINK A6");
}
static inline void DisasmRts()
{
	DisasmStartOne("RTS");
}

static inline void DisasmJmp()
{
	DisasmStartOne("JMP ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmClr()
{
	Disasm_xxxxxxxxssmmmrrr("CLR");
}
static inline void DisasmAddA()
{
	DisasmEaA_xxxxdddsxxmmmrrr("ADDA");
}

static inline void DisasmAddQA()
{
	DisasmStartOne("ADDQA #");
	out_hex(Disasm_octdat(Disasm_rg9));
	out_str(", A");
	out_hex(Disasm_reg);
}

static inline void DisasmSubQA()
{
	DisasmStartOne("SUBQA #");
	out_hex(Disasm_octdat(Disasm_rg9));
	out_str(", A");
	out_hex(Disasm_reg);
}

static inline void DisasmSubA()
{
	DisasmEaA_xxxxdddsxxmmmrrr("SUBA");
}

static inline void DisasmCmpA()
{
	DisasmStartOne("CMPA ");
	Disasm_opsize = Disasm_b8 * 2 + 2;
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", A");
	out_hex(Disasm_rg9);
}

static inline void DisasmAddXd()
{
	DisasmDD_xxxxdddxssxxxrrr("ADDX");
}
static inline void DisasmAddXm()
{
	DisasmAAs_xxxxdddxssxxxrrr("ADDX");
}
static inline void DisasmSubXd()
{
	DisasmDD_xxxxdddxssxxxrrr("SUBX");
}
static inline void DisasmSubXm()
{
	DisasmAAs_xxxxdddxssxxxrrr("SUBX");
}

static void DisasmBinOp1(uint32_t x)
{
	static const char *const shift_ops[2][4] = {{"ASR", "LSR", "RXR", "ROR"},
												{"ASL", "LSL", "RXL", "ROL"}};
	if (x < 4)
	{
		DisasmStartOne(shift_ops[Disasm_b8 ? 1 : 0][x]);
	}
}

static inline void DisasmRolopNM()
{
	DisasmBinOp1(Disasm_rg9);
	out_str(" ");
	Disasm_opsize = 2;
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmRolopND()
{
	DisasmBinOp1(Disasm_mode & 3);
	DisasmOpSizeFromb76();
	out_str(" #");
	out_hex(Disasm_octdat(Disasm_rg9));
	out_str(", ");
	DisasmModeRegister(0, Disasm_reg);
}

static inline void DisasmRolopDD()
{
	DisasmBinOp1(Disasm_mode & 3);
	DisasmOpSizeFromb76();
	out_str(" ");
	DisasmModeRegister(0, Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(0, Disasm_reg);
}

static void DisasmBinBitOp1()
{
	static const char *const bit_ops[4] = {"BTST", "BCHG", "BCLR", "BSET"};
	if (Disasm_b76 < 4)
	{
		DisasmStartOne(bit_ops[Disasm_b76]);
	}
}

static inline void DisasmBitOpDD()
{
	DisasmBinBitOp1();
	Disasm_opsize = 4;
	out_str(" ");
	DisasmModeRegister(0, Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(0, Disasm_reg);
}

static inline void DisasmBitOpDM()
{
	DisasmBinBitOp1();
	Disasm_opsize = 1;
	out_str(" ");
	DisasmModeRegister(0, Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmBitOpND()
{
	DisasmBinBitOp1();
	Disasm_opsize = 4;
	out_str(" #");
	out_hex(static_cast<uint32_t>(static_cast<int8_t>(Disasm_nextibyte())));
	out_str(", ");
	DisasmModeRegister(0, Disasm_reg);
}

static inline void DisasmBitOpNM()
{
	DisasmBinBitOp1();
	Disasm_opsize = 1;
	out_str(" #");
	out_hex(static_cast<uint32_t>(static_cast<int8_t>(Disasm_nextibyte())));
	out_str(", ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmAndI()
{
	DisasmI_xxxxxxxxssmmmrrr("ANDI");
}
static inline void DisasmAndDEa()
{
	DisasmDEa_xxxxdddxssmmmrrr("AND");
}
static inline void DisasmAndEaD()
{
	DisasmEaD_xxxxdddxssmmmrrr("AND");
}
static inline void DisasmOrI()
{
	DisasmI_xxxxxxxxssmmmrrr("ORI");
}
static inline void DisasmOrDEa()
{
	DisasmDEa_xxxxdddxssmmmrrr("OR");
}
static inline void DisasmOrEaD()
{
	DisasmEaD_xxxxdddxssmmmrrr("OR");
}
static inline void DisasmEorI()
{
	DisasmI_xxxxxxxxssmmmrrr("EORI");
}
static inline void DisasmEor()
{
	DisasmDEa_xxxxdddxssmmmrrr("EOR");
}
static inline void DisasmNot()
{
	Disasm_xxxxxxxxssmmmrrr("NOT");
}

static inline void DisasmScc()
{
	Disasm_opsize = 1;
	DisasmStartOne("S");
	DisasmCC();
	out_str(" ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmEXTL()
{
	DisasmStartOne("EXT.L D");
	out_hex(Disasm_reg);
}
static inline void DisasmEXTW()
{
	DisasmStartOne("EXT.W D");
	out_hex(Disasm_reg);
}

static inline void DisasmNeg()
{
	Disasm_xxxxxxxxssmmmrrr("NEG");
}
static inline void DisasmNegX()
{
	Disasm_xxxxxxxxssmmmrrr("NEGX");
}

static inline void DisasmMulU()
{
	Disasm_opsize = 2;
	DisasmStartOne("MULU ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(0, Disasm_rg9);
}

static inline void DisasmMulS()
{
	Disasm_opsize = 2;
	DisasmStartOne("MULS ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(0, Disasm_rg9);
}

static inline void DisasmDivU()
{
	Disasm_opsize = 2;
	DisasmStartOne("DIVU ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(0, Disasm_rg9);
}

static inline void DisasmDivS()
{
	Disasm_opsize = 2;
	DisasmStartOne("DIVS ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(0, Disasm_rg9);
}

static inline void DisasmExgdd()
{
	Disasm_opsize = 4;
	DisasmStartOne("EXG ");
	DisasmModeRegister(0, Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(0, Disasm_reg);
}

static inline void DisasmExgaa()
{
	Disasm_opsize = 4;
	DisasmStartOne("EXG ");
	DisasmModeRegister(1, Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(1, Disasm_reg);
}

static inline void DisasmExgda()
{
	Disasm_opsize = 4;
	DisasmStartOne("EXG ");
	DisasmModeRegister(0, Disasm_rg9);
	out_str(", ");
	DisasmModeRegister(1, Disasm_reg);
}

static inline void DisasmMoveCCREa()
{
	Disasm_opsize = 2;
	DisasmStartOne("MOVE CCR, ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmMoveEaCR()
{
	Disasm_opsize = 2;
	DisasmStartOne("MOVE ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", CCR");
}

static inline void DisasmMoveSREa()
{
	Disasm_opsize = 2;
	DisasmStartOne("MOVE SR, ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmMoveEaSR()
{
	Disasm_opsize = 2;
	DisasmStartOne("MOVE ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", SR");
}

static void DisasmBinOpStatusCCR()
{
	switch (Disasm_rg9)
	{
		case 0:
			DisasmStartOne("OR");
			break;
		case 1:
			DisasmStartOne("AND");
			break;
		case 5:
			DisasmStartOne("EOR");
			break;
		default:
			break;
	}
	DisasmOpSizeFromb76();
	out_str(" #");
	out_hex(static_cast<uint32_t>(static_cast<int16_t>(Disasm_nextiword())));
	if (Disasm_b76 != 0)
	{
		out_str(", SR");
	}
	else
	{
		out_str(", CCR");
	}
}

static void disasmreglist(int16_t direction, uint32_t m1, uint32_t r1)
{
	int16_t z;
	uint32_t regmask;

	DisasmStartOne("MOVEM");

	regmask = Disasm_nextiword();
	Disasm_opsize = 2 * Disasm_b76 - 2;

	if (Disasm_opsize == 2)
	{
		out_str(".W");
	}
	else
	{
		out_str(".L");
	}

	out_str(" ");

	if (direction != 0)
	{
		DisasmModeRegister(m1, r1);
		out_str(", ");
	}

	for (z = 0; z < 16; ++z)
	{
		if ((regmask & (1 << z)) != 0)
		{
			if (z >= 8)
			{
				out_str("A");
				out_hex(z - 8);
			}
			else
			{
				out_str("D");
				out_hex(z);
			}
		}
	}

	if (direction == 0)
	{
		out_str(", ");
		DisasmModeRegister(m1, r1);
	}
}

static inline void DisasmMOVEMrm()
{
	disasmreglist(0, Disasm_mode, Disasm_reg);
}
static inline void DisasmMOVEMmr()
{
	disasmreglist(1, Disasm_mode, Disasm_reg);
}

static void DisasmByteBinOp(const char *s, uint32_t m1, uint32_t r1, uint32_t m2, uint32_t r2)
{
	DisasmStartOne(s);
	out_str(" ");
	DisasmOpSizeFromb76();
	DisasmModeRegister(m1, r1);
	out_str(", ");
	DisasmModeRegister(m2, r2);
}

static inline void DisasmAbcdr()
{
	DisasmByteBinOp("ABCD", 0, Disasm_reg, 0, Disasm_rg9);
}
static inline void DisasmAbcdm()
{
	DisasmByteBinOp("ABCD", 4, Disasm_reg, 4, Disasm_rg9);
}
static inline void DisasmSbcdr()
{
	DisasmByteBinOp("SBCD", 0, Disasm_reg, 0, Disasm_rg9);
}
static inline void DisasmSbcdm()
{
	DisasmByteBinOp("SBCD", 4, Disasm_reg, 4, Disasm_rg9);
}

static inline void DisasmNbcd()
{
	Disasm_xxxxxxxxssmmmrrr("NBCD");
}

static inline void DisasmRte()
{
	DisasmStartOne("RTE");
}
static inline void DisasmNop()
{
	DisasmStartOne("NOP");
}

static inline void DisasmMoveP()
{
	DisasmStartOne("MOVEP");
	if (0 == (Disasm_b76 & 1))
	{
		Disasm_opsize = 2;
		out_str(".W");
	}
	else
	{
		Disasm_opsize = 4;
		out_str(".L");
	}
	out_str(" ");
	if (Disasm_b76 < 2)
	{
		DisasmModeRegister(5, Disasm_reg);
		out_str(", ");
		DisasmModeRegister(0, Disasm_rg9);
	}
	else
	{
		DisasmModeRegister(0, Disasm_rg9);
		out_str(", ");
		DisasmModeRegister(5, Disasm_reg);
	}
}

static inline void DisasmIllegal()
{
	DisasmStartOne("ILLEGAL");
}

static void DisasmCheck()
{
	DisasmStartOne("CHK");
	if (2 == Disasm_opsize)
	{
		out_str(".W");
	}
	else
	{
		out_str(".L");
	}
	out_str(" ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
	out_str(", ");
	DisasmModeRegister(0, Disasm_rg9);
}

static inline void DisasmChkW()
{
	Disasm_opsize = 2;
	DisasmCheck();
}

static inline void DisasmTrap()
{
	DisasmStartOne("TRAP ");
	out_hex(Disasm_opcode & 15);
}

static inline void DisasmTrapV()
{
	DisasmStartOne("TRAPV");
}
static inline void DisasmRtr()
{
	DisasmStartOne("RTR");
}

static inline void DisasmLink()
{
	DisasmStartOne("LINK A");
	out_hex(Disasm_reg);
	out_str(", ");
	out_hex(Disasm_nextiword());
}

static inline void DisasmUnlk()
{
	DisasmStartOne("UNLINK A");
	out_hex(Disasm_reg);
}

static inline void DisasmMoveRUSP()
{
	DisasmStartOne("MOVE A");
	out_hex(Disasm_reg);
	out_str(", USP");
}

static inline void DisasmMoveUSPR()
{
	DisasmStartOne("MOVE USP, A");
	out_hex(Disasm_reg);
}

static inline void DisasmTas()
{
	Disasm_opsize = 1;
	DisasmStartOne("TAS ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmFLine()
{
	DisasmStartOne("$");
	out_hex(Disasm_opcode);
}

static inline void DisasmCallMorRtm()
{
	DisasmStartOne("CALLM #");
	out_hex(Disasm_nextibyte());
	out_str(", ");
	DisasmModeRegister(Disasm_mode, Disasm_reg);
}

static inline void DisasmStop()
{
	DisasmStartOne("STOP #");
	out_hex(Disasm_nextiword());
}

static inline void DisasmReset()
{
	DisasmStartOne("RESET");
}

static inline void DisasmEXTBL()
{
	DisasmStartOne("EXTB.L D");
	out_hex(Disasm_reg);
}

static inline void DisasmTRAPcc()
{
	DisasmStartOne("TRAP");
	DisasmCC();

	switch (Disasm_reg)
	{
		case 2:
			out_str(" ");
			out_hex(Disasm_nextiword());
			break;
		case 3:
			out_str(" ");
			out_hex(Disasm_nextilong());
			break;
		case 4:
			break;
		default:
			break;
	}
}

static inline void DisasmChkL()
{
	Disasm_opsize = 4;
	DisasmCheck();
}

static inline void DisasmBkpt()
{
	DisasmStartOne("BKPT #");
	out_hex(Disasm_reg);
}

static inline void DisasmDivL()
{
	Disasm_opsize = 4;
	DisasmStartOne("DIV");

	{
		uint16_t extra = Disasm_nextiword();
		uint32_t rDr = extra & 7;
		uint32_t rDq = (extra >> 12) & 7;

		out_str((extra & 0x0800) ? "S" : "U");
		if (extra & 0x0400)
		{
			out_str("L");
		}
		out_str(".L ");

		DisasmModeRegister(Disasm_mode, Disasm_reg);
		out_str(", ");

		if (rDr != rDq)
		{
			out_str("D");
			out_hex(rDr);
			out_str(":");
		}
		out_str("D");
		out_hex(rDq);
	}
}

static inline void DisasmMulL()
{
	Disasm_opsize = 4;
	DisasmStartOne("MUL");

	{
		uint16_t extra = Disasm_nextiword();
		uint32_t rhi = extra & 7;
		uint32_t rlo = (extra >> 12) & 7;

		out_str((extra & 0x0800) ? "S" : "U");
		out_str(".L ");

		DisasmModeRegister(Disasm_mode, Disasm_reg);
		out_str(", ");

		if (extra & 0x400)
		{
			out_str("D");
			out_hex(rhi);
			out_str(":");
		}
		out_str("D");
		out_hex(rlo);
	}
}

static inline void DisasmRtd()
{
	DisasmStartOne("RTD #");
	out_hex((int32_t)(int16_t)Disasm_nextiword());
}

static void DisasmControlReg(uint16_t i)
{
	switch (i)
	{
		case 0x0000:
			out_str("SFC");
			break;
		case 0x0001:
			out_str("DFC");
			break;
		case 0x0002:
			out_str("CACR");
			break;
		case 0x0800:
			out_str("USP");
			break;
		case 0x0801:
			out_str("VBR");
			break;
		case 0x0802:
			out_str("CAAR");
			break;
		case 0x0803:
			out_str("MSP");
			break;
		case 0x0804:
			out_str("ISP");
			break;
		default:
			out_str("???");
			break;
	}
}

static inline void DisasmMoveC()
{
	DisasmStartOne("MOVEC ");

	{
		uint16_t src = Disasm_nextiword();
		int regno = (src >> 12) & 0x0F;
		switch (Disasm_reg)
		{
			case 2:
				DisasmControlReg(src & 0x0FFF);
				out_str(", ");
				out_str(regno < 8 ? "D" : "A");
				out_hex(regno & 7);
				break;
			case 3:
				out_str(regno < 8 ? "D" : "A");
				out_hex(regno & 7);
				out_str(", ");
				DisasmControlReg(src & 0x0FFF);
				break;
			default:
				break;
		}
	}
}

static inline void DisasmLinkL()
{
	DisasmStartOne("LINK.L A");
	out_hex(Disasm_reg);
	out_str(", ");
	out_hex(Disasm_nextilong());
}

static inline void DisasmPack()
{
	DisasmStartOne("PACK ???");
}
static inline void DisasmUnpk()
{
	DisasmStartOne("UNPK ???");
}
static inline void DisasmCHK2orCMP2()
{
	DisasmStartOne("CHK2/CMP2 ???");
}
static inline void DisasmCAS2()
{
	DisasmStartOne("CAS2 ???");
}

[[maybe_unused]]
static inline void DisasmCAS()
{
	DisasmStartOne("CAS ???");
}

[[maybe_unused]]
static inline void DisasmMOVES()
{
	DisasmStartOne("MOVES ???");
}

static inline void DisasmBitField()
{
	DisasmStartOne("BitField ???");
}

/* -- address mode validation ---- */

static bool IsValidAddrMode()
{
	return (Disasm_mode != 7) || (Disasm_reg < 5);
}

static bool IsValidDstAddrMode()
{
	return (Disasm_md6 != 7) || (Disasm_rg9 < 2);
}

static bool IsValidDataAltAddrMode()
{
	switch (Disasm_mode)
	{
		case 1:
			return false;
		case 0:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			return true;
		case 7:
			return Disasm_reg < 2;
		default:
			return false;
	}
}

static bool IsValidDataAddrMode()
{
	switch (Disasm_mode)
	{
		case 1:
			return false;
		case 0:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			return true;
		case 7:
			return Disasm_reg < 5;
		default:
			return false;
	}
}

static bool IsValidControlAddrMode()
{
	switch (Disasm_mode)
	{
		case 0:
		case 1:
		case 3:
		case 4:
			return false;
		case 2:
		case 5:
		case 6:
			return true;
		case 7:
			return Disasm_reg < 4;
		default:
			return false;
	}
}

static bool IsValidControlAltAddrMode()
{
	switch (Disasm_mode)
	{
		case 0:
		case 1:
		case 3:
		case 4:
			return false;
		case 2:
		case 5:
		case 6:
			return true;
		case 7:
			return Disasm_reg < 2;
		default:
			return false;
	}
}

static bool IsValidAltMemAddrMode()
{
	switch (Disasm_mode)
	{
		case 0:
		case 1:
			return false;
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			return true;
		case 7:
			return Disasm_reg < 2;
		default:
			return false;
	}
}

/* -- top-level opcode group dispatchers -- */

static inline void DisasmCode0()
{
	if (Disasm_b8 == 1)
	{
		if (Disasm_mode == 1)
		{
			DisasmMoveP();
		}
		else
		{
			if (Disasm_mode == 0)
			{
				DisasmBitOpDD();
			}
			else
			{
				if (Disasm_b76 == 0)
				{
					if (IsValidDataAddrMode())
					{
						DisasmBitOpDM();
					}
					else
					{
						DisasmIllegal();
					}
				}
				else
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmBitOpDM();
					}
					else
					{
						DisasmIllegal();
					}
				}
			}
		}
	}
	else
	{
		if (Disasm_rg9 == 4)
		{
			if (Disasm_mode == 0)
			{
				DisasmBitOpND();
			}
			else
			{
				if (Disasm_b76 == 0)
				{
					if ((Disasm_mode == 7) && (Disasm_reg == 4))
					{
						DisasmIllegal();
					}
					else
					{
						if (IsValidDataAddrMode())
						{
							DisasmBitOpNM();
						}
						else
						{
							DisasmIllegal();
						}
					}
				}
				else
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmBitOpNM();
					}
					else
					{
						DisasmIllegal();
					}
				}
			}
		}
		else if (Disasm_b76 == 3)
		{
			if (Disasm_rg9 < 3)
			{
				if (IsValidControlAddrMode())
				{
					DisasmCHK2orCMP2();
				}
				else
				{
					DisasmIllegal();
				}
			}
			else if (Disasm_rg9 >= 5)
			{
				DisasmCAS2();
			}
			else if (Disasm_rg9 == 3)
			{
				DisasmCallMorRtm();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else if (Disasm_rg9 == 6)
		{
			if (IsValidDataAltAddrMode())
			{
				DisasmCmpI();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else if (Disasm_rg9 == 7)
		{
			if (IsValidAltMemAddrMode())
			{
				DisasmMoveSREa();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if ((Disasm_mode == 7) && (Disasm_reg == 4))
			{
				switch (Disasm_rg9)
				{
					case 0:
					case 1:
					case 5:
						DisasmBinOpStatusCCR();
						break;
					default:
						DisasmIllegal();
						break;
				}
			}
			else
			{
				if (!IsValidDataAltAddrMode())
				{
					DisasmIllegal();
				}
				else
				{
					switch (Disasm_rg9)
					{
						case 0:
							DisasmOrI();
							break;
						case 1:
							DisasmAndI();
							break;
						case 2:
							DisasmSubI();
							break;
						case 3:
							DisasmAddI();
							break;
						case 5:
							DisasmEorI();
							break;
						default:
							DisasmIllegal();
							break;
					}
				}
			}
		}
	}
}

static inline void DisasmCode1()
{
	if ((Disasm_mode == 1) || !IsValidAddrMode())
	{
		DisasmIllegal();
	}
	else if (Disasm_md6 == 1)
	{
		DisasmIllegal();
	}
	else if (!IsValidDstAddrMode())
	{
		DisasmIllegal();
	}
	else
	{
		DisasmMoveB();
	}
}

static inline void DisasmCode2()
{
	if (Disasm_md6 == 1)
	{
		if (IsValidAddrMode())
		{
			DisasmMoveAL();
		}
		else
		{
			DisasmIllegal();
		}
	}
	else if (!IsValidAddrMode())
	{
		DisasmIllegal();
	}
	else if (!IsValidDstAddrMode())
	{
		DisasmIllegal();
	}
	else
	{
		DisasmMoveL();
	}
}

static inline void DisasmCode3()
{
	if (Disasm_md6 == 1)
	{
		if (IsValidAddrMode())
		{
			DisasmMoveAW();
		}
		else
		{
			DisasmIllegal();
		}
	}
	else if (!IsValidAddrMode())
	{
		DisasmIllegal();
	}
	else if (!IsValidDstAddrMode())
	{
		DisasmIllegal();
	}
	else
	{
		DisasmMoveW();
	}
}

static inline void DisasmCode4()
{
	if (Disasm_b8 != 0)
	{
		switch (Disasm_b76)
		{
			case 0:
				if (IsValidDataAddrMode())
				{
					DisasmChkL();
				}
				else
				{
					DisasmIllegal();
				}
				break;
			case 1:
				DisasmIllegal();
				break;
			case 2:
				if (IsValidDataAddrMode())
				{
					DisasmChkW();
				}
				else
				{
					DisasmIllegal();
				}
				break;
			case 3:
			default:
				if ((0 == Disasm_mode) && (4 == Disasm_rg9))
				{
					DisasmEXTBL();
				}
				else
				{
					if (IsValidControlAddrMode())
					{
						DisasmLea();
					}
					else
					{
						DisasmIllegal();
					}
				}
				break;
		}
	}
	else
	{
		switch (Disasm_rg9)
		{
			case 0:
				if (Disasm_b76 != 3)
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmNegX();
					}
					else
					{
						DisasmIllegal();
					}
				}
				else
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmMoveSREa();
					}
					else
					{
						DisasmIllegal();
					}
				}
				break;
			case 1:
				if (Disasm_b76 != 3)
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmClr();
					}
					else
					{
						DisasmIllegal();
					}
				}
				else
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmMoveCCREa();
					}
					else
					{
						DisasmIllegal();
					}
				}
				break;
			case 2:
				if (Disasm_b76 != 3)
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmNeg();
					}
					else
					{
						DisasmIllegal();
					}
				}
				else
				{
					if (IsValidDataAddrMode())
					{
						DisasmMoveEaCR();
					}
					else
					{
						DisasmIllegal();
					}
				}
				break;
			case 3:
				if (Disasm_b76 != 3)
				{
					if (IsValidDataAltAddrMode())
					{
						DisasmNot();
					}
					else
					{
						DisasmIllegal();
					}
				}
				else
				{
					if (IsValidDataAddrMode())
					{
						DisasmMoveEaSR();
					}
					else
					{
						DisasmIllegal();
					}
				}
				break;
			case 4:
				switch (Disasm_b76)
				{
					case 0:
						if (Disasm_mode == 1)
						{
							DisasmLinkL();
						}
						else
						{
							if (IsValidDataAltAddrMode())
							{
								DisasmNbcd();
							}
							else
							{
								DisasmIllegal();
							}
						}
						break;
					case 1:
						if (Disasm_mode == 0)
						{
							DisasmSwap();
						}
						else if (Disasm_mode == 1)
						{
							DisasmBkpt();
						}
						else
						{
							if (IsValidControlAddrMode())
							{
								DisasmPEA();
							}
							else
							{
								DisasmIllegal();
							}
						}
						break;
					case 2:
						if (Disasm_mode == 0)
						{
							DisasmEXTW();
						}
						else
						{
							if (Disasm_mode == 4)
							{
								DisasmMOVEMRmM();
							}
							else
							{
								if (IsValidControlAltAddrMode())
								{
									DisasmMOVEMrm();
								}
								else
								{
									DisasmIllegal();
								}
							}
						}
						break;
					case 3:
					default:
						if (Disasm_mode == 0)
						{
							DisasmEXTL();
						}
						else
						{
							if (Disasm_mode == 4)
							{
								DisasmMOVEMRmM();
							}
							else
							{
								if (IsValidControlAltAddrMode())
								{
									DisasmMOVEMrm();
								}
								else
								{
									DisasmIllegal();
								}
							}
						}
						break;
				}
				break;
			case 5:
				if (Disasm_b76 == 3)
				{
					if ((Disasm_mode == 7) && (Disasm_reg == 4))
					{
						DisasmIllegal();
					}
					else
					{
						if (IsValidDataAltAddrMode())
						{
							DisasmTas();
						}
						else
						{
							DisasmIllegal();
						}
					}
				}
				else
				{
					if (Disasm_b76 == 0)
					{
						if (IsValidDataAltAddrMode())
						{
							DisasmTst();
						}
						else
						{
							DisasmIllegal();
						}
					}
					else
					{
						if (IsValidAddrMode())
						{
							DisasmTst();
						}
						else
						{
							DisasmIllegal();
						}
					}
				}
				break;
			case 6:
				if (((Disasm_opcode >> 7) & 1) == 1)
				{
					if (Disasm_mode == 3)
					{
						DisasmMOVEMApR();
					}
					else
					{
						if (IsValidControlAddrMode())
						{
							DisasmMOVEMmr();
						}
						else
						{
							DisasmIllegal();
						}
					}
				}
				else
				{
					if (((Disasm_opcode >> 6) & 1) == 1)
					{
						DisasmDivL();
					}
					else
					{
						DisasmMulL();
					}
				}
				break;
			case 7:
			default:
				switch (Disasm_b76)
				{
					case 0:
						DisasmIllegal();
						break;
					case 1:
						switch (Disasm_mode)
						{
							case 0:
							case 1:
								DisasmTrap();
								break;
							case 2:
								if (Disasm_reg == 6)
								{
									DisasmLinkA6();
								}
								else
								{
									DisasmLink();
								}
								break;
							case 3:
								if (Disasm_reg == 6)
								{
									DisasmUnlkA6();
								}
								else
								{
									DisasmUnlk();
								}
								break;
							case 4:
								DisasmMoveRUSP();
								break;
							case 5:
								DisasmMoveUSPR();
								break;
							case 6:
								switch (Disasm_reg)
								{
									case 0:
										DisasmReset();
										break;
									case 1:
										DisasmNop();
										break;
									case 2:
										DisasmStop();
										break;
									case 3:
										DisasmRte();
										break;
									case 4:
										DisasmRtd();
										break;
									case 5:
										DisasmRts();
										break;
									case 6:
										DisasmTrapV();
										break;
									case 7:
									default:
										DisasmRtr();
										break;
								}
								break;
							case 7:
							default:
								DisasmMoveC();
								break;
						}
						break;
					case 2:
						if (IsValidControlAddrMode())
						{
							DisasmJsr();
						}
						else
						{
							DisasmIllegal();
						}
						break;
					case 3:
					default:
						if (IsValidControlAddrMode())
						{
							DisasmJmp();
						}
						else
						{
							DisasmIllegal();
						}
						break;
				}
				break;
		}
	}
}

static inline void DisasmCode5()
{
	if (Disasm_b76 == 3)
	{
		if (Disasm_mode == 1)
		{
			DisasmDBcc();
		}
		else
		{
			if ((Disasm_mode == 7) && (Disasm_reg >= 2))
			{
				DisasmTRAPcc();
			}
			else
			{
				if (IsValidDataAltAddrMode())
				{
					DisasmScc();
				}
				else
				{
					DisasmIllegal();
				}
			}
		}
	}
	else
	{
		if (Disasm_mode == 1)
		{
			if (Disasm_b8 == 0)
			{
				DisasmAddQA();
			}
			else
			{
				DisasmSubQA();
			}
		}
		else
		{
			if (Disasm_b8 == 0)
			{
				if (IsValidDataAltAddrMode())
				{
					DisasmAddQ();
				}
				else
				{
					DisasmIllegal();
				}
			}
			else
			{
				if (IsValidDataAltAddrMode())
				{
					DisasmSubQ();
				}
				else
				{
					DisasmIllegal();
				}
			}
		}
	}
}

static inline void DisasmCode6()
{
	uint32_t cond = (Disasm_opcode >> 8) & 15;
	if (cond == 1)
	{
		DisasmBsr();
	}
	else
	{
		DisasmBcc();
	}
}

static inline void DisasmCode7()
{
	if (Disasm_b8 == 0)
	{
		DisasmMoveQ();
	}
	else
	{
		DisasmIllegal();
	}
}

static inline void DisasmCode8()
{
	if (Disasm_b76 == 3)
	{
		if (Disasm_b8 == 0)
		{
			if (IsValidDataAddrMode())
			{
				DisasmDivU();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if (IsValidDataAddrMode())
			{
				DisasmDivS();
			}
			else
			{
				DisasmIllegal();
			}
		}
	}
	else
	{
		if (Disasm_b8 == 0)
		{
			if (IsValidDataAddrMode())
			{
				DisasmOrEaD();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if (Disasm_mode < 2)
			{
				switch (Disasm_b76)
				{
					case 0:
						if (Disasm_mode == 0)
						{
							DisasmSbcdr();
						}
						else
						{
							DisasmSbcdm();
						}
						break;
					case 1:
						DisasmPack();
						break;
					case 2:
						DisasmUnpk();
						break;
					default:
						DisasmIllegal();
						break;
				}
			}
			else
			{
				if (IsValidDataAltAddrMode())
				{
					DisasmOrDEa();
				}
				else
				{
					DisasmIllegal();
				}
			}
		}
	}
}

static inline void DisasmCode9()
{
	if (Disasm_b76 == 3)
	{
		if (IsValidAddrMode())
		{
			DisasmSubA();
		}
		else
		{
			DisasmIllegal();
		}
	}
	else
	{
		if (Disasm_b8 == 0)
		{
			if (IsValidAddrMode())
			{
				DisasmSubEaR();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if (Disasm_mode == 0)
			{
				DisasmSubXd();
			}
			else if (Disasm_mode == 1)
			{
				DisasmSubXm();
			}
			else
			{
				if (IsValidAltMemAddrMode())
				{
					DisasmSubREa();
				}
				else
				{
					DisasmIllegal();
				}
			}
		}
	}
}

static inline void DisasmCodeA()
{
	DisasmALine();
}

static inline void DisasmCodeB()
{
	if (Disasm_b76 == 3)
	{
		if (IsValidAddrMode())
		{
			DisasmCmpA();
		}
		else
		{
			DisasmIllegal();
		}
	}
	else if (Disasm_b8 == 1)
	{
		if (Disasm_mode == 1)
		{
			DisasmCmpM();
		}
		else
		{
			if (IsValidDataAltAddrMode())
			{
				DisasmEor();
			}
			else
			{
				DisasmIllegal();
			}
		}
	}
	else
	{
		if (IsValidAddrMode())
		{
			DisasmCompare();
		}
		else
		{
			DisasmIllegal();
		}
	}
}

static inline void DisasmCodeC()
{
	if (Disasm_b76 == 3)
	{
		if (Disasm_b8 == 0)
		{
			if (IsValidDataAddrMode())
			{
				DisasmMulU();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if (IsValidDataAddrMode())
			{
				DisasmMulS();
			}
			else
			{
				DisasmIllegal();
			}
		}
	}
	else
	{
		if (Disasm_b8 == 0)
		{
			if (IsValidDataAddrMode())
			{
				DisasmAndEaD();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if (Disasm_mode < 2)
			{
				switch (Disasm_b76)
				{
					case 0:
						if (Disasm_mode == 0)
						{
							DisasmAbcdr();
						}
						else
						{
							DisasmAbcdm();
						}
						break;
					case 1:
						if (Disasm_mode == 0)
						{
							DisasmExgdd();
						}
						else
						{
							DisasmExgaa();
						}
						break;
					case 2:
					default:
						if (Disasm_mode == 0)
						{
							DisasmIllegal();
						}
						else
						{
							DisasmExgda();
						}
						break;
				}
			}
			else
			{
				if (IsValidAltMemAddrMode())
				{
					DisasmAndDEa();
				}
				else
				{
					DisasmIllegal();
				}
			}
		}
	}
}

static inline void DisasmCodeD()
{
	if (Disasm_b76 == 3)
	{
		if (IsValidAddrMode())
		{
			DisasmAddA();
		}
		else
		{
			DisasmIllegal();
		}
	}
	else
	{
		if (Disasm_b8 == 0)
		{
			if (IsValidAddrMode())
			{
				DisasmAddEaR();
			}
			else
			{
				DisasmIllegal();
			}
		}
		else
		{
			if (Disasm_mode == 0)
			{
				DisasmAddXd();
			}
			else if (Disasm_mode == 1)
			{
				DisasmAddXm();
			}
			else
			{
				if (IsValidAltMemAddrMode())
				{
					DisasmAddREa();
				}
				else
				{
					DisasmIllegal();
				}
			}
		}
	}
}

static inline void DisasmCodeE()
{
	if (Disasm_b76 == 3)
	{
		if ((Disasm_opcode & 0x0800) != 0)
		{
			switch (Disasm_mode)
			{
				case 1:
				case 3:
				case 4:
				default:
					DisasmIllegal();
					break;
				case 0:
				case 2:
				case 5:
				case 6:
					DisasmBitField();
					break;
				case 7:
					switch (Disasm_reg)
					{
						case 0:
						case 1:
							DisasmBitField();
							break;
						case 2:
						case 3:
							switch ((Disasm_opcode >> 8) & 7)
							{
								case 0:
								case 1:
								case 3:
								case 5:
									DisasmBitField();
									break;
								default:
									DisasmIllegal();
									break;
							}
							break;
						default:
							DisasmIllegal();
							break;
					}
					break;
			}
		}
		else
		{
			if (IsValidAltMemAddrMode())
			{
				DisasmRolopNM();
			}
			else
			{
				DisasmIllegal();
			}
		}
	}
	else
	{
		if (Disasm_mode < 4)
		{
			DisasmRolopND();
		}
		else
		{
			DisasmRolopDD();
		}
	}
}

static inline void DisasmCodeF()
{
	DisasmFLine();
}

/* -- main dispatch ---- */

static void m68k_Disasm_one()
{
	Disasm_opcode = Disasm_nextiword();

	switch (Disasm_opcode >> 12)
	{
		case 0x0:
			DisasmCode0();
			break;
		case 0x1:
			DisasmCode1();
			break;
		case 0x2:
			DisasmCode2();
			break;
		case 0x3:
			DisasmCode3();
			break;
		case 0x4:
			DisasmCode4();
			break;
		case 0x5:
			DisasmCode5();
			break;
		case 0x6:
			DisasmCode6();
			break;
		case 0x7:
			DisasmCode7();
			break;
		case 0x8:
			DisasmCode8();
			break;
		case 0x9:
			DisasmCode9();
			break;
		case 0xA:
			DisasmCodeA();
			break;
		case 0xB:
			DisasmCodeB();
			break;
		case 0xC:
			DisasmCodeC();
			break;
		case 0xD:
			DisasmCodeD();
			break;
		case 0xE:
			DisasmCodeE();
			break;
		case 0xF:
		default:
			DisasmCodeF();
			break;
	}
}

/* -- new public API ---- */

std::string Disassemble(uint32_t &pc)
{
	std::string result;
	s_out = &result;

	Disasm_setpc(pc);
	m68k_Disasm_one();
	pc = Disasm_pc;

	s_out = nullptr;
	return result;
}

/* -- legacy API (implemented on top of Disassemble) ---- */

#define Ln2SavedPCs 4
#define NumSavedPCs (1 << Ln2SavedPCs)
#define SavedPCsMask (NumSavedPCs - 1)
static uint32_t SavedPCs[NumSavedPCs];
static uint32_t SavedPCsIn = 0;
static uint32_t SavedPCsOut = 0;

static uint32_t DisasmCounter = 0;

static void DisasmOneAndBack_legacy(uint32_t pc)
{
	std::string text = Disassemble(pc);
	dbglog_writeHex(pc);
	dbglog_writeCStr("  ");
	dbglog_writeCStr(const_cast<char *>(text.c_str()));
	dbglog_writeReturn();
}

static void DisasmSavedPCs_legacy()
{
	uint32_t n = SavedPCsIn - SavedPCsOut;

	if (n != 0)
	{
		uint32_t j = SavedPCsOut;

		SavedPCsOut = SavedPCsIn;

		if (n > NumSavedPCs)
		{
			n = NumSavedPCs;
			j = SavedPCsIn - NumSavedPCs;
			dbglog_writeReturn();
		}

		do
		{
			--n;
			uint32_t pc = SavedPCs[j & SavedPCsMask];
			DisasmOneAndBack_legacy(pc);
			++j;
		} while (n != 0);
	}
}

void DisasmOneOrSave(uint32_t pc)
{
	if (0 != DisasmCounter)
	{
		DisasmOneAndBack_legacy(pc);
		--DisasmCounter;
	}
	else
	{
		SavedPCs[SavedPCsIn & SavedPCsMask] = pc;
		++SavedPCsIn;
	}
}

void DumpRecentDisasm()
{
	uint32_t n = SavedPCsIn - SavedPCsOut;
	if (n > NumSavedPCs) n = NumSavedPCs;
	if (n == 0) return;

	uint32_t j = SavedPCsIn - n;
	std::fprintf(stderr, "--- last %u instructions ---\n", (unsigned)n);
	for (uint32_t i = 0; i < n; i++)
	{
		uint32_t pc = SavedPCs[(j + i) & SavedPCsMask];
		uint32_t pc_copy = pc;
		std::string text = Disassemble(pc_copy);
		std::fprintf(stderr, "  %08X  %s\n", (unsigned)pc, text.c_str());
	}
	std::fprintf(stderr, "---\n");
	std::fflush(stderr);
}

void m68k_WantDisasmContext()
{
	DisasmSavedPCs_legacy();
	DisasmCounter = 128;
}
