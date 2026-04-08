#pragma once

#include <cstdint>
#include <deque>
#include <string>

/*
	Clipboard extension handler for the new register-block interface.
	Called from regDispatch() in machine.cpp when command codes
	$100–$1FF are written to the register block.
*/
void extnClipDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);

/* Access the guest debug console log buffer (for UI display). */
const std::deque<std::string>& extnDbgConsoleLines();
void extnDbgConsoleClear();
