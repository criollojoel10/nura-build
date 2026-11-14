/* Self-contained EGL/GLES2 render probe (no headers required).
 *
 * Used where EGL/GBM headers are unavailable (e.g. the Alpine chroot holding
 * the musl Mesa build).  Opens EGL on the surfaceless platform, which binds to
 * the first DRM render node (the Mali GPU), renders red and blue into a
 * pbuffer and reads the pixels back.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;

#define EGL_DEFAULT_DISPLAY ((void *)0)
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
#define EGL_VENDOR 0x3053
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
#define GL_COLOR_BUFFER_BIT2 0

extern EGLDisplay eglGetDisplay(void *);
extern EGLBoolean eglInitialize(EGLDisplay, EGLint *, EGLint *);
extern const char *eglQueryString(EGLDisplay, EGLint);
extern EGLBoolean eglBindAPI(EGLenum);
extern EGLBoolean eglChooseConfig(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
extern EGLSurface eglCreatePbufferSurface(EGLDisplay, EGLConfig, const EGLint *);
extern EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
extern EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
extern EGLint eglGetError(void);
extern void *eglGetProcAddress(const char *);

typedef EGLDisplay (*get_platform_display_fn)(EGLenum, void *, const EGLint *);
typedef const unsigned char *(*glGetString_fn)(unsigned int);
typedef void (*glClearColor_fn)(float, float, float, float);
typedef void (*glClear_fn)(unsigned int);
typedef void (*glReadPixels_fn)(int, int, int, int, unsigned int, unsigned int, void *);
typedef unsigned int (*glGetError_fn)(void);
typedef void (*glFinish_fn)(void);
typedef void (*glGenFramebuffers_fn)(int, unsigned int *);
typedef void (*glBindFramebuffer_fn)(unsigned int, unsigned int);
typedef void (*glGenTextures_fn)(int, unsigned int *);
typedef void (*glBindTexture_fn)(unsigned int, unsigned int);
typedef void (*glTexImage2D_fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
typedef void (*glFramebufferTexture2D_fn)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (*glCheckFramebufferStatus_fn)(unsigned int);

static glGetString_fn pGetString;
static glClearColor_fn pClearColor;
static glClear_fn pClear;
static glReadPixels_fn pReadPixels;
static glGetError_fn pGetError;
static glFinish_fn pFinish;
static glGenFramebuffers_fn pGenFramebuffers;
static glBindFramebuffer_fn pBindFramebuffer;
static glGenTextures_fn pGenTextures;
static glBindTexture_fn pBindTexture;
static glTexImage2D_fn pTexImage2D;
static glFramebufferTexture2D_fn pFramebufferTexture2D;
static glCheckFramebufferStatus_fn pCheckFramebufferStatus;
static unsigned int fbo, tex;

static int check_pixel_once(const char *name, float r, float b)
{
	unsigned char px[4] = {0};
	unsigned int err;

	pClearColor(r, 0.0f, b, 1.0f);
	pClear(GL_COLOR_BUFFER_BIT);
	pFinish();
	pReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
	pFinish();
	err = pGetError();
	printf("%s=(%u,%u,%u,%u) gl_error=0x%x\n", name,
	       px[0], px[1], px[2], px[3], err);
	return err == GL_NO_ERROR && px[0] == (r ? 255 : 0) && px[1] == 0 &&
	       px[2] == (b ? 255 : 0) && px[3] == 255;
}

static int check_pixel(const char *name, float r, float b)
{
	int i;
	for (i = 0; i < 5; i++) {
		if (check_pixel_once(name, r, b))
			return 1;
	}
	return 0;
}

int main(void)
{
	const EGLint cfg_attrs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8, EGL_NONE
	};
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	get_platform_display_fn getPlatformDisplay;
	EGLDisplay dpy = EGL_NO_DISPLAY;
	EGLSurface surf;
	EGLContext ctx;
	EGLConfig cfg;
	EGLint n = 0, major = 0, minor = 0;
	const unsigned char *renderer, *version;
	int red, blue, result = 1;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	getPlatformDisplay = (get_platform_display_fn)
		eglGetProcAddress("eglGetPlatformDisplayEXT");
	if (getPlatformDisplay) {
		dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
		printf("plataforma surfaceless\n");
	} else {
		dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		printf("plataforma por defecto\n");
	}
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
		fprintf(stderr, "eglInitialize fallo: 0x%x\n", eglGetError());
		return 1;
	}
	printf("EGL %d.%d vendor=%s\n", major, minor,
	       eglQueryString(dpy, EGL_VENDOR) ? eglQueryString(dpy, EGL_VENDOR) : "?");
	if (!eglBindAPI(EGL_OPENGL_ES_API) ||
	    !eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &n) || n != 1) {
		fprintf(stderr, "sin config EGL: 0x%x\n", eglGetError());
		return 1;
	}
	surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(dpy, surf, surf, ctx)) {
		fprintf(stderr, "superficie/contexto fallo: 0x%x\n", eglGetError());
		return 1;
	}
	pGetString = (glGetString_fn)eglGetProcAddress("glGetString");
	pClearColor = (glClearColor_fn)eglGetProcAddress("glClearColor");
	pClear = (glClear_fn)eglGetProcAddress("glClear");
	pReadPixels = (glReadPixels_fn)eglGetProcAddress("glReadPixels");
	pGetError = (glGetError_fn)eglGetProcAddress("glGetError");
	pFinish = (glFinish_fn)eglGetProcAddress("glFinish");
	pGenFramebuffers = (glGenFramebuffers_fn)eglGetProcAddress("glGenFramebuffers");
	pBindFramebuffer = (glBindFramebuffer_fn)eglGetProcAddress("glBindFramebuffer");
	pGenTextures = (glGenTextures_fn)eglGetProcAddress("glGenTextures");
	pBindTexture = (glBindTexture_fn)eglGetProcAddress("glBindTexture");
	pTexImage2D = (glTexImage2D_fn)eglGetProcAddress("glTexImage2D");
	pFramebufferTexture2D = (glFramebufferTexture2D_fn)eglGetProcAddress("glFramebufferTexture2D");
	pCheckFramebufferStatus = (glCheckFramebufferStatus_fn)eglGetProcAddress("glCheckFramebufferStatus");
	if (!pGetString || !pClearColor || !pClear || !pReadPixels || !pGetError || !pFinish) {
		fprintf(stderr, "faltan entry points GL\n");
		return 1;
	}
	renderer = pGetString(GL_RENDERER);
	version = pGetString(GL_VERSION);
	printf("GL_RENDERER=%s\nGL_VERSION=%s\n",
	       renderer ? (const char *)renderer : "(null)",
	       version ? (const char *)version : "(null)");
	pGenTextures(1, &tex);
	pBindTexture(GL_TEXTURE_2D, tex);
	pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	pGenFramebuffers(1, &fbo);
	pBindFramebuffer(GL_FRAMEBUFFER, fbo);
	pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	printf("fbo completo=0x%x (0x8cd5 esperado)\n", pCheckFramebufferStatus(GL_FRAMEBUFFER));
	red = check_pixel("rojo", 1.0f, 0.0f);
	blue = check_pixel("azul", 0.0f, 1.0f);
	result = !(renderer && red && blue &&
		   !strstr((const char *)renderer, "llvmpipe") &&
		   !strstr((const char *)renderer, "softpipe"));
	printf("result_render: %s\n", result ? "FALLO" : "OK");
	return result;
}
