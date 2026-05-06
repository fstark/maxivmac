#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

/*
	External filesystem extension handler for the register-block interface.
	Called from regDispatch() in machine.cpp when command codes
	$200–$2FF are written to the register block.
*/
void ExtnExtFSDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);

// Mount a host directory as a new shared drive.
// Returns the slot index, or -1 on failure.
int ExtFSMountDrive(const std::filesystem::path &hostDir);

// Unmount a shared drive by slot.  Closes all open forks.
// Returns false if the slot was already empty.
bool ExtFSUnmountDrive(int slot);

// Print all mounted shared drives to a callback.
// Callback: void(const char *line)
void ExtFSDriveList(void (*printFn)(void *ctx, const char *line), void *ctx);

// Queue a guest command for pickup by the SharedDrive INIT.
// cmd: 1=launch, 2=exittoshell, 3=shutdown
void ExtFS_QueueGuestCmd(uint16_t cmd, std::string_view path = {});
