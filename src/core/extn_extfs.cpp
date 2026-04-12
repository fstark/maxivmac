#include "core/extn_extfs.h"
#include "platform/platform.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cstring>

/* Guest RAM access */
extern uint8_t get_vm_byte(uint32_t addr);
extern void put_vm_byte(uint32_t addr, uint8_t b);

namespace fs = std::filesystem;

/* ── Command codes ────────────────────────────────── */

static constexpr uint16_t kExtFSVersion = 0x200;
static constexpr uint16_t kExtFSGetVol = 0x201;
static constexpr uint16_t kExtFSGetCatInfo = 0x202;
static constexpr uint16_t kExtFSGetCatInfoName = 0x203;
static constexpr uint16_t kExtFSOpen = 0x204;
static constexpr uint16_t kExtFSRead = 0x205;
static constexpr uint16_t kExtFSClose = 0x206;
static constexpr uint16_t kExtFSGetFileInfo = 0x207;
static constexpr uint16_t kExtFSReadDir = 0x208;
static constexpr uint16_t kExtFSObjByName = 0x209;
static constexpr uint16_t kExtFSGetWDInfo = 0x20A;
static constexpr uint16_t kExtFSOpenWD = 0x20B;
static constexpr uint16_t kExtFSCloseWD = 0x20C;
static constexpr uint16_t kExtFSDbgLog = 0x20D;

/* ── Catalog ──────────────────────────────────────── */

struct CatalogEntry
{
	uint32_t cnid;
	uint32_t parentDirID;
	bool isDirectory;
	std::string hostPath;
	std::string macName; /* Mac OS Roman, ≤31 bytes */
	uint32_t dataForkSize;
	uint32_t type;
	uint32_t creator;
	uint32_t crDate;  /* Mac epoch */
	uint32_t modDate; /* Mac epoch */
};

static std::vector<CatalogEntry> s_catalog;
static bool s_mounted = false;
static std::string s_sharedDir;

/* ── Open file handles ────────────────────────────── */

struct OpenFile
{
	FILE *fp;
	uint32_t cnid;
};

static std::unordered_map<uint32_t, OpenFile> s_openFiles;
static uint32_t s_nextHandle = 1;

/* ── Working directories ──────────────────────────── */

struct WDEntry
{
	uint32_t dirID;
};

static std::unordered_map<uint32_t, WDEntry> s_wdTable;
static uint32_t s_nextWD = 0x8000; /* WD refs start here */

/* ── Helpers ──────────────────────────────────────── */

static constexpr uint32_t kRootDirID = 2;
static constexpr uint32_t kFirstCNID = 16;
static constexpr uint32_t kMacEpochOffset = 2082844800u; /* 1904→1970 diff */

/* Convert a 4-char string like "TEXT" to a uint32_t. */
static uint32_t fourCC(const char *s)
{
	return (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16) |
		   (uint32_t(uint8_t(s[2])) << 8) | uint32_t(uint8_t(s[3]));
}

/* Extension → type/creator mapping. */
struct TypeMap
{
	const char *ext;
	const char *type;
	const char *creator;
};

static const TypeMap s_typeMap[] = {
	{".txt", "TEXT", "ttxt"},  {".text", "TEXT", "ttxt"}, {".c", "TEXT", "KAHL"},
	{".h", "TEXT", "KAHL"},	   {".p", "TEXT", "KAHL"},	  {".r", "TEXT", "KAHL"},
	{".cpp", "TEXT", "KAHL"},  {".hpp", "TEXT", "KAHL"},  {".s", "TEXT", "KAHL"},
	{".asm", "TEXT", "KAHL"},  {".md", "TEXT", "ttxt"},	  {".csv", "TEXT", "ttxt"},
	{".htm", "TEXT", "MOSS"},  {".html", "TEXT", "MOSS"}, {".jpg", "JPEG", "ogle"},
	{".jpeg", "JPEG", "ogle"}, {".gif", "GIFf", "ogle"},  {".bmp", "BMPf", "ogle"},
	{".png", "PNGf", "ogle"},  {".bin", "BINA", "hDmp"},  {nullptr, nullptr, nullptr}};

