/*
	test_stb_image_impl.cpp — stb_image implementation for the test binary.

	Provides PNG decode (stb_image) and PNG encode (stb_image_write)
	so that pict_convert.cpp and roundtrip tests can link.
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma GCC diagnostic pop
