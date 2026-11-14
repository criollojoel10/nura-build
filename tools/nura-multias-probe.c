/* Multi-AS probe: several EGL contexts (one address space each) used in turn.
 * KWin keeps several GL contexts alive; the single-context probes worked, and
 * "AS_ACTIVE bit stuck" points at AS switching.
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

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_NO_ERROR 0

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
typedef void (*glClearColor_fn)(float, float, float, float);
typedef void (*glClear_fn)(unsigned int);
typedef void (*glReadPixels_fn)(int, int, int, int, unsigned int, unsigned int, void *);
typedef unsigned int (*glGetError_fn)(void);
typedef void (*glFinish_fn)(void);

#define N 3

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
	get_pd_fn gpd;
	EGLDisplay dpy;
	EGLConfig cfg;
	EGLSurface surf[N];
	EGLContext ctx[N];
	EGLint n = 0, maj = 0, min = 0;
	glClearColor_fn pClearColor;
	glClear_fn pClear;
	glReadPixels_fn pReadPixels;
	glGetError_fn pGetError;
	glFinish_fn pFinish;
	int i, round, fail = 0;

	setvbuf(stdout, NULL, _IONBF, 0);

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

	/* Crear los N contextos antes de usar ninguno: N address spaces vivos. */
	for (i = 0; i < N; i++) {
		surf[i] = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
		ctx[i] = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
		if (surf[i] == EGL_NO_SURFACE || ctx[i] == EGL_NO_CONTEXT) {
			printf("contexto %d fallo: 0x%x\n", i, eglGetError());
			return 1;
		}
	}
	printf("%d contextos creados\n", N);

	for (round = 0; round < 3; round++) {
		for (i = 0; i < N; i++) {
			unsigned char px[4] = {0};
			unsigned int err;
			float r = (i == 0), b = (i == 1), g = (i == 2);

			if (!eglMakeCurrent(dpy, surf[i], surf[i], ctx[i])) {
				printf("makeCurrent %d fallo: 0x%x\n", i, eglGetError());
				return 1;
			}
			pClearColor = (glClearColor_fn)eglGetProcAddress("glClearColor");
			pClear = (glClear_fn)eglGetProcAddress("glClear");
			pReadPixels = (glReadPixels_fn)eglGetProcAddress("glReadPixels");
			pGetError = (glGetError_fn)eglGetProcAddress("glGetError");
			pFinish = (glFinish_fn)eglGetProcAddress("glFinish");
			pClearColor(r, g, b, 1.0f);
			pClear(GL_COLOR_BUFFER_BIT);
			pFinish();
			pReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
			pFinish();
			err = pGetError();
			printf("ronda %d ctx %d pixel=(%u,%u,%u,%u) err=0x%x\n",
			       round, i, px[0], px[1], px[2], px[3], err);
			if (err != GL_NO_ERROR || px[3] != 255)
				fail = 1;
		}
	}
	printf("result_multias: %s\n", fail ? "FALLO" : "OK");
	return fail;
}