static void mapTypeCreator(const std::string &name, uint32_t &outType, uint32_t &outCreator)
{
	auto dot = name.rfind('.');
	if (dot != std::string::npos)
	{
		std::string ext = name.substr(dot);
		/* lowercase for matching */
		for (auto &c : ext)
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		for (const TypeMap *m = s_typeMap; m->ext; m++)
		{
			if (ext == m->ext)
			{
				outType = fourCC(m->type);
				outCreator = fourCC(m->creator);
				return;
			}
		}
	}
	outType = fourCC("????");
	outCreator = fourCC("????");
}

/* UTF-8 → Mac OS Roman for filenames (lossy). */
static std::string toMacRoman(const std::string &utf8)
{
	std::string out;
	out.reserve(utf8.size());
	for (size_t i = 0; i < utf8.size();)
	{
		uint8_t c = static_cast<uint8_t>(utf8[i]);
		if (c < 0x80)
		{
			/* Replace : with - (: is the Mac path separator) */
			out.push_back(c == ':' ? '-' : static_cast<char>(c));
			i++;
		}
		else
		{
			/* Multi-byte UTF-8: replace with '?' for now */
			out.push_back('?');
			/* skip continuation bytes */
			i++;
			while (i < utf8.size() && (static_cast<uint8_t>(utf8[i]) & 0xC0) == 0x80)
				i++;
		}
	}
	return out;
}

/* Truncate a Mac filename to 31 chars. */
static std::string truncateMacName(const std::string &name)
{
	if (name.size() <= 31) return name;
	return name.substr(0, 31);
}

/* Convert POSIX timestamp to Mac epoch. */
static uint32_t toMacDate(std::filesystem::file_time_type ft)
{
	/* Convert file_time to seconds since Mac epoch (1904-01-01). */
	auto dur = ft.time_since_epoch();
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
	/* file_clock epoch varies by implementation; on macOS/libc++ it's
	   the POSIX epoch (1970-01-01). */
	return static_cast<uint32_t>(secs + kMacEpochOffset);
}

/* Write a Pascal string to guest RAM. */
static void writePascalString(uint32_t addr, const std::string &s)
{
	uint8_t len = static_cast<uint8_t>(std::min(s.size(), size_t(31)));
	put_vm_byte(addr, len);
	for (uint8_t i = 0; i < len; i++)
		put_vm_byte(addr + 1 + i, static_cast<uint8_t>(s[i]));
}

/* Read a Pascal string from guest RAM. */
static std::string readPascalString(uint32_t addr)
{
	uint8_t len = get_vm_byte(addr);
	std::string s;
	s.reserve(len);
	for (uint8_t i = 0; i < len; i++)
		s.push_back(static_cast<char>(get_vm_byte(addr + 1 + i)));
	return s;
}

/* ── Catalog scanner ──────────────────────────────── */

static uint32_t s_nextCNID;

static void scanDirectory(const fs::path &hostDir, uint32_t parentDirID)
{
	std::error_code ec;
	for (auto &entry : fs::directory_iterator(hostDir, ec))
	{
		if (ec) break;
		std::string fname = entry.path().filename().string();
		if (fname.empty() || fname[0] == '.') continue; /* skip hidden */

		std::string macName = truncateMacName(toMacRoman(fname));
		if (macName.empty()) continue;

		CatalogEntry ce{};
		ce.cnid = s_nextCNID++;
		ce.parentDirID = parentDirID;
		ce.hostPath = entry.path().string();
		ce.macName = macName;

		if (entry.is_directory(ec))
		{
			ce.isDirectory = true;
			ce.dataForkSize = 0;
			ce.type = 0;
			ce.creator = 0;
			auto ftime = fs::last_write_time(entry.path(), ec);
			ce.crDate = ec ? 0 : toMacDate(ftime);
			ce.modDate = ce.crDate;
			uint32_t thisDirID = ce.cnid;
			s_catalog.push_back(std::move(ce));
			scanDirectory(entry.path(), thisDirID);
		}
		else if (entry.is_regular_file(ec))
		{
			ce.isDirectory = false;
			ce.dataForkSize =
				static_cast<uint32_t>(std::min(entry.file_size(ec), uintmax_t(0xFFFFFFFF)));
			mapTypeCreator(fname, ce.type, ce.creator);
			auto ftime = fs::last_write_time(entry.path(), ec);
			ce.crDate = ec ? 0 : toMacDate(ftime);
			ce.modDate = ce.crDate;
			s_catalog.push_back(std::move(ce));
		}
	}
}

