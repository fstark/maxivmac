/*
	render_effect.h — Optional post-process effect interface

	An optional RenderEffect sits between the raw emulator texture and
	ImGui::Image(). ImGuiBackend holds at most one effect at a time;
	null means plain pass-through.
*/

#pragma once

#include <imgui.h>

/* GL opaque integer handle — same underlying type on all platforms. */
typedef unsigned int GLuint;

class RenderEffect
{
public:
	virtual ~RenderEffect() = default;

	/* Called once after the GL context is ready. */
	virtual void init() = 0;

	/* Called before the GL context is torn down. */
	virtual void shutdown() = 0;

	/* Render srcTex through the effect; returns the output texture ID.
	   dstW/dstH are the physical pixel dimensions of the display area. */
	virtual GLuint process(GLuint srcTex, int srcW, int srcH,
	                       int dstW, int dstH) = 0;

	/* Background color for the letterbox surround in fullscreen mode. */
	virtual ImVec4 surroundColor() const { return {0.0f, 0.0f, 0.0f, 1.0f}; }
};
