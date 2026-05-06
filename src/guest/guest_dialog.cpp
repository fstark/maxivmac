/*
	guest_dialog.cpp — Dialog Manager introspection.

	Reads the front window's dialog item list from guest memory
	using the standard DialogRecord / DITL layout (IM I-400..I-421).
*/
#include "guest/guest_dialog.h"
#include "util/macroman.h"
#include <algorithm>
#include <cctype>

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

namespace guest
{

/* ── Low-memory globals ───────────────────────────────── */

static constexpr GuestAddr kWindowList = 0x09D6; // Ptr to front window

/* ── WindowRecord offsets ─────────────────────────────── */

static constexpr int kPortRect = 16;	   // Rect (8 bytes) within GrafPort
static constexpr int kPortBoundsTop = 8;   // BitMap.bounds.top
static constexpr int kPortBoundsLeft = 10; // BitMap.bounds.left
static constexpr int kWindowKind = 108;

/* ── DialogRecord offsets (relative to start of WindowRecord) ─── */

static constexpr int kDlgItems = 156;	 // Handle to item list
static constexpr int kDlgADefItem = 168; // INTEGER — default button item number

/* ── Constants ────────────────────────────────────────── */

static constexpr int16_t kDialogKind = 2;

/* ── Helpers ──────────────────────────────────────────── */

static Rect readRect(GuestAddr addr)
{
	Rect r;
	r.top = static_cast<int16_t>(get_vm_word(addr));
	r.left = static_cast<int16_t>(get_vm_word(addr + 2));
	r.bottom = static_cast<int16_t>(get_vm_word(addr + 4));
	r.right = static_cast<int16_t>(get_vm_word(addr + 6));
	return r;
}

static std::string readBytes(GuestAddr addr, int len)
{
	std::string s(len, '\0');
	for (int i = 0; i < len; i++)
		s[i] = static_cast<char>(get_vm_byte(addr + i));
	return s;
}

/* ── DITL walk ────────────────────────────────────────── */

/*
	DITL in-memory format (from IM I-421):
	  word: itemCount - 1 (number of items minus one)
	  For each item:
		long:  reserved (handle placeholder, used at runtime)
		Rect:  bounds (8 bytes)
		byte:  type (high bit = disabled flag, low 7 = item type)
		byte:  dataLen
		byte[dataLen]: item data (Pascal string for buttons/text, etc.)
		[pad to even boundary]
*/

static std::vector<DialogItem> walkDITL(GuestAddr itemsPtr)
{
	std::vector<DialogItem> items;

	int16_t count = static_cast<int16_t>(get_vm_word(itemsPtr)) + 1;
	GuestAddr p = itemsPtr + 2;

	for (int i = 0; i < count; i++)
	{
		DialogItem item;
		item.index = i + 1;

		p += 4; // skip reserved handle
		item.bounds = readRect(p);
		p += 8;

		uint8_t typeByte = get_vm_byte(p);
		p += 1;

		item.enabled = (typeByte & 0x80) == 0;
		uint8_t rawType = typeByte & 0x7F;

		// Map raw type code to our enum
		switch (rawType)
		{
			case 4:
				item.type = DialogItemType::Button;
				break;
			case 5:
				item.type = DialogItemType::CheckBox;
				break;
			case 6:
				item.type = DialogItemType::RadioButton;
				break;
			case 7:
				item.type = DialogItemType::ResControl;
				break;
			case 8:
				item.type = DialogItemType::StaticText;
				break;
			case 16:
				item.type = DialogItemType::EditText;
				break;
			case 32:
				item.type = DialogItemType::Icon;
				break;
			case 64:
				item.type = DialogItemType::Picture;
				break;
			default:
				item.type = DialogItemType::UserItem;
				break;
		}

		uint8_t dataLen = get_vm_byte(p);
		p += 1;

		// For buttons, static text, and edit text: data is text content
		if (rawType == 4 || rawType == 5 || rawType == 6 || rawType == 8 || rawType == 16)
		{
			std::string raw = readBytes(p, dataLen);
			std::vector<uint8_t> bytes(raw.begin(), raw.end());
			item.text = UTF8FromMacRoman(bytes);
		}

		p += dataLen;
		// Pad to even boundary
		if (dataLen % 2 != 0) p += 1;

		items.push_back(std::move(item));
	}

	return items;
}

/* ── Public API ───────────────────────────────────────── */

std::optional<DialogInfo> readFrontDialog()
{
	GuestAddr frontWin = get_vm_long(kWindowList);
	if (frontWin == 0) return std::nullopt;

	int16_t kind = static_cast<int16_t>(get_vm_word(frontWin + kWindowKind));
	if (kind != kDialogKind) return std::nullopt;

	DialogInfo info;
	info.windowPtr = frontWin;
	info.portRect = readRect(frontWin + kPortRect);

	// Global origin = -portBounds.topLeft (the window's global position)
	info.origin.h = -static_cast<int16_t>(get_vm_word(frontWin + kPortBoundsLeft));
	info.origin.v = -static_cast<int16_t>(get_vm_word(frontWin + kPortBoundsTop));

	info.defaultItem = static_cast<int16_t>(get_vm_word(frontWin + kDlgADefItem));

	// Dereference items handle
	GuestAddr itemsHandle = get_vm_long(frontWin + kDlgItems);
	if (itemsHandle == 0) return std::nullopt;
	GuestAddr itemsPtr = get_vm_long(itemsHandle);
	if (itemsPtr == 0) return std::nullopt;

	info.items = walkDITL(itemsPtr);
	return info;
}

const DialogItem *findButton(const DialogInfo &dlg, std::string_view title)
{
	// Case-insensitive substring match
	std::string lower_title(title);
	std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	for (const auto &item : dlg.items)
	{
		if (item.type != DialogItemType::Button) continue;

		std::string lower_item(item.text);
		std::transform(lower_item.begin(), lower_item.end(), lower_item.begin(),
					   [](unsigned char c) { return std::tolower(c); });

		if (lower_item.find(lower_title) != std::string::npos) return &item;
	}
	return nullptr;
}

Point itemCenter(const DialogInfo &dlg, const DialogItem &item)
{
	Point pt;
	pt.h = dlg.origin.h + item.bounds.centerH();
	pt.v = dlg.origin.v + item.bounds.centerV();
	return pt;
}

} // namespace guest
