#include "core/extn_clip.h"
#include "core/extn_clip_pict.h"
#include "core/host_pasteboard.h"
#include "core/diag.h"
#include "debugger/debugger.h"
#include "platform/clipboard_image.h"
#include "platform/common/clipboard.h"
#include "util/macroman.h"

#include <string>
#include <vector>
#include <deque>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <unordered_map>
#include <mutex>

#ifdef HAVE_SDL
#include <SDL3/SDL.h>
#endif

#include "stb_image.h"

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
static constexpr uint16_t kPictExport = 0x109;
static constexpr uint16_t kPictHasImage = 0x10A;
static constexpr uint16_t kPictImport = 0x10B;
static constexpr uint16_t kClipCommit = 0x10C;

static std::unordered_map<uint32_t, uint32_t> s_kvStore;

/* Staging buffers for guest→host export */
static std::string s_stagedText;
static bool s_hasStagedText = false;

void ExtnClipReset()
{
	s_kvStore.clear();
	s_stagedText.clear();
	s_hasStagedText = false;
	ExtnPictReset();
}

/* ── Debug console log buffer ──────────────────────── */

static constexpr size_t kMaxConsoleLines = 2048;
static std::deque<std::string> s_consoleLines;

const std::deque<std::string> &extnDbgConsoleLines()
{
	return s_consoleLines;
}

void ExtnDbgConsoleClear()
{
	s_consoleLines.clear();
}

void guestConsoleAppend(const std::string &line)
{
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

/* Read a Pascal string from guest RAM (first byte = length). */
static std::string readGuestPascalString(uint32_t addr)
{
	if (addr == 0) return "<null>";
	uint8_t len = get_vm_byte(addr);
	std::string s;
	s.reserve(len);
	for (uint8_t i = 0; i < len; i++)
		s.push_back(static_cast<char>(get_vm_byte(addr + 1 + i)));
	return s;
}

/*
	Format a guest printf-style string with up to 6 long args.
	Supported: %lx (hex), %ld (decimal), %lu (unsigned decimal),
			   %s (guest string pointer), %% (literal %).
*/
std::string guestFormatLog(uint32_t fmtAddr, uint32_t args[7])
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

		/* consume optional flags/width/precision (e.g. "02", "-8") */
		char fmtspec[16];
		int fpos = 0;
		fmtspec[fpos++] = '%';
		while (i < fmt.size() &&
			   (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '0' || fmt[i] == '#' ||
				(fmt[i] >= '1' && fmt[i] <= '9') || fmt[i] == '.'))
		{
			if (fpos < 12) fmtspec[fpos++] = fmt[i];
			i++;
		}
		if (i >= fmt.size()) break;

		/* consume optional 'l' prefix */
		if (fmt[i] == 'l' && i + 1 < fmt.size()) i++;

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
				fmtspec[fpos++] = 'x';
				fmtspec[fpos] = '\0';
				snprintf(numbuf, sizeof(numbuf), fmtspec, val);
				out += numbuf;
				break;
			case 'X':
				fmtspec[fpos++] = 'X';
				fmtspec[fpos] = '\0';
				snprintf(numbuf, sizeof(numbuf), fmtspec, val);
				out += numbuf;
				break;
			case 'd':
				fmtspec[fpos++] = 'd';
				fmtspec[fpos] = '\0';
				snprintf(numbuf, sizeof(numbuf), fmtspec, static_cast<int32_t>(val));
				out += numbuf;
				break;
			case 'u':
				fmtspec[fpos++] = 'u';
				fmtspec[fpos] = '\0';
				snprintf(numbuf, sizeof(numbuf), fmtspec, val);
				out += numbuf;
				break;
			case 's':
				out += readGuestString(val);
				break;
			case 'S':
				out += readGuestPascalString(val);
				break;
			default:
				out.push_back('%');
				out.push_back(fmt[i]);
				break;
		}
	}
	return out;
}

void ExtnClipDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd)
	{
		case kClipVersion:
			regParam[0] = 3;
			regResult = 0;
			break;

		case kClipHasData:
		{
			auto &pb = GetHostPasteboard();
			std::lock_guard lock(pb.mu);
			regParam[0] = pb.text.empty() ? 0 : 1;
			regResult = 0;
		}
		break;

		case kClipGetLen:
		{
			auto &pb = GetHostPasteboard();
			std::lock_guard lock(pb.mu);
			regParam[0] = static_cast<uint32_t>(pb.text.size());
			regResult = 0;
		}
		break;

		case kClipSeqNo:
		{
			auto &pb = GetHostPasteboard();
			std::lock_guard lock(pb.mu);
			regParam[0] = pb.seq;
			DIAG(CLIP, "ClipSeqNo: returning seq=%u\n", pb.seq);
			regResult = 0;
		}
		break;

		case kClipImport:
		{
			auto &pb = GetHostPasteboard();
			std::string text;
			{
				std::lock_guard lock(pb.mu);
				text = pb.text;
			}
			uint32_t guestAddr = regParam[0];
			uint32_t capacity = regParam[1];
			uint32_t actual =
				static_cast<uint32_t>(std::min(static_cast<size_t>(capacity), text.size()));
			DIAG(CLIP, "ClipImport: %u bytes → guest $%08X\n", actual, guestAddr);
			for (uint32_t i = 0; i < actual; i++)
				put_vm_byte(guestAddr + i, static_cast<uint8_t>(text[i]));
			regParam[1] = actual;
			regResult = 0;
		}
		break;

		case kClipExport:
		{
			uint32_t guestAddr = regParam[0];
			uint32_t count = regParam[1];
			DIAG(CLIP, "ClipExport: staged %u bytes text\n", count);
			s_stagedText.resize(count);
			for (uint32_t i = 0; i < count; i++)
				s_stagedText[i] = static_cast<char>(get_vm_byte(guestAddr + i));
			s_hasStagedText = true;
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
			std::string line = guestFormatLog(regParam[0], regParam);
			DIAG(CLIP, "%s\n", line.c_str());
			guestConsoleAppend(line);
			regResult = 0;
		}
		break;

		case kPictExport:
			HandlePictExport(regParam, regResult);
			break;
		case kPictHasImage:
			HandlePictHasImage(regParam, regResult);
			break;
		case kPictImport:
			HandlePictImport(regParam, regResult);
			break;

		case kClipCommit:
		{
			auto &pb = GetHostPasteboard();
			bool hasPng = HasStagedPng();

			if (!s_hasStagedText && !hasPng)
			{
				std::lock_guard lock(pb.mu);
				DIAG(CLIP, "ClipCommit: nothing staged, returning seq=%u\n", pb.seq);
				regParam[0] = pb.seq;
				regResult = 0;
				break;
			}

			/* Convert and publish to SDL (outside lock) */
			std::string textUtf8;
			if (s_hasStagedText)
				textUtf8 = UTF8FromMacRoman(
					{reinterpret_cast<const uint8_t *>(s_stagedText.data()), s_stagedText.size()});

			std::vector<uint8_t> pngData;
			if (hasPng) pngData = TakeStagedPng();

			if (s_hasStagedText) SDL_SetClipboardText(textUtf8.c_str());
			if (!pngData.empty()) HostClipSetImage(pngData.data(), pngData.size());

			/* Update pasteboard so SDL event sees identical content */
			{
				std::lock_guard lock(pb.mu);
				uint32_t oldSeq = pb.seq;
				if (s_hasStagedText) pb.text = std::move(s_stagedText);
				if (!pngData.empty())
				{
					pb.png = std::move(pngData);
					int comp = 0;
					stbi_info_from_memory(pb.png.data(), static_cast<int>(pb.png.size()), &pb.imgW,
										  &pb.imgH, &comp);
				}
				pb.seq++;
				DIAG(CLIP, "ClipCommit: publishing text=%zuB png=%zuB → seq %u→%u\n",
					 pb.text.size(), pb.png.size(), oldSeq, pb.seq);
				regParam[0] = pb.seq;
			}

			s_hasStagedText = false;
			s_stagedText.clear();
			regResult = 0;
		}
		break;

		default:
			regResult = 0xFFFF;
			break;
	}
}
