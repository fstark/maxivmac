#pragma once

#include <cstdint>
#include <deque>
#include <string>

/*
	Clipboard extension handler for the new register-block interface.
	Called from regDispatch() in machine.cpp when command codes
	$100–$1FF are written to the register block.
*/
void ExtnClipDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);

/* Access the guest debug console log buffer (for UI display). */
const std::deque<std::string> &extnDbgConsoleLines();

/* Guest debug helpers — shared with other extensions. */
void guestConsoleAppend(const std::string &line);
std::string guestFormatLog(uint32_t fmtAddr, uint32_t args[7]);
void ExtnDbgConsoleClear();
