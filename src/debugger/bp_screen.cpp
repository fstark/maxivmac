/*
	bp_screen.cpp — Screen breakpoint matching implementation

	ScreenMatcher loads a reference PNG and compares it against the
	live framebuffer on each tick. CheckScreenBreakpoints() iterates
	active breakpoints and fires when a match occurs.
*/
#include "debugger/bp_screen.h"
#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "platform/emulator_shell.h"
#include "platform/common/path_utils.h"
#include "platform/platform.h"

#include "stb_image.h"
#include "stb_image_write.h"

/*
	Load reference PNG and convert RGBA to ARGB8888 format.
	Alpha channel is set to opaque (0xFF) and ignored during comparison.
*/
bool ScreenMatcher::loadReference(const std::filesystem::path &png)
{
	int w, h, channels;
	auto pngPath = path_str(png);
	unsigned char *data = stbi_load(pngPath.c_str(), &w, &h, &channels, 4);
	if (!data) return false;

	refWidth = w;
	refHeight = h;
	refPixels.resize(w * h);

	// stbi loads as RGBA — convert to ARGB for comparison with framebuffer
	const uint8_t *src = data;
	for (int i = 0; i < w * h; ++i)
	{
		uint8_t r = src[i * 4 + 0];
		uint8_t g = src[i * 4 + 1];
		uint8_t b = src[i * 4 + 2];
		refPixels[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
	}

	stbi_image_free(data);
	return true;
}

/*
	Compare framebuffer to reference pixel-by-pixel (RGB only).
	Return true if at least threshold% of pixels match.
*/
bool ScreenMatcher::matches(const uint8_t *framebuffer, int width, int height) const
{
	if (!framebuffer) return false;
	if (refWidth != width || refHeight != height) return false;
	if (refPixels.empty()) return false;

	int total = width * height;
	int matched = 0;
	const uint32_t *fb = reinterpret_cast<const uint32_t *>(framebuffer);

	for (int i = 0; i < total; ++i)
	{
		if ((fb[i] & 0x00FFFFFFu) == (refPixels[i] & 0x00FFFFFFu)) ++matched;
	}

	float pct = static_cast<float>(matched) * 100.0f / static_cast<float>(total);
	return pct >= threshold;
}

/*
	Fire screen breakpoints when framebuffer matches a reference PNG.
	Stops emulation and executes associated commands. Only one breakpoint
	fires per tick even if multiple match.
*/
void CheckScreenBreakpoints()
{
	auto *dbg = Debugger::instance();
	if (!dbg) return;

	auto &bps = dbg->breakpoints();
	bool hasScreenBp = false;
	for (const auto &bp : bps)
	{
		if (bp.kind == Debugger::Breakpoint::Kind::Screen && bp.enabled)
		{
			hasScreenBp = true;
			break;
		}
	}
	if (!hasScreenBp) return;

	if (!g_shell) return;
	const uint8_t *fb = g_shell->getFramebuffer();
	if (!fb) return;
	int width = static_cast<int>(g_screenWidth);
	int height = static_cast<int>(g_screenHeight);

	for (size_t i = 0; i < bps.size(); ++i)
	{
		auto &bp = bps[i];
		if (bp.kind != Debugger::Breakpoint::Kind::Screen) continue;
		if (!bp.enabled) continue;

		if (bp.screenMatcher.matches(fb, width, height))
		{
			dbg->io().write("Breakpoint %u: screen match\n", bp.id);

			bool wasTemporary = bp.temporary;
			bool wasScriptOwned = bp.scriptOwned;
			uint32_t bpId = bp.id;

			if (!bp.commands.empty()) dbg->executeCommands(bp.commands);

			if (wasTemporary) dbg->deleteById(bpId);

			dbg->stop("");

			if (wasScriptOwned) dbg->tryResumeScript(nullptr);

			return; // only fire one per tick
		}
	}
}

/*
	Save current framebuffer to PNG file.
	Converts ARGB8888 to RGBA for stb_image_write compatibility.
*/
bool SaveScreenshot(const std::filesystem::path &path)
{
	if (!g_shell) return false;
	const uint8_t *fb = g_shell->getFramebuffer();
	if (!fb) return false;

	int width = static_cast<int>(g_screenWidth);
	int height = static_cast<int>(g_screenHeight);

	std::vector<uint8_t> rgba(width * height * 4);
	const uint32_t *src = reinterpret_cast<const uint32_t *>(fb);
	for (int i = 0; i < width * height; ++i)
	{
		uint32_t pixel = src[i];
		rgba[i * 4 + 0] = (pixel >> 16) & 0xFF;
		rgba[i * 4 + 1] = (pixel >> 8) & 0xFF;
		rgba[i * 4 + 2] = pixel & 0xFF;
		rgba[i * 4 + 3] = 0xFF;
	}

	auto pngPath = path_str(path);
	return stbi_write_png(pngPath.c_str(), width, height, 4, rgba.data(), width * 4) != 0;
}
