/*
	cpu.h

	Thin wrapper around the m68k emulator, presenting it as a class.
	The actual CPU state remains as file-scope statics in m68k.cpp;
	this class just wraps the public API for the Machine to own.

	Part of Phase 4: Device Interface & Machine Object.
*/

#pragma once
#include <cstdint>

// ATTep is typedef'd in machine.h; we just need the forward declaration
// so this header can be parsed standalone.
struct ATTer;

class CPU {
public:
	// Initialization
	void init(uint8_t* iplPtr);
	void reset();

#if SmallGlobals
	void reserveAlloc();
#endif

	// Execution
	void go_nCycles(uint32_t n);

	// Cycle accounting
	int32_t getCyclesRemaining() const;
	void    setCyclesRemaining(int32_t n);

	// Interrupt priority level change notification
	void iplChangeNotify();

	// Pseudo-exception for disk insertion
	void diskInsertedPseudoException(uint32_t newpc, uint32_t data);

	// Address Translation Table
	void    setHeadATTel(ATTer* p);
	ATTer*  findATTel(uint32_t addr);
};

// Global CPU instance (will move to Machine in a later step)
extern CPU g_cpu;
