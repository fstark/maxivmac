/*
	serial_factory — Create serial backends from command-line mode strings.
*/
#pragma once

#include "devices/serial_backend.h"
#include <memory>
#include <string>

/* Parse a --serial-X=MODE string and return the corresponding backend.
   Returns nullptr on empty string (= no backend).
   Prints a warning to stderr and returns nullptr on unknown mode. */
std::unique_ptr<SerialBackend> CreateSerialBackend(const std::string& mode, int chan);
