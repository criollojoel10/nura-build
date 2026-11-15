/* Texture/sampler probe: upload a texture, sample it in the fragment shader and
 * read back. KWin samples textures for wallpapers and effects; the other
 * probes only used clear/draw without samplers.
 */
#include <stdio.h>

typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;

#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_NONE 0x3038
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TRIANGLES 0x0004
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_RGBA8 0x8058
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_NO_ERROR 0
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803

extern EGLDisplay eglGetDisplay(void *);
extern EGLBoolean eglInitialize(EGLDisplay, EGLint *, EGLint *);
extern EGLBoolean eglBindAPI(EGLenum);
extern EGLBoolean eglChooseConfig(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
extern EGLSurface eglCreatePbufferSurface(EGLDisplay, EGLConfig, const EGLint *);
extern EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
extern EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
extern EGLint eglGetError(void);
extern void *eglGetProcAddress(const char *);

typedef EGLDisplay (*get_pd_fn)(EGLenum, void *, const EGLint *);
typedef const unsigned char *(*glStr_fn)(unsigned int);
typedef unsigned int (*glCreateShader_fn)(unsigned int);
typedef void (*glShaderSource_fn)(unsigned int, int, const char *const *, const int *);
typedef void (*glCompileShader_fn)(unsigned int);
typedef void (*glGetShaderiv_fn)(unsigned int, unsigned int, int *);
typedef void (*glGetShaderInfoLog_fn)(unsigned int, int, int *, char *);
typedef unsigned int (*glCreateProgram_fn)(void);
typedef void (*glAttachShader_fn)(unsigned int, unsigned int);
typedef void (*glLinkProgram_fn)(unsigned int);
typedef void (*glGetProgramiv_fn)(unsigned int, unsigned int, int *);
typedef void (*glUseProgram_fn)(unsigned int);
typedef int (*glGetUniformLocation_fn)(unsigned int, const char *);
typedef void (*glUniform1i_fn)(int, int);
typedef void (*glGenBuffers_fn)(int, unsigned int *);
typedef void (*glBindBuffer_fn)(unsigned int, unsigned int);
typedef void (*glBufferData_fn)(unsigned int, long, const void *, unsigned int);
typedef void (*glVertexAttribPointer_fn)(unsigned int, int, unsigned int, unsigned char, int, const void *);
typedef void (*glEnableVertexAttribArray_fn)(unsigned int);
typedef void (*glGenFramebuffers_fn)(int, unsigned int *);
typedef void (*glBindFramebuffer_fn)(unsigned int, unsigned int);
typedef void (*glFramebufferTexture2D_fn)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (*glCheckFramebufferStatus_fn)(unsigned int);
typedef void (*glGenTextures_fn)(int, unsigned int *);
typedef void (*glBindTexture_fn)(unsigned int, unsigned int);
typedef void (*glTexImage2D_fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
typedef void (*glTexParameteri_fn)(unsigned int, unsigned int, int);
typedef void (*glActiveTexture_fn)(unsigned int);
typedef void (*glViewport_fn)(int, int, int, int);
typedef void (*glClearColor_fn)(float, float, float, float);
typedef void (*glClear_fn)(unsigned int);
typedef void (*glDrawArrays_fn)(unsigned int, int, int);
typedef void (*glReadPixels_fn)(int, int, int, int, unsigned int, unsigned int, void *);
typedef unsigned int (*glGetError_fn)(void);
typedef void (*glFinish_fn)(void);

int main(void)
{
	static const char *VS =
		"#version 300 es\n"
		"layout(location = 0) in vec2 p;\n"
		"out vec2 uv;\n"
		"void main() { uv = p * 0.5 + 0.5; gl_Position = vec4(p, 0.0, 1.0); }\n";
	static const char *FS =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec2 uv;\n"
		"uniform sampler2D tex;\n"
		"out vec4 c;\n"
		"void main() { c = texture(tex, uv); }\n";
	const EGLint cfg_attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	static const float verts[6] = { -1, -1, 3, -1, -1, 3 };
	static const unsigned char texels[16] = {
		255, 0, 0, 255, 255, 0, 0, 255,
		255, 0, 0, 255, 255, 0, 0, 255 };
	get_pd_fn gpd;
	EGLDisplay dpy;
	EGLSurface surf;
	EGLContext ctx;
	EGLConfig cfg;
	EGLint n = 0, maj = 0, min = 0;
	unsigned int vs, fs, prog, vbo, tex, fbo;
	int ok = 0, loc, i;
	char log[256] = {0};
	unsigned char px[4] = {0};
	unsigned int err;

#define F(t, v, name) t v = (t)eglGetProcAddress(name)
	F(glStr_fn, pStr, "glGetString");
	F(glCreateShader_fn, pCreateShader, "glCreateShader");
	F(glShaderSource_fn, pShaderSource, "glShaderSource");
	F(glCompileShader_fn, pCompileShader, "glCompileShader");
	F(glGetShaderiv_fn, pGetShaderiv, "glGetShaderiv");
	F(glGetShaderInfoLog_fn, pGetShaderInfoLog, "glGetShaderInfoLog");
	F(glCreateProgram_fn, pCreateProgram, "glCreateProgram");
	F(glAttachShader_fn, pAttachShader, "glAttachShader");
	F(glLinkProgram_fn, pLinkProgram, "glLinkProgram");
	F(glGetProgramiv_fn, pGetProgramiv, "glGetProgramiv");
	F(glUseProgram_fn, pUseProgram, "glUseProgram");
	F(glGetUniformLocation_fn, pGetUniformLocation, "glGetUniformLocation");
	F(glUniform1i_fn, pUniform1i, "glUniform1i");
	F(glGenBuffers_fn, pGenBuffers, "glGenBuffers");
	F(glBindBuffer_fn, pBindBuffer, "glBindBuffer");
	F(glBufferData_fn, pBufferData, "glBufferData");
	F(glVertexAttribPointer_fn, pVertexAttribPointer, "glVertexAttribPointer");
	F(glEnableVertexAttribArray_fn, pEnableVertexAttribArray, "glEnableVertexAttribArray");
	F(glGenFramebuffers_fn, pGenFramebuffers, "glGenFramebuffers");
	F(glBindFramebuffer_fn, pBindFramebuffer, "glBindFramebuffer");
	F(glFramebufferTexture2D_fn, pFramebufferTexture2D, "glFramebufferTexture2D");
	F(glCheckFramebufferStatus_fn, pCheckFramebufferStatus, "glCheckFramebufferStatus");
	F(glGenTextures_fn, pGenTextures, "glGenTextures");
	F(glBindTexture_fn, pBindTexture, "glBindTexture");
	F(glTexImage2D_fn, pTexImage2D, "glTexImage2D");
	F(glTexParameteri_fn, pTexParameteri, "glTexParameteri");
	F(glActiveTexture_fn, pActiveTexture, "glActiveTexture");
	F(glViewport_fn, pViewport, "glViewport");
	F(glClearColor_fn, pClearColor, "glClearColor");
	F(glClear_fn, pClear, "glClear");
	F(glDrawArrays_fn, pDrawArrays, "glDrawArrays");
	F(glReadPixels_fn, pReadPixels, "glReadPixels");
	F(glGetError_fn, pGetError, "glGetError");
	F(glFinish_fn, pFinish, "glFinish");

	gpd = (get_pd_fn)eglGetProcAddress("eglGetPlatformDisplayEXT");
	dpy = gpd ? gpd(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL) : eglGetDisplay(NULL);
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &maj, &min)) {
		printf("eglInitialize fallo\n"); return 1;
	}
	if (!eglBindAPI(EGL_OPENGL_ES_API) ||
	    !eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &n) || n != 1) {
		printf("sin config\n"); return 1;
	}
	surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(dpy, surf, surf, ctx)) {
		printf("contexto fallo: 0x%x\n", eglGetError()); return 1;
	}
	printf("GL_RENDERER=%s\nGL_VERSION=%s\n",
	       (const char *)pStr(GL_RENDERER), (const char *)pStr(GL_VERSION));

	vs = pCreateShader(GL_VERTEX_SHADER);
	pShaderSource(vs, 1, &VS, NULL);
	pCompileShader(vs);
	pGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
	if (!ok) { pGetShaderInfoLog(vs, sizeof(log), NULL, log); printf("VS: %s\n", log); return 1; }
	fs = pCreateShader(GL_FRAGMENT_SHADER);
	pShaderSource(fs, 1, &FS, NULL);
	pCompileShader(fs);
	pGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
	if (!ok) { pGetShaderInfoLog(fs, sizeof(log), NULL, log); printf("FS: %s\n", log); return 1; }
	prog = pCreateProgram();
	pAttachShader(prog, vs);
	pAttachShader(prog, fs);
	pLinkProgram(prog);
	pGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) { printf("link fallo\n"); return 1; }
	pUseProgram(prog);

	pGenBuffers(1, &vbo);
	pBindBuffer(GL_ARRAY_BUFFER, vbo);
	pBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	pVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, NULL);
	pEnableVertexAttribArray(0);

	pGenTextures(1, &tex);
	pActiveTexture(GL_TEXTURE0);
	pBindTexture(GL_TEXTURE_2D, tex);
	pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA,
		    GL_UNSIGNED_BYTE, texels);
	pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	loc = pGetUniformLocation(prog, "tex");
	pUniform1i(loc, 0);
	printf("textura subida (gl=0x%x)\n", pGetError());

	{
		unsigned int t2;
		pGenTextures(1, &t2);
		pBindTexture(GL_TEXTURE_2D, t2);
		pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA,
			    GL_UNSIGNED_BYTE, NULL);
		pGenFramebuffers(1, &fbo);
		pBindFramebuffer(GL_FRAMEBUFFER, fbo);
		pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D, t2, 0);
	}
	printf("fbo=0x%x\n", pCheckFramebufferStatus(GL_FRAMEBUFFER));
	/* La textura destino se enlazo en la unidad 0: devolver el sampler. */
	pActiveTexture(GL_TEXTURE0);
	pBindTexture(GL_TEXTURE_2D, tex);
	pUniform1i(loc, 0);
	pViewport(0, 0, 64, 64);

	for (i = 0; i < 5; i++) {
		pClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		pClear(GL_COLOR_BUFFER_BIT);
		pDrawArrays(GL_TRIANGLES, 0, 3);
		pFinish();
		pReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
		pFinish();
		if (px[0] == 255)
			break;
	}
	err = pGetError();
	printf("pixel=(%u,%u,%u,%u) gl_error=0x%x intentos=%d\n",
	       px[0], px[1], px[2], px[3], err, i + 1);
	printf("result_tex: %s\n",
	       (err == GL_NO_ERROR && px[0] == 255 && px[2] == 0) ? "OK" : "FALLO");
	return 0;
}
