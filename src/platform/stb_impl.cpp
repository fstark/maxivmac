/*
	stb_impl.cpp — Single-file implementation of stb_image_write.

	Note: STB_IMAGE_IMPLEMENTATION lives in imgui_launcher.cpp
	(pulled in by the ImGui backend).  Only the write side is here.
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma GCC diagnostic pop
