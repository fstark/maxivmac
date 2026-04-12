#pragma once

#include <cstdint>

/*
	External filesystem extension handler for the register-block interface.
	Called from regDispatch() in machine.cpp when command codes
	$200–$2FF are written to the register block.
*/
void ExtnExtFSDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);
