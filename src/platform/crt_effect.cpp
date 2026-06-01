/*
	crt_effect.cpp — CRT monitor post-process effect

	Implements CRTEffect: scanline gaps, barrel distortion, and a corner
	vignette rendered as an off-screen FBO pass.

	GL 3.x entry points are linked directly on Apple (OpenGL/gl3.h) and
	loaded at runtime via SDL_GL_GetProcAddress on other platforms.
*/

#include "platform/crt_effect.h"

#if defined(__APPLE__)
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#elif defined(_WIN32)
#  include <GL/gl.h>
#  include <GL/glext.h>
#else
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include <SDL3/SDL.h>
#include <cstdio>
#include <algorithm>

/* ── Non-Apple GL 2.x/3.x function pointers ─────────────────────────────────
   On Apple the symbols are direct exports from OpenGL.framework (via gl3.h).
   On other platforms we load them via SDL_GL_GetProcAddress.                */
#ifndef __APPLE__
static PFNGLACTIVETEXTUREPROC           s_glActiveTexture           = nullptr;
static PFNGLCREATESHADERPROC            s_glCreateShader            = nullptr;
static PFNGLSHADERSOURCEPROC            s_glShaderSource            = nullptr;
static PFNGLCOMPILESHADERPROC           s_glCompileShader           = nullptr;
static PFNGLGETSHADERIVPROC             s_glGetShaderiv             = nullptr;
static PFNGLGETSHADERINFOLOGPROC        s_glGetShaderInfoLog        = nullptr;
static PFNGLCREATEPROGRAMPROC           s_glCreateProgram           = nullptr;
static PFNGLATTACHSHADERPROC            s_glAttachShader            = nullptr;
static PFNGLLINKPROGRAMPROC             s_glLinkProgram             = nullptr;
static PFNGLGETPROGRAMIVPROC            s_glGetProgramiv            = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC       s_glGetProgramInfoLog       = nullptr;
static PFNGLDELETESHADERPROC            s_glDeleteShader            = nullptr;
static PFNGLDELETEPROGRAMPROC           s_glDeleteProgram           = nullptr;
static PFNGLUSEPROGRAMPROC              s_glUseProgram              = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC      s_glGetUniformLocation      = nullptr;
static PFNGLUNIFORM1IPROC               s_glUniform1i               = nullptr;
static PFNGLUNIFORM2FPROC               s_glUniform2f               = nullptr;
static PFNGLGETATTRIBLOCATIONPROC       s_glGetAttribLocation       = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC     s_glVertexAttribPointer     = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC s_glEnableVertexAttribArray = nullptr;
static PFNGLGENVERTEXARRAYSPROC         s_glGenVertexArrays         = nullptr;
static PFNGLBINDVERTEXARRAYPROC         s_glBindVertexArray         = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC      s_glDeleteVertexArrays      = nullptr;
static PFNGLGENBUFFERSPROC              s_glGenBuffers              = nullptr;
static PFNGLBINDBUFFERPROC              s_glBindBuffer              = nullptr;
static PFNGLBUFFERDATAPROC              s_glBufferData              = nullptr;
static PFNGLDELETEBUFFERSPROC           s_glDeleteBuffers           = nullptr;
static PFNGLGENFRAMEBUFFERSPROC         s_glGenFramebuffers         = nullptr;
static PFNGLBINDFRAMEBUFFERPROC         s_glBindFramebuffer         = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC    s_glFramebufferTexture2D    = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC  s_glCheckFramebufferStatus  = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC      s_glDeleteFramebuffers      = nullptr;

