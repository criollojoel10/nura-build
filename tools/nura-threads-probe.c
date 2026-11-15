/* Multithread probe: two threads, one EGL context each, drawing in parallel.
 * KWin issues GL from several threads; the single-threaded probes all pass and
 * "AS_ACTIVE bit stuck" points at concurrent address-space use.
 */
#include <pthread.h>
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

typedef void (*glClearColor_fn)(float, float, float, float);
typedef void (*glClear_fn)(unsigned int);
typedef void (*glReadPixels_fn)(int, int, int, int, unsigned int, unsigned int, void *);
typedef unsigned int (*glGetError_fn)(void);
typedef void (*glFinish_fn)(void);

static EGLDisplay dpy;
static EGLConfig cfg;

struct worker {
	EGLSurface surf;
	EGLContext ctx;
	float r, g, b;
	int ok;
	int iterations;
};

static void *run(void *arg)
{
	struct worker *w = arg;
	glClearColor_fn cc;
	glClear_fn cl;
	glReadPixels_fn rp;
	glGetError_fn ge;
	glFinish_fn fi;
	int i;

	if (!eglMakeCurrent(dpy, w->surf, w->surf, w->ctx)) {
		printf("makeCurrent fallo en hilo: 0x%x\n", eglGetError());
		w->ok = 0;
		return NULL;
	}
	cc = (glClearColor_fn)eglGetProcAddress("glClearColor");
	cl = (glClear_fn)eglGetProcAddress("glClear");
	rp = (glReadPixels_fn)eglGetProcAddress("glReadPixels");
	ge = (glGetError_fn)eglGetProcAddress("glGetError");
	fi = (glFinish_fn)eglGetProcAddress("glFinish");

	w->ok = 1;
	for (i = 0; i < w->iterations; i++) {
		unsigned char px[4] = {0};
		unsigned int err;

		cc(w->r, w->g, w->b, 1.0f);
		cl(GL_COLOR_BUFFER_BIT);
		fi();
		rp(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
		fi();
		err = ge();
		if (err != GL_NO_ERROR || px[3] != 255) {
			printf("hilo (%.0f,%.0f,%.0f) iter %d: pixel=(%u,%u,%u,%u) err=0x%x\n",
			       w->r, w->g, w->b, i, px[0], px[1], px[2], px[3], err);
			w->ok = 0;
			break;
		}
	}
	return NULL;
}

int main(void)
{
	const EGLint cfg_attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLDisplay (*gpd)(EGLenum, void *, const EGLint *);
	EGLint n = 0, maj = 0, min = 0;
	struct worker w[2];
	pthread_t t[2];
	int i;

	setvbuf(stdout, NULL, _IONBF, 0);

	gpd = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
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

	for (i = 0; i < 2; i++) {
		w[i].surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
		w[i].ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
		w[i].iterations = 50;
		w[i].ok = 0;
		if (w[i].surf == EGL_NO_SURFACE || w[i].ctx == EGL_NO_CONTEXT) {
			printf("contexto %d fallo\n", i);
			return 1;
		}
	}
	w[0].r = 1; w[0].g = 0; w[0].b = 0;
	w[1].r = 0; w[1].g = 0; w[1].b = 1;

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	pthread_create(&t[0], NULL, run, &w[0]);
	pthread_create(&t[1], NULL, run, &w[1]);
	pthread_join(t[0], NULL);
	pthread_join(t[1], NULL);

	printf("hilo0=%s hilo1=%s\n", w[0].ok ? "OK" : "FALLO", w[1].ok ? "OK" : "FALLO");
	printf("result_threads: %s\n", (w[0].ok && w[1].ok) ? "OK" : "FALLO");
	return (w[0].ok && w[1].ok) ? 0 : 1;
}
