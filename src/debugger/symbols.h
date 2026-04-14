/*
	symbols.h — Symbol lookup for debugger

	Provides name→address and address→name mapping over the built-in
	trap dictionary and low-memory global tables.
*/
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

struct SymbolEntry
{
	std::string_view name;
	uint32_t address;
	uint16_t size;	   // 0 for traps
	bool isTrap;	   // true = trap, false = low-mem global
	uint16_t trapWord; // valid only when isTrap
};

// Must be called once at startup to build sorted indexes.
void SymbolsInit();

// Return counts after init.
int SymbolsTrapCount();
int SymbolsGlobalCount();

// Resolve a name to an address.  Returns false if not found.
// For traps, sets outAddr=0 and outTrapWord to the trap word.
// For globals, sets outAddr to the global's address and outTrapWord=0.
bool SymbolsResolve(std::string_view name, uint32_t &outAddr, uint16_t &outTrapWord);

// Reverse-lookup: given an address, find the symbol name.
// Returns empty string_view if no symbol at that address.
std::string_view SymbolsAtAddress(uint32_t addr);

// Prefix search.  Appends matching entries to results.
// kind: 't' for traps only, 'g' for globals only, '*' for both.
void SymbolsSearch(std::string_view prefix, char kind, std::vector<SymbolEntry> &results,
				   int maxResults = 20);

// Get the size of a low-memory global by address.  Returns 0 if unknown.
uint16_t SymbolsSizeAt(uint32_t addr);
