/*
	crt_effect.h — CRT monitor post-process effect

	Simulates a vintage CRT display with scanline gaps, barrel distortion
	(convex phosphor curvature), and corner vignette. Implemented as an
	off-screen FBO pass using a GLSL 1.50 fragment shader.
*/

#pragma once

#include "platform/render_effect.h"

class CRTEffect : public RenderEffect
{
public:
	CRTEffect() = default;
	~CRTEffect() override { shutdown(); }

	void   init()     override;
	void   shutdown() override;

	/*
		Renders srcTex through the CRT shader into an internal FBO sized
		dstW×dstH (physical pixels) and returns the FBO colour texture.
		Returns the original texture if the effect is not initialized.
	*/
	GLuint process(GLuint srcTex, int srcW, int srcH,
	               int dstW, int dstH) override;

private:
	void ensureFBO(int w, int h);

	GLuint program_  = 0;
	GLuint vao_      = 0;
	GLuint vbo_      = 0;
	GLuint fboId_    = 0;
	GLuint fboTexId_ = 0;
	int    fboW_     = 0;
	int    fboH_     = 0;
	int    uTex_     = -1;
	int    uTexSize_ = -1;
};
