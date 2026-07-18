/*
	gl_texture.h — GL texture loading utility
*/

#pragma once

#include <cstdint>

/* Load a PNG file into a GL texture. Returns the GL texture ID, or 0 on failure.
   Uses GL_NEAREST filtering (pixel-perfect). Must be called with a valid GL context. */
uint32_t loadPngTexture(const char *path);
