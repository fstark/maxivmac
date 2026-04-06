#include "core/extn_clip.h"
#include "platform/common/clipboard.h"

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

/* Guest RAM access — just need these four functions from m68k */
extern uint8_t get_vm_byte(uint32_t addr);
extern void put_vm_byte(uint32_t addr, uint8_t b);

static constexpr uint16_t kClipVersion = 0x100;
static constexpr uint16_t kClipExport  = 0x101;
static constexpr uint16_t kClipImport  = 0x102;
static constexpr uint16_t kClipHasData = 0x103;
static constexpr uint16_t kClipGetLen  = 0x104;
static constexpr uint16_t kClipSeqNo   = 0x105;
static constexpr uint16_t kClipKVSet   = 0x106;
static constexpr uint16_t kClipKVGet   = 0x107;

static std::string s_clipCache;
static std::unordered_map<uint32_t, uint32_t> s_kvStore;
static uint32_t    s_clipSeqNo = 0;
static std::string s_lastClipText;

static void refreshCache()
{
	s_clipCache = hostClipGetTextMacRoman();
}

void extnClipDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd) {
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
				if (current != s_lastClipText) {
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
				if (s_clipCache.empty()) {
					refreshCache();
				}
				uint32_t guestAddr = regParam[0];
				uint32_t capacity  = regParam[1];
				uint32_t actual = static_cast<uint32_t>(
					std::min(static_cast<size_t>(capacity), s_clipCache.size()));
				for (uint32_t i = 0; i < actual; i++) {
					put_vm_byte(guestAddr + i,
						static_cast<uint8_t>(s_clipCache[i]));
				}
				regParam[1] = actual;
				regResult = 0;
			}
			break;

		case kClipExport:
			{
				uint32_t guestAddr = regParam[0];
				uint32_t count     = regParam[1];
				std::vector<uint8_t> buf(count);
				for (uint32_t i = 0; i < count; i++) {
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
				s_lastClipText.assign(
					reinterpret_cast<char *>(buf.data()), count);
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

		default:
			regResult = 0xFFFF;
			break;
	}
}