static void buildCatalog()
{
	s_catalog.clear();
	s_nextCNID = kFirstCNID;
	s_openFiles.clear();
	s_wdTable.clear();
	s_nextHandle = 1;
	s_nextWD = 0x8000;

	s_sharedDir = "shared";
	std::error_code ec;
	if (!fs::is_directory(s_sharedDir, ec))
	{
		dbglog_writeCStr((char *)"ExtFS: shared/ directory not found\n");
		s_mounted = false;
		return;
	}

	scanDirectory(s_sharedDir, kRootDirID);
	s_mounted = true;
	dbglog_writeCStr((char *)"ExtFS: catalog built, ");
	dbglog_writeNum(static_cast<long>(s_catalog.size()));
	dbglog_writeCStr((char *)" entries\n");
}

/* ── Catalog lookup helpers ───────────────────────── */

static const CatalogEntry *findByCNID(uint32_t cnid)
{
	for (auto &e : s_catalog)
		if (e.cnid == cnid) return &e;
	return nullptr;
}

static const CatalogEntry *findByNameInDir(uint32_t parentDirID, const std::string &macName)
{
	for (auto &e : s_catalog)
	{
		if (e.parentDirID == parentDirID)
		{
			/* Case-insensitive compare (Mac HFS is case-insensitive) */
			if (e.macName.size() == macName.size())
			{
				bool match = true;
				for (size_t i = 0; i < macName.size(); i++)
				{
					if (tolower(static_cast<unsigned char>(e.macName[i])) !=
						tolower(static_cast<unsigned char>(macName[i])))
					{
						match = false;
						break;
					}
				}
				if (match) return &e;
			}
		}
	}
	return nullptr;
}

/* Get the Nth child (1-based) of a directory. */
static const CatalogEntry *getNthChild(uint32_t parentDirID, int index)
{
	int count = 0;
	for (auto &e : s_catalog)
	{
		if (e.parentDirID == parentDirID)
		{
			count++;
			if (count == index) return &e;
		}
	}
	return nullptr;
}

/* Count children of a directory. */
static int countChildren(uint32_t parentDirID)
{
	int count = 0;
	for (auto &e : s_catalog)
		if (e.parentDirID == parentDirID) count++;
	return count;
}

/* ── Dispatch ─────────────────────────────────────── */

void ExtnExtFSDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd)
	{
		case kExtFSVersion:
		{
			if (!s_mounted) buildCatalog();
			regParam[0] = s_mounted ? 1 : 0;
			regResult = 0;
			dbglog_writeCStr((char *)"ExtFS: version query → ");
			dbglog_writeNum(regParam[0]);
			dbglog_writeCStr((char *)"\n");
		}
		break;

		case kExtFSGetVol:
		{
			uint32_t totalFiles = 0;
			uint32_t totalBytes = 0;
			for (auto &e : s_catalog)
			{
				if (!e.isDirectory)
				{
					totalFiles++;
					totalBytes += e.dataForkSize;
				}
			}
			regParam[0] = totalFiles;
			regParam[1] = totalBytes;
			regResult = 0;
		}
		break;

		case kExtFSGetCatInfo:
		{
			/* p0 = dirID, p1 = index (1-based), p2 = name buf addr */
			uint32_t dirID = regParam[0];
			int32_t index = static_cast<int32_t>(regParam[1]);
			uint32_t nameBuf = regParam[2];

			if (index == 0)
			{
				/* Return info about the directory itself */
				if (dirID == kRootDirID)
				{
					regParam[0] = kRootDirID;
					regParam[1] = 0x10; /* directory flag */
					regParam[2] = static_cast<uint32_t>(countChildren(kRootDirID));
					regParam[3] = 0; /* parent = 0 for root */
					if (nameBuf) writePascalString(nameBuf, "Shared");
					regResult = 0;
				}
				else
				{
					const CatalogEntry *e = findByCNID(dirID);
					if (e && e->isDirectory)
					{
						regParam[0] = e->cnid;
						regParam[1] = 0x10; /* directory flag */
						regParam[2] = static_cast<uint32_t>(countChildren(e->cnid));
						regParam[3] = e->parentDirID;
						if (nameBuf) writePascalString(nameBuf, e->macName);
						regResult = 0;
					}
					else
					{
						regResult = 43; /* fnfErr */
					}
				}
			}
			else if (index > 0)
			{
				/* Indexed enumeration */
				const CatalogEntry *e = getNthChild(dirID, index);
				if (e)
				{
					regParam[0] = e->cnid;
					regParam[1] = e->isDirectory ? 0x10u : 0u;
					regParam[2] = e->isDirectory ? static_cast<uint32_t>(countChildren(e->cnid))
												 : e->dataForkSize;
					regParam[3] = e->parentDirID;
					if (nameBuf) writePascalString(nameBuf, e->macName);
					regResult = 0;
				}
				else
				{
					regResult = 43; /* fnfErr */
				}
			}
			else
			{
				regResult = 50; /* paramErr */
			}
		}
		break;

		case kExtFSGetCatInfoName:
		{
			/* p0 = dirID (parent), p1 = name ptr in guest RAM, p2 = name buf */
			uint32_t parentDir = regParam[0];
			uint32_t nameAddr = regParam[1];
			uint32_t nameBuf = regParam[2];

			std::string name = readPascalString(nameAddr);
			const CatalogEntry *e = findByNameInDir(parentDir, name);
			if (e)
			{
				regParam[0] = e->cnid;
				regParam[1] = e->isDirectory ? 0x10u : 0u;
				regParam[2] = e->isDirectory ? static_cast<uint32_t>(countChildren(e->cnid))
											 : e->dataForkSize;
				regParam[3] = e->parentDirID;
				if (nameBuf) writePascalString(nameBuf, e->macName);
				regResult = 0;
			}
			else
			{
				regResult = 43; /* fnfErr */
			}
		}
		break;

		case kExtFSOpen:
		{
			/* p0 = CNID, p1 = fork (0=data, 1=resource) */
			uint32_t cnid = regParam[0];
			/* uint32_t fork = regParam[1]; — resource forks not yet supported */

			const CatalogEntry *e = findByCNID(cnid);
			if (!e || e->isDirectory)
			{
				regResult = 43; /* fnfErr */
				break;
			}

			FILE *fp = fopen(e->hostPath.c_str(), "rb");
			if (!fp)
			{
				regResult = 43; /* fnfErr */
				break;
			}

			uint32_t handle = s_nextHandle++;
			s_openFiles[handle] = {fp, cnid};
			regParam[0] = handle;
			regResult = 0;
		}
		break;

		case kExtFSRead:
		{
			/* p0 = handle, p1 = offset, p2 = count, p3 = guest buf addr */
			uint32_t handle = regParam[0];
			uint32_t offset = regParam[1];
			uint32_t count = regParam[2];
			uint32_t guestBuf = regParam[3];

			auto it = s_openFiles.find(handle);
			if (it == s_openFiles.end())
			{
				regResult = 43; /* fnfErr */
				break;
			}

			FILE *fp = it->second.fp;
			fseek(fp, static_cast<long>(offset), SEEK_SET);

			/* Read in chunks to avoid huge stack allocations */
			uint32_t totalRead = 0;
			uint8_t buf[4096];
			while (totalRead < count)
			{
				uint32_t chunk = std::min(count - totalRead, uint32_t(sizeof(buf)));
				size_t got = fread(buf, 1, chunk, fp);
				for (size_t i = 0; i < got; i++)
					put_vm_byte(guestBuf + totalRead + static_cast<uint32_t>(i), buf[i]);
				totalRead += static_cast<uint32_t>(got);
				if (got < chunk) break; /* EOF */
			}
			regParam[0] = totalRead;
			regResult = 0;
		}
		break;

		case kExtFSClose:
		{
			/* p0 = handle */
			uint32_t handle = regParam[0];
			auto it = s_openFiles.find(handle);
			if (it != s_openFiles.end())
			{
				fclose(it->second.fp);
				s_openFiles.erase(it);
			}
			regResult = 0;
		}
		break;

		case kExtFSGetFileInfo:
		{
			/* p0 = CNID → p0=type, p1=creator, p2=crDate, p3=modDate */
			uint32_t cnid = regParam[0];
			const CatalogEntry *e = findByCNID(cnid);
			if (e)
			{
				regParam[0] = e->type;
				regParam[1] = e->creator;
				regParam[2] = e->crDate;
				regParam[3] = e->modDate;
				regResult = 0;
			}
			else
			{
				regResult = 43; /* fnfErr */
			}
		}
		break;

		case kExtFSReadDir:
		{
			/* p0 = dirID → p0 = child count */
			uint32_t dirID = regParam[0];
			regParam[0] = static_cast<uint32_t>(countChildren(dirID));
			regResult = 0;
		}
		break;

		case kExtFSObjByName:
		{
			/* p0 = parentDirID, p1 = name ptr → p0 = CNID or 0 */
			uint32_t parentDir = regParam[0];
			uint32_t nameAddr = regParam[1];
			std::string name = readPascalString(nameAddr);
			const CatalogEntry *e = findByNameInDir(parentDir, name);
			regParam[0] = e ? e->cnid : 0;
			regResult = 0;
		}
		break;

		case kExtFSGetWDInfo:
		{
			/* p0 = wdRefNum → p0 = vRefNum, p1 = dirID */
			uint32_t wdRef = regParam[0];
			auto it = s_wdTable.find(wdRef);
			if (it != s_wdTable.end())
			{
				regParam[0] = 0; /* vRefNum filled by INIT */
				regParam[1] = it->second.dirID;
				regResult = 0;
			}
			else
			{
				regResult = 43; /* fnfErr */
			}
		}
		break;

		case kExtFSOpenWD:
		{
			/* p0 = vRefNum (unused), p1 = dirID → p0 = wdRefNum */
			uint32_t dirID = regParam[1];
			uint32_t wdRef = s_nextWD++;
			s_wdTable[wdRef] = {dirID};
			regParam[0] = wdRef;
			regResult = 0;
		}
		break;

		case kExtFSCloseWD:
		{
			/* p0 = wdRefNum */
			uint32_t wdRef = regParam[0];
			s_wdTable.erase(wdRef);
			regResult = 0;
		}
		break;

		case kExtFSDbgLog:
		{
			/* Same as ClipDbgLog — reuse format */
			uint32_t fmtAddr = regParam[0];
			std::string fmt;
			for (int i = 0; i < 256; i++)
			{
				uint8_t c = get_vm_byte(fmtAddr + i);
				if (c == 0) break;
				fmt.push_back(static_cast<char>(c));
			}
			dbglog_writeCStr((char *)"ExtFS guest: ");
			dbglog_writeCStr((char *)fmt.c_str());
			dbglog_writeCStr((char *)"\n");
			regResult = 0;
		}
		break;

		default:
			regResult = 0xFFFF;
			break;
	}
}
