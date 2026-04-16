/*
	extfs_log.h — SharedDrive structured trap logging.

	The guest sends a single RPC per trap invocation with numeric
	parameters; this module pretty-prints them on the host side.
*/

#pragma once

#include <cstdint>

/*
	extfsLogTrap — called from the kExtFSLogTrap (0x020F) handler.

	trapWord  : flat trap number (0x00=Open, 0x01=Close, …) or HFS
				selector (0x0001=OpenWD, 0x0009=GetCatInfo, …)
	pbAddr    : address of the param block in guest RAM
	action    : 0=PASS-THROUGH, 1=HANDLED, 2=HANDLED+ERROR
	err       : OSErr result (meaningful when action != 0)
	flags     : bit 0 = isHFS, bit 1 = param block was modified
*/
void extfsLogTrap(uint16_t trapWord, uint32_t pbAddr, uint16_t action, int16_t err, uint16_t flags);
