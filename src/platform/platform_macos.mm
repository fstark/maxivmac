/*
	platform_macos.mm — macOS AppKit helpers called from imgui_backend.

	platformPasteboardChangeCount — returns the system pasteboard's
	changeCount (increments on every clipboard mutation; one integer
	read, no data copying).

	platformRemoveMenuKeyEquivalents — strips the Cmd+Q and Cmd+W key
	equivalents from the SDL-generated macOS menu bar so those combos
	flow through SDL as normal keyboard events and are forwarded to the
	guest instead of triggering OS-level quit / window-close actions.
*/

#import <AppKit/AppKit.h>

extern "C" long platformPasteboardChangeCount()
{
	return [[NSPasteboard generalPasteboard] changeCount];
}

extern "C" void platformRemoveMenuKeyEquivalents()
{
	NSMenu *mainMenu = [NSApp mainMenu];
	if (!mainMenu) return;
	for (NSMenuItem *topItem in mainMenu.itemArray) {
		NSMenu *sub = topItem.submenu;
		if (!sub) continue;
		for (NSMenuItem *item in sub.itemArray) {
			NSString *key = item.keyEquivalent;
			if ([key isEqualToString:@"q"] || [key isEqualToString:@"w"])
				item.keyEquivalent = @"";
		}
	}
}
