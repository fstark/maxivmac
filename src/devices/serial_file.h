/*
	FileBackend — log TX to a file, optionally read RX from a file.

	Usage:
	  --serial-a=file:tx=/tmp/serial.log
	  --serial-a=file:tx=/tmp/out.bin,rx=/tmp/in.bin
*/
#pragma once

#include "devices/serial_backend.h"
#include <cstdio>
#include <string>

class FileBackend : public SerialBackend {
public:
	/* `txPath` may be empty (no TX logging).
	   `rxPath` may be empty (no RX source). */
	FileBackend(const std::string& txPath, const std::string& rxPath)
	{
		if (!txPath.empty())
			txFile_ = fopen(txPath.c_str(), "ab");
		if (!rxPath.empty())
			rxFile_ = fopen(rxPath.c_str(), "rb");
	}

	~FileBackend() override
	{
		if (txFile_) fclose(txFile_);
		if (rxFile_) fclose(rxFile_);
	}

	void txByte(uint8_t byte) override
	{
		if (txFile_) {
			fputc(byte, txFile_);
			fflush(txFile_);
		}
	}

	bool rxReady() override
	{
		if (!rxFile_) return false;
		if (rxEof_) return false;
		/* Peek ahead: try to read one byte and buffer it. */
		if (!rxBuffered_) {
			int c = fgetc(rxFile_);
			if (c == EOF) { rxEof_ = true; return false; }
			rxBuf_ = (uint8_t)c;
			rxBuffered_ = true;
		}
		return true;
	}

	uint8_t rxByte() override
	{
		rxBuffered_ = false;
		return rxBuf_;
	}

	void poll() override {}
	const char* name() const override { return "file"; }

private:
	FILE* txFile_ = nullptr;
	FILE* rxFile_ = nullptr;
	uint8_t rxBuf_ = 0;
	bool rxBuffered_ = false;
	bool rxEof_ = false;
};