static bool loadGLFunctions()
{
#define LOAD(type, sym)                                                             \
	do {                                                                            \
		s_##sym = (type)SDL_GL_GetProcAddress(#sym);                                \
		if (!s_##sym) { fprintf(stderr, "CRT: missing %s\n", #sym); return false; } \
	} while (0)
	LOAD(PFNGLACTIVETEXTUREPROC,           glActiveTexture);
	LOAD(PFNGLCREATESHADERPROC,            glCreateShader);
	LOAD(PFNGLSHADERSOURCEPROC,            glShaderSource);
	LOAD(PFNGLCOMPILESHADERPROC,           glCompileShader);
	LOAD(PFNGLGETSHADERIVPROC,             glGetShaderiv);
	LOAD(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog);
	LOAD(PFNGLCREATEPROGRAMPROC,           glCreateProgram);
	LOAD(PFNGLATTACHSHADERPROC,            glAttachShader);
	LOAD(PFNGLLINKPROGRAMPROC,             glLinkProgram);
	LOAD(PFNGLGETPROGRAMIVPROC,            glGetProgramiv);
	LOAD(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog);
	LOAD(PFNGLDELETESHADERPROC,            glDeleteShader);
	LOAD(PFNGLDELETEPROGRAMPROC,           glDeleteProgram);
	LOAD(PFNGLUSEPROGRAMPROC,              glUseProgram);
	LOAD(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation);
	LOAD(PFNGLUNIFORM1IPROC,               glUniform1i);
	LOAD(PFNGLUNIFORM2FPROC,               glUniform2f);
	LOAD(PFNGLGETATTRIBLOCATIONPROC,       glGetAttribLocation);
	LOAD(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer);
	LOAD(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
	LOAD(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays);
	LOAD(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray);
	LOAD(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays);
	LOAD(PFNGLGENBUFFERSPROC,              glGenBuffers);
	LOAD(PFNGLBINDBUFFERPROC,              glBindBuffer);
	LOAD(PFNGLBUFFERDATAPROC,              glBufferData);
	LOAD(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers);
	LOAD(PFNGLGENFRAMEBUFFERSPROC,         glGenFramebuffers);
	LOAD(PFNGLBINDFRAMEBUFFERPROC,         glBindFramebuffer);
	LOAD(PFNGLFRAMEBUFFERTEXTURE2DPROC,    glFramebufferTexture2D);
	LOAD(PFNGLCHECKFRAMEBUFFERSTATUSPROC,  glCheckFramebufferStatus);
	LOAD(PFNGLDELETEFRAMEBUFFERSPROC,      glDeleteFramebuffers);
#undef LOAD
	return true;
}

/* Route standard GL names through the loaded pointers. */
#define glActiveTexture            s_glActiveTexture
#define glCreateShader             s_glCreateShader
#define glShaderSource             s_glShaderSource
#define glCompileShader            s_glCompileShader
#define glGetShaderiv              s_glGetShaderiv
#define glGetShaderInfoLog         s_glGetShaderInfoLog
#define glCreateProgram            s_glCreateProgram
#define glAttachShader             s_glAttachShader
#define glLinkProgram              s_glLinkProgram
#define glGetProgramiv             s_glGetProgramiv
#define glGetProgramInfoLog        s_glGetProgramInfoLog
#define glDeleteShader             s_glDeleteShader
#define glDeleteProgram            s_glDeleteProgram
#define glUseProgram               s_glUseProgram
#define glGetUniformLocation       s_glGetUniformLocation
#define glUniform1i                s_glUniform1i
#define glUniform2f                s_glUniform2f
#define glGetAttribLocation        s_glGetAttribLocation
#define glVertexAttribPointer      s_glVertexAttribPointer
#define glEnableVertexAttribArray  s_glEnableVertexAttribArray
#define glGenVertexArrays          s_glGenVertexArrays
#define glBindVertexArray          s_glBindVertexArray
#define glDeleteVertexArrays       s_glDeleteVertexArrays
#define glGenBuffers               s_glGenBuffers
#define glBindBuffer               s_glBindBuffer
#define glBufferData               s_glBufferData
#define glDeleteBuffers            s_glDeleteBuffers
#define glGenFramebuffers          s_glGenFramebuffers
#define glBindFramebuffer          s_glBindFramebuffer
#define glFramebufferTexture2D     s_glFramebufferTexture2D
#define glCheckFramebufferStatus   s_glCheckFramebufferStatus
#define glDeleteFramebuffers       s_glDeleteFramebuffers
#endif /* !__APPLE__ */

/* ── GLSL shaders ────────────────────────────────────── */

static const char *kVertSrc = R"GLSL(
#version 150
in  vec2 aPos;
in  vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char *kFragSrc = R"GLSL(
#version 150
in  vec2      vUV;
out vec4      fragColor;
uniform sampler2D uTex;
uniform vec2      uTexSize;   /* emulator texture pixel dimensions */

/*
    Barrel distortion — simulate convex CRT curvature by pushing UVs
    outward from the centre using quadratic distance scaling.
*/
vec2 barrel(vec2 uv) {
    vec2  cc   = uv - 0.5;
    float dist = dot(cc, cc);
    return uv + cc * (dist * 0.10);
}

void main() {
    vec2 uv = barrel(vUV);

    /* Out-of-bounds → black border */
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 col = texture(uTex, uv);

    /*
        Scanlines: modulate brightness per row using sine to create
        dark gaps between rows (py≈0,1) and bright centres (py≈0.5).
        Squaring the sine sharpens the falloff for a crisp gap effect.
    */
    float py       = fract(uv.y * uTexSize.y);
    float scanline = sin(py * 3.14159265);
    scanline       = scanline * scanline;
    col.rgb       *= mix(0.50, 1.0, scanline);

    /*
        Vignette: darken corners using a quartic falloff (uv * (1-uv))
        raised to 0.35 for subtle edge darkening that mimics CRT
        phosphor fall-off at the screen periphery.
    */
    vec2  vig      = uv * (1.0 - uv);
    float vignette = clamp(pow(vig.x * vig.y * 18.0, 0.35), 0.0, 1.0);
    col.rgb *= vignette;

    fragColor = col;
}
)GLSL";

/* ── Helpers ─────────────────────────────────────────── */

/*
    Compile a GLSL shader of the given type from the source string.
    Returns 0 and logs errors if compilation fails.
*/
static GLuint compileShader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		fprintf(stderr, "CRT shader compile error: %s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

/* ── CRTEffect ───────────────────────────────────────── */

void CRTEffect::init()
{
#ifndef __APPLE__
	if (!loadGLFunctions()) return;
#endif

	GLuint vert = compileShader(GL_VERTEX_SHADER, kVertSrc);
	if (!vert) return;
	GLuint frag = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
	if (!frag) { glDeleteShader(vert); return; }

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	glDeleteShader(vert);
	glDeleteShader(frag);

	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[512];
		glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
		fprintf(stderr, "CRT shader link error: %s\n", log);
		glDeleteProgram(prog);
		return;
	}

	/* Fullscreen quad: two triangles covering NDC [-1,1]×[-1,1]. */
	static const float kQuad[] = {
		/* x      y     u     v  */
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,
	};

	GLuint vao = 0, vbo = 0;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);

	GLint aPos = glGetAttribLocation(prog, "aPos");
	GLint aUV  = glGetAttribLocation(prog, "aUV");
	if (aPos >= 0)
	{
		glEnableVertexAttribArray((GLuint)aPos);
		glVertexAttribPointer((GLuint)aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	}
	if (aUV >= 0)
	{
		glEnableVertexAttribArray((GLuint)aUV);
		glVertexAttribPointer((GLuint)aUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
		                      (void *)(2 * sizeof(float)));
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	program_  = prog;
	vao_      = vao;
	vbo_      = vbo;
	uTex_     = glGetUniformLocation(prog, "uTex");
	uTexSize_ = glGetUniformLocation(prog, "uTexSize");
}

void CRTEffect::shutdown()
{
	if (fboId_)    { glDeleteFramebuffers(1, &fboId_);  fboId_ = 0; }
	if (fboTexId_) { glDeleteTextures(1, &fboTexId_);   fboTexId_ = 0; }
	if (vbo_)      { glDeleteBuffers(1, &vbo_);         vbo_ = 0; }
	if (vao_)      { glDeleteVertexArrays(1, &vao_);    vao_ = 0; }
	if (program_)  { glDeleteProgram(program_);         program_ = 0; }
	fboW_ = fboH_ = 0;
}

/*
    Create or resize the off-screen FBO and texture used for CRT post-processing.
    Recycles existing resources if dimensions match; otherwise destroys and recreates.
*/
void CRTEffect::ensureFBO(int w, int h)
{
	if (fboW_ == w && fboH_ == h) return;

	if (fboTexId_) glDeleteTextures(1, &fboTexId_);
	if (fboId_)    glDeleteFramebuffers(1, &fboId_);

	glGenTextures(1, &fboTexId_);
	glBindTexture(GL_TEXTURE_2D, fboTexId_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glGenFramebuffers(1, &fboId_);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexId_, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "CRT FBO incomplete (%d×%d)\n", w, h);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	fboW_ = w;
	fboH_ = h;
}

GLuint CRTEffect::process(GLuint srcTex, int srcW, int srcH, int dstW, int dstH)
{
	if (!program_) return srcTex;
	ensureFBO(dstW, dstH);

	/* Save GL state we touch: FBO, viewport, program, texture units, VAO. */
	GLint prevFBO       = 0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
	GLint prevVP[4]     = {}; glGetIntegerv(GL_VIEWPORT, prevVP);
	GLint prevProg      = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
	GLint prevActiveTex = 0; glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTex);
	GLint prevTex0      = 0;
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
	GLint prevVAO       = 0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

	/* Render srcTex through CRT shader into the FBO. */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId_);
	glViewport(0, 0, dstW, dstH);

	glUseProgram(program_);
	glUniform1i(uTex_, 0);
	glUniform2f(uTexSize_, (float)srcW, (float)srcH);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcTex);

	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray((GLuint)prevVAO);

	/* Restore GL state. */
	glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex0);
	glActiveTexture((GLenum)prevActiveTex);
	glUseProgram((GLuint)prevProg);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevFBO);
	glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);

	return fboTexId_;
}

#ifndef __APPLE__
#undef glActiveTexture
#undef glCreateShader
#undef glShaderSource
#undef glCompileShader
#undef glGetShaderiv
#undef glGetShaderInfoLog
#undef glCreateProgram
#undef glAttachShader
#undef glLinkProgram
#undef glGetProgramiv
#undef glGetProgramInfoLog
#undef glDeleteShader
#undef glDeleteProgram
#undef glUseProgram
#undef glGetUniformLocation
#undef glUniform1i
#undef glUniform2f
#undef glGetAttribLocation
#undef glVertexAttribPointer
#undef glEnableVertexAttribArray
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#undef glGenBuffers
#undef glBindBuffer
#undef glBufferData
#undef glDeleteBuffers
#undef glGenFramebuffers
#undef glBindFramebuffer
#undef glFramebufferTexture2D
#undef glCheckFramebufferStatus
#undef glDeleteFramebuffers
#endif /* !__APPLE__ */
