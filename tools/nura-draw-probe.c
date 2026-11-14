/* Draw probe: ES3 context, real shaders, VBO and a fullscreen triangle, then a
 * readback. KWin's GL path does this; plain glClear worked.
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
		"void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
	static const char *FS =
		"#version 300 es\n"
		"precision mediump float;\n"
		"out vec4 c;\n"
		"void main() { c = vec4(0.0, 0.0, 1.0, 1.0); }\n";
	const EGLint cfg_attrs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8, EGL_NONE
	};
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	static const float verts[6] = { -1, -1, 3, -1, -1, 3 };
	get_pd_fn gpd;
	EGLDisplay dpy;
	EGLSurface surf;
	EGLContext ctx;
	EGLConfig cfg;
	EGLint n = 0, maj = 0, min = 0;
	unsigned int vs, fs, prog, vbo, tex, fbo;
	int ok = 0, i;
	char log[256] = {0};
	unsigned char px[4] = {0};
	unsigned int err;

#define S(t, v, name) t v = (t)eglGetProcAddress(name)
	S(glStr_fn, glStr, "glGetString");
	S(glCreateShader_fn, glCreateShader, "glCreateShader");
	S(glShaderSource_fn, glShaderSource, "glShaderSource");
	S(glCompileShader_fn, glCompileShader, "glCompileShader");
	S(glGetShaderiv_fn, glGetShaderiv, "glGetShaderiv");
	S(glGetShaderInfoLog_fn, glGetShaderInfoLog, "glGetShaderInfoLog");
	S(glCreateProgram_fn, glCreateProgram, "glCreateProgram");
	S(glAttachShader_fn, glAttachShader, "glAttachShader");
	S(glLinkProgram_fn, glLinkProgram, "glLinkProgram");
	S(glGetProgramiv_fn, glGetProgramiv, "glGetProgramiv");
	S(glUseProgram_fn, glUseProgram, "glUseProgram");
	S(glGenBuffers_fn, glGenBuffers, "glGenBuffers");
	S(glBindBuffer_fn, glBindBuffer, "glBindBuffer");
	S(glBufferData_fn, glBufferData, "glBufferData");
	S(glVertexAttribPointer_fn, glVertexAttribPointer, "glVertexAttribPointer");
	S(glEnableVertexAttribArray_fn, glEnableVertexAttribArray, "glEnableVertexAttribArray");
	S(glGenFramebuffers_fn, glGenFramebuffers, "glGenFramebuffers");
	S(glBindFramebuffer_fn, glBindFramebuffer, "glBindFramebuffer");
	S(glFramebufferTexture2D_fn, glFramebufferTexture2D, "glFramebufferTexture2D");
	S(glCheckFramebufferStatus_fn, glCheckFramebufferStatus, "glCheckFramebufferStatus");
	S(glGenTextures_fn, glGenTextures, "glGenTextures");
	S(glBindTexture_fn, glBindTexture, "glBindTexture");
	S(glTexImage2D_fn, glTexImage2D, "glTexImage2D");
	S(glViewport_fn, glViewport, "glViewport");
	S(glClearColor_fn, glClearColor, "glClearColor");
	S(glClear_fn, glClear, "glClear");
	S(glDrawArrays_fn, glDrawArrays, "glDrawArrays");
	S(glReadPixels_fn, glReadPixels, "glReadPixels");
	S(glGetError_fn, glGetError, "glGetError");
	S(glFinish_fn, glFinish, "glFinish");

	if (!glStr || !glCreateShader || !glDrawArrays) {
		printf("faltan entry points\n");
		return 1;
	}

	gpd = (get_pd_fn)eglGetProcAddress("eglGetPlatformDisplayEXT");
	dpy = gpd ? gpd(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL) : eglGetDisplay(NULL);
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &maj, &min)) {
		printf("eglInitialize fallo\n");
		return 1;
	}
	if (!eglBindAPI(EGL_OPENGL_ES_API) ||
	    !eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &n) || n != 1) {
		printf("sin config\n");
		return 1;
	}
	surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(dpy, surf, surf, ctx)) {
		printf("contexto fallo: 0x%x\n", eglGetError());
		return 1;
	}
	printf("GL_RENDERER=%s\nGL_VERSION=%s\n",
	       (const char *)glStr(GL_RENDERER), (const char *)glStr(GL_VERSION));

	vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &VS, NULL);
	glCompileShader(vs);
	glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(vs, sizeof(log), NULL, log);
		printf("VS fallo: %s\n", log);
		return 1;
	}
	fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &FS, NULL);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(fs, sizeof(log), NULL, log);
		printf("FS fallo: %s\n", log);
		return 1;
	}
	prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		printf("link fallo\n");
		return 1;
	}
	glUseProgram(prog);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, NULL);
	glEnableVertexAttribArray(0);

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, NULL);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, tex, 0);
	printf("fbo=0x%x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
	glViewport(0, 0, 64, 64);
	printf("shaders y vbo listos\n");

	for (i = 0; i < 5; i++) {
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glFinish();
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
		glFinish();
		if (px[2] == 255)
			break;
	}
	err = glGetError();
	printf("pixel=(%u,%u,%u,%u) gl_error=0x%x intentos=%d\n",
	       px[0], px[1], px[2], px[3], err, i + 1);
	printf("result_draw: %s\n",
	       (err == GL_NO_ERROR && px[2] == 255 && px[0] == 0) ? "OK" : "FALLO");
	return 0;
}
