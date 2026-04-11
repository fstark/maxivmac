#include "core/extn_clip.h"
#include "platform/common/clipboard.h"

#include <string>
#include <vector>
#include <deque>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <unordered_map>
#include <mutex>

/* Guest RAM access — just need these four functions from m68k */
extern uint8_t get_vm_byte(uint32_t addr);
extern void put_vm_byte(uint32_t addr, uint8_t b);

static constexpr uint16_t kClipVersion = 0x100;
static constexpr uint16_t kClipExport = 0x101;
static constexpr uint16_t kClipImport = 0x102;
static constexpr uint16_t kClipHasData = 0x103;
static constexpr uint16_t kClipGetLen = 0x104;
static constexpr uint16_t kClipSeqNo = 0x105;
static constexpr uint16_t kClipKVSet = 0x106;
static constexpr uint16_t kClipKVGet = 0x107;
static constexpr uint16_t kClipDbgLog = 0x108;

static std::string s_clipCache;
static std::unordered_map<uint32_t, uint32_t> s_kvStore;
static uint32_t s_clipSeqNo = 0;
static std::string s_lastClipText;

/* ── Debug console log buffer ──────────────────────── */

static constexpr size_t kMaxConsoleLines = 2048;
static std::deque<std::string> s_consoleLines;

const std::deque<std::string> &extnDbgConsoleLines()
{
	return s_consoleLines;
}

void extnDbgConsoleClear()
{
	s_consoleLines.clear();
}

static void consoleAppend(const std::string &line)
{
	fprintf(stderr, "[GUEST] %s\n", line.c_str());
	s_consoleLines.push_back(line);
	while (s_consoleLines.size() > kMaxConsoleLines)
	{
		s_consoleLines.pop_front();
	}
}

/* Read a C string from guest RAM (max 256 bytes, MacRoman). */
static std::string readGuestString(uint32_t addr, size_t maxLen = 256)
{
	std::string s;
	s.reserve(maxLen);
	for (size_t i = 0; i < maxLen; i++)
	{
		uint8_t ch = get_vm_byte(addr + static_cast<uint32_t>(i));
		if (ch == 0) break;
		s.push_back(static_cast<char>(ch));
	}
	return s;
}

/*
	Format a guest printf-style string with up to 6 long args.
	Supported: %lx (hex), %ld (decimal), %lu (unsigned decimal),
			   %s (guest string pointer), %% (literal %).
*/
static std::string formatGuestLog(uint32_t fmtAddr, uint32_t args[7])
{
	std::string fmt = readGuestString(fmtAddr);
	std::string out;
	out.reserve(fmt.size() * 2);
	int argIdx = 0;
	char numbuf[20];

	for (size_t i = 0; i < fmt.size(); i++)
	{
		if (fmt[i] != '%')
		{
			out.push_back(fmt[i]);
			continue;
		}
		/* look at next char(s) */
		i++;
		if (i >= fmt.size()) break;

		if (fmt[i] == '%')
		{
			out.push_back('%');
			continue;
		}

		/* consume optional 'l' prefix */
		bool hasL = false;
		if (fmt[i] == 'l' && i + 1 < fmt.size())
		{
			hasL = true;
			i++;
		}
		(void)hasL;

		if (argIdx > 5)
		{
			out += "<?>";
			continue;
		}
		uint32_t val = args[argIdx + 1]; /* args[1..6] = p1..p6 */
		argIdx++;

		switch (fmt[i])
		{
			case 'x':
			case 'X':
				snprintf(numbuf, sizeof(numbuf), "%08X", val);
				out += numbuf;
				break;
			case 'd':
				snprintf(numbuf, sizeof(numbuf), "%ld",
						 static_cast<long>(static_cast<int32_t>(val)));
				out += numbuf;
				break;
			case 'u':
				snprintf(numbuf, sizeof(numbuf), "%lu", static_cast<unsigned long>(val));
				out += numbuf;
				break;
			case 's':
				out += readGuestString(val);
				break;
			default:
				out.push_back('%');
				out.push_back(fmt[i]);
				break;
		}
	}
	return out;
}

static void refreshCache()
{
	s_clipCache = hostClipGetTextMacRoman();
}

void extnClipDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd)
	{
		case kClipVersion:
			regParam[0] = 2;
			regResult = 0;
			break;

		case kClipHasData:
			regParam[0] = hostClipHasText() ? 1 : 0;
			regResult = 0;
			break;

		case kClipGetLen:
			refreshCache();
			regParam[0] = static_cast<uint32_t>(s_clipCache.size());
			regResult = 0;
			break;

		case kClipSeqNo:
		{
			std::string current = hostClipGetTextMacRoman();
			if (current != s_lastClipText)
			{
				s_lastClipText = current;
				s_clipCache = current;
				s_clipSeqNo++;
			}
			regParam[0] = s_clipSeqNo;
			regResult = 0;
		}
		break;

		case kClipImport:
		{
			if (s_clipCache.empty())
			{
				refreshCache();
			}
			uint32_t guestAddr = regParam[0];
			uint32_t capacity = regParam[1];
			uint32_t actual =
				static_cast<uint32_t>(std::min(static_cast<size_t>(capacity), s_clipCache.size()));
			for (uint32_t i = 0; i < actual; i++)
			{
				put_vm_byte(guestAddr + i, static_cast<uint8_t>(s_clipCache[i]));
			}
			regParam[1] = actual;
			regResult = 0;
		}
		break;

		case kClipExport:
		{
			uint32_t guestAddr = regParam[0];
			uint32_t count = regParam[1];
			std::vector<uint8_t> buf(count);
			for (uint32_t i = 0; i < count; i++)
			{
				buf[i] = get_vm_byte(guestAddr + i);
			}
			hostClipSetText(buf.data(), count);

			/*
				Update cache to match what we just exported.
				This prevents ClipSeqNo from seeing the export
				as a "new" host change (feedback loop).
				The cache is Mac Roman with CRs — same encoding
				as the buffer we just read from guest RAM.
			*/
			s_lastClipText.assign(reinterpret_cast<char *>(buf.data()), count);
			s_clipCache = s_lastClipText;

			regResult = 0;
		}
		break;

		case kClipKVSet:
			s_kvStore[regParam[0]] = regParam[1];
			regResult = 0;
			break;

		case kClipKVGet:
		{
			auto it = s_kvStore.find(regParam[0]);
			regParam[0] = (it != s_kvStore.end()) ? it->second : 0;
			regResult = 0;
		}
		break;

		case kClipDbgLog:
		{
			std::string line = formatGuestLog(regParam[0], regParam);
			consoleAppend(line);
			regResult = 0;
		}
		break;

		default:
			regResult = 0xFFFF;
			break;
	}
}
