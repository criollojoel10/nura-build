/* ES3 probe: identical to nura-egl-probe (surfaceless + pbuffer + FBO) but
 * requests an OpenGL ES 3 context, which is what KWin uses.
 */
#include <stdio.h>
#include <string.h>

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

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_NO_ERROR 0
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA8 0x8058
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

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
typedef void (*glClearColor_fn)(float, float, float, float);
typedef void (*glClear_fn)(unsigned int);
typedef void (*glReadPixels_fn)(int, int, int, int, unsigned int, unsigned int, void *);
typedef unsigned int (*glGetError_fn)(void);
typedef void (*glGenFramebuffers_fn)(int, unsigned int *);
typedef void (*glBindFramebuffer_fn)(unsigned int, unsigned int);
typedef void (*glGenTextures_fn)(int, unsigned int *);
typedef void (*glBindTexture_fn)(unsigned int, unsigned int);
typedef void (*glTexImage2D_fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
typedef void (*glFramebufferTexture2D_fn)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (*glCheckFramebufferStatus_fn)(unsigned int);
typedef void (*glFinish_fn)(void);

#define SYM(t, v, n) t v = (t)eglGetProcAddress(n)

int main(void)
{
	const EGLint cfg_attrs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8, EGL_NONE
	};
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	get_pd_fn gpd;
	EGLDisplay dpy;
	EGLSurface surf;
	EGLContext ctx;
	EGLConfig cfg;
	EGLint n = 0, maj = 0, min = 0;
	unsigned int tex, fbo;
	unsigned char px[4] = {0};
	unsigned int err;
	int i;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	gpd = (get_pd_fn)eglGetProcAddress("eglGetPlatformDisplayEXT");
	dpy = gpd ? gpd(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL)
		  : eglGetDisplay(NULL);
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &maj, &min)) {
		printf("eglInitialize fallo: 0x%x\n", eglGetError());
		return 1;
	}
	printf("EGL %d.%d\n", maj, min);
	if (!eglBindAPI(EGL_OPENGL_ES_API) ||
	    !eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &n) || n != 1) {
		printf("sin config: 0x%x\n", eglGetError());
		return 1;
	}
	surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(dpy, surf, surf, ctx)) {
		printf("contexto ES3 fallo: 0x%x\n", eglGetError());
		return 1;
	}

	SYM(glStr_fn, glStr, "glGetString");
	SYM(glClearColor_fn, glClearColor, "glClearColor");
	SYM(glClear_fn, glClear, "glClear");
	SYM(glReadPixels_fn, glReadPixels, "glReadPixels");
	SYM(glGetError_fn, glGetError, "glGetError");
	SYM(glGenFramebuffers_fn, glGenFramebuffers, "glGenFramebuffers");
	SYM(glBindFramebuffer_fn, glBindFramebuffer, "glBindFramebuffer");
	SYM(glGenTextures_fn, glGenTextures, "glGenTextures");
	SYM(glBindTexture_fn, glBindTexture, "glBindTexture");
	SYM(glTexImage2D_fn, glTexImage2D, "glTexImage2D");
	SYM(glFramebufferTexture2D_fn, glFramebufferTexture2D, "glFramebufferTexture2D");
	SYM(glCheckFramebufferStatus_fn, glCheckFramebufferStatus, "glCheckFramebufferStatus");
	SYM(glFinish_fn, glFinish, "glFinish");

	printf("GL_RENDERER=%s\n", glStr ? (const char *)glStr(GL_RENDERER) : "?");
	printf("GL_VERSION=%s\n", glStr ? (const char *)glStr(GL_VERSION) : "?");

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, NULL);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, tex, 0);
	printf("fbo=0x%x (0x8cd5 esperado)\n",
	       glCheckFramebufferStatus(GL_FRAMEBUFFER));

	for (i = 0; i < 5; i++) {
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glFinish();
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
		glFinish();
		if (px[0] == 255 && px[2] == 0 && px[1] == 0)
			break;
	}
	err = glGetError();
	printf("rojo=(%u,%u,%u,%u) gl_error=0x%x intentos=%d\n",
	       px[0], px[1], px[2], px[3], err, i + 1);
	printf("result_es3: %s\n",
	       (err == GL_NO_ERROR && px[0] == 255 && px[2] == 0) ? "OK" : "FALLO");
	return 0;
}
