/*
	gl_texture.cpp — GL texture loading utility
*/

#include "platform/gl_texture.h"
#include "stb_image.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

uint32_t loadPngTexture(const char *path)
{
	int w, h, channels;
	unsigned char *pixels = stbi_load(path, &w, &h, &channels, 4);
	if (!pixels) return 0;

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	stbi_image_free(pixels);

	return tex;
}
