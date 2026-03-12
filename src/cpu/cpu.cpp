/*
	cpu.cpp

	Thin wrapper implementation for the CPU class.
	All methods forward to the existing free functions in m68k.cpp.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/common.h"
#include "cpu/cpu.h"
#include "cpu/m68k.h"

CPU g_cpu;

void CPU::init(uint8_t* iplPtr, const MachineConfig* config)
{
	MINEM68K_Init(iplPtr, config);
}

void CPU::reset()
{
	m68k_reset();
}

#if SmallGlobals
void CPU::reserveAlloc()
{
	MINEM68K_ReserveAlloc();
}
#endif

void CPU::go_nCycles(uint32_t n)
{
	m68k_go_nCycles(n);
}

int32_t CPU::getCyclesRemaining() const
{
	return GetCyclesRemaining();
}

void CPU::setCyclesRemaining(int32_t n)
{
	SetCyclesRemaining(n);
}

void CPU::iplChangeNotify()
{
	m68k_IPLchangeNtfy();
}

void CPU::diskInsertedPseudoException(uint32_t newpc, uint32_t data)
{
	DiskInsertedPsuedoException(newpc, data);
}

void CPU::setHeadATTel(ATTer* p)
{
	SetHeadATTel(p);
}

ATTer* CPU::findATTel(uint32_t addr)
{
	return FindATTel(addr);
}
