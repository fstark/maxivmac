/*
	guest_dialog.h — Dialog Manager introspection from host side.
*/
#pragma once
#include "guest/guest_types.h"
#include <optional>
#include <string>
#include <vector>

namespace guest
{

enum class DialogItemType : uint8_t
{
	UserItem = 0,
	Button = 4,
	CheckBox = 5,
	RadioButton = 6,
	ResControl = 7,
	StaticText = 8,
	EditText = 16,
	Icon = 32,
	Picture = 64,
};

struct DialogItem
{
	int index; // 1-based item number
	DialogItemType type;
	bool enabled;
	Rect bounds;	  // local coordinates (relative to dialog window)
	std::string text; // title for buttons, content for staticText/editText
};

struct DialogInfo
{
	GuestAddr windowPtr; // guest address of the WindowRecord
	Rect portRect;		 // window's portRect
	Point origin;		 // window's global top-left
	int16_t defaultItem; // aDefItem (1-based, 0 = none)
	std::vector<DialogItem> items;
};

/// Read the front window's dialog item list.
/// Returns nullopt if front window is not a dialog (windowKind != 2).
std::optional<DialogInfo> readFrontDialog();

/// Find a button by title (case-insensitive substring match).
/// Returns nullptr if not found.
const DialogItem *findButton(const DialogInfo &dlg, std::string_view title);

/// Compute global pixel coordinates for an item's center.
Point itemCenter(const DialogInfo &dlg, const DialogItem &item);

} // namespace guest
