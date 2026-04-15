#include "storage/host_volume.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

namespace storage
{

namespace fs = std::filesystem;

/* ── Helpers ──────────────────────────────────────── */

uint32_t HostVolume::currentMacDate()
{
	auto now = std::chrono::system_clock::now();
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	return static_cast<uint32_t>(secs + appledouble::kMacEpochOffset);
}

/* ── Mount ────────────────────────────────────────── */

bool HostVolume::mount(const std::filesystem::path & /*hostDir*/)
{
	return false;
}

bool HostVolume::isMounted() const
{
	return mounted_;
}

/* ── Catalog queries ──────────────────────────────── */

const CatalogEntry *HostVolume::findByCNID(uint32_t /*cnid*/) const
{
	(void)nextCNID_;
	return nullptr;
}

const CatalogEntry *HostVolume::findByName(uint32_t /*parentDirID*/,
										   std::string_view /*macName*/) const
{
	return nullptr;
}

const CatalogEntry *HostVolume::nthChild(uint32_t /*parentDirID*/, int /*index*/) const
{
	return nullptr;
}

int HostVolume::childCount(uint32_t /*parentDirID*/) const
{
	return 0;
}

void HostVolume::volumeStats(uint32_t &outFiles, uint32_t &outBytes) const
{
	outFiles = 0;
	outBytes = 0;
}

/* ── File/directory creation ──────────────────────── */

uint32_t HostVolume::createFile(uint32_t /*parentDirID*/, std::string_view /*macName*/,
								FMErr &errOut)
{
	errOut = FMErr::kFnfErr;
	return 0;
}

uint32_t HostVolume::createDir(uint32_t /*parentDirID*/, std::string_view /*macName*/,
							   FMErr &errOut)
{
	errOut = FMErr::kFnfErr;
	return 0;
}

/* ── Deletion ─────────────────────────────────────── */

FMErr HostVolume::remove(uint32_t /*parentDirID*/, std::string_view /*macName*/)
{
	return FMErr::kFnfErr;
}

/* ── Move / rename ────────────────────────────────── */

FMErr HostVolume::move(uint32_t /*srcDirID*/, std::string_view /*macName*/, uint32_t /*dstDirID*/)
{
	return FMErr::kFnfErr;
}

/* ── Metadata ─────────────────────────────────────── */

FMErr HostVolume::setFileInfo(uint32_t /*cnid*/, uint32_t /*type*/, uint32_t /*creator*/)
{
	return FMErr::kFnfErr;
}

/* ── Fork I/O ─────────────────────────────────────── */

uint32_t HostVolume::openFork(uint32_t /*cnid*/, ForkType /*fork*/, uint32_t & /*outSize*/,
							  FMErr &errOut)
{
	(void)nextHandle_;
	errOut = FMErr::kFnfErr;
	return 0;
}

FMErr HostVolume::readFork(uint32_t /*handle*/, uint32_t /*offset*/, std::span<uint8_t> /*buf*/,
						   uint32_t &outRead)
{
	outRead = 0;
	return FMErr::kRfNumErr;
}

FMErr HostVolume::writeFork(uint32_t /*handle*/, uint32_t /*offset*/,
							std::span<const uint8_t> /*data*/, uint32_t &outWritten)
{
	outWritten = 0;
	return FMErr::kRfNumErr;
}

void HostVolume::closeFork(uint32_t /*handle*/) {}

/* ── Working directories ──────────────────────────── */

uint32_t HostVolume::openWD(uint32_t /*dirID*/)
{
	(void)nextWD_;
	return 0;
}

uint32_t HostVolume::wdToDirID(uint32_t /*wdRef*/) const
{
	return 0;
}

void HostVolume::closeWD(uint32_t /*wdRef*/) {}

/* ── TEXT conversion stats ────────────────────────── */

HostVolume::TextStats HostVolume::textConversionStats() const
{
	return textStats_;
}

void HostVolume::resetTextConversionStats()
{
	textStats_ = {};
}

/* ── Private helpers ──────────────────────────────── */

void HostVolume::scanDirectory(const std::filesystem::path & /*hostDir*/, uint32_t /*parentDirID*/)
{
}

CatalogEntry *HostVolume::mutableFindByCNID(uint32_t /*cnid*/)
{
	return nullptr;
}

std::string HostVolume::resolveParentPath(uint32_t /*parentDirID*/) const
{
	return {};
}

void HostVolume::invalidateTextSize(CatalogEntry & /*entry*/) {}

} // namespace storage
