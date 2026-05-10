/*
	pasteboard_macos.mm — macOS NSPasteboard change counter.

	Returns the system pasteboard's changeCount, which increments
	on every clipboard mutation.  One integer read, no data copying.
*/

#import <AppKit/AppKit.h>

extern "C" long platformPasteboardChangeCount()
{
	return [[NSPasteboard generalPasteboard] changeCount];
}
