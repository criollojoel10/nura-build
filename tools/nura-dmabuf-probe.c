/* dmabuf/EGLImage probe: allocate a gbm bo, export its fd and import it as an
 * EGLImage (the path KWin uses for client buffers), render into it and read.
 */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLImageKHR;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;

#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_IMAGE ((EGLImageKHR)0)
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
#define EGL_PLATFORM_GBM_KHR 0x31D7
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_LINUX_DMA_BUF_EXT 0x3270
#define EGL_LINUX_DRM_FOURCC_EXT 0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT 0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT 0x3274
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_NO_ERROR 0
#define GL_TEXTURE_2D 0x0DE1

extern EGLDisplay eglGetDisplay(void *);
extern EGLBoolean eglInitialize(EGLDisplay, EGLint *, EGLint *);
extern EGLBoolean eglBindAPI(EGLenum);
extern EGLBoolean eglChooseConfig(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
extern EGLSurface eglCreatePbufferSurface(EGLDisplay, EGLConfig, const EGLint *);
extern EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
extern EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
extern EGLint eglGetError(void);
extern void *eglGetProcAddress(const char *);

struct gbm_device;
struct gbm_bo;
struct gbm_device *gbm_create_device(int fd);
struct gbm_bo *gbm_bo_create(struct gbm_device *, unsigned int w, unsigned int h,
			     unsigned int format, unsigned int flags);
int gbm_bo_get_fd(struct gbm_bo *);
unsigned int gbm_bo_get_stride(struct gbm_bo *);
unsigned long long gbm_bo_get_modifier(struct gbm_bo *);
void gbm_bo_destroy(struct gbm_bo *);
void gbm_device_destroy(struct gbm_device *);

#define GBM_FORMAT_XRGB8888 0x34325258u
#define GBM_BO_USE_RENDERING (1u << 2)
#define GBM_BO_USE_LINEAR (1u << 4)

typedef EGLDisplay (*get_pd_fn)(EGLenum, void *, const EGLint *);
typedef EGLImageKHR (*create_image_fn)(EGLDisplay, EGLContext, EGLenum, void *, const EGLint *);
typedef void (*target_tex_fn)(unsigned int, EGLImageKHR);
typedef void (*glGenTextures_fn)(int, unsigned int *);
typedef void (*glBindTexture_fn)(unsigned int, unsigned int);
typedef void (*glGenFramebuffers_fn)(int, unsigned int *);
typedef void (*glBindFramebuffer_fn)(unsigned int, unsigned int);
typedef void (*glFramebufferTexture2D_fn)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (*glCheckFramebufferStatus_fn)(unsigned int);
typedef void (*glClearColor_fn)(float, float, float, float);
typedef void (*glClear_fn)(unsigned int);
typedef void (*glReadPixels_fn)(int, int, int, int, unsigned int, unsigned int, void *);
typedef unsigned int (*glGetError_fn)(void);
typedef void (*glFinish_fn)(void);

int main(void)
{
	const EGLint cfg_attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	get_pd_fn gpd;
	create_image_fn pCreateImage;
	target_tex_fn pTargetTex;
	glGenTextures_fn pGenTextures;
	glBindTexture_fn pBindTexture;
	glGenFramebuffers_fn pGenFramebuffers;
	glBindFramebuffer_fn pBindFramebuffer;
	glFramebufferTexture2D_fn pFramebufferTexture2D;
	glCheckFramebufferStatus_fn pCheckFramebufferStatus;
	glClearColor_fn pClearColor;
	glClear_fn pClear;
	glReadPixels_fn pReadPixels;
	glGetError_fn pGetError;
	glFinish_fn pFinish;
	struct gbm_device *gbm;
	struct gbm_bo *bo;
	EGLDisplay dpy;
	EGLConfig cfg;
	EGLSurface surf;
	EGLContext ctx;
	EGLImageKHR img;
	EGLint n = 0, maj = 0, min = 0;
	EGLint attrs[20];
	unsigned int tex, fbo, err;
	unsigned char px[4] = {0};
	int fd, i;

	setvbuf(stdout, NULL, _IONBF, 0);

	fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
	if (fd < 0) { perror("open"); return 1; }
	gbm = gbm_create_device(fd);
	if (!gbm) { printf("gbm_create_device fallo\n"); return 1; }
	bo = gbm_bo_create(gbm, 64, 64, GBM_FORMAT_XRGB8888,
			   GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
	if (!bo) { printf("gbm_bo_create fallo\n"); return 1; }
	printf("bo fd=%d stride=%u modifier=0x%llx\n", gbm_bo_get_fd(bo),
	       gbm_bo_get_stride(bo), gbm_bo_get_modifier(bo));

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

	pCreateImage = (create_image_fn)eglGetProcAddress("eglCreateImageKHR");
	pTargetTex = (target_tex_fn)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if (!pCreateImage || !pTargetTex) {
		printf("faltan EGLImage/EGLImageTargetTexture\n"); return 1;
	}

	i = 0;
	attrs[i++] = EGL_WIDTH;            attrs[i++] = 64;
	attrs[i++] = EGL_HEIGHT;           attrs[i++] = 64;
	attrs[i++] = EGL_LINUX_DRM_FOURCC_EXT; attrs[i++] = GBM_FORMAT_XRGB8888;
	attrs[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;      attrs[i++] = gbm_bo_get_fd(bo);
	attrs[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;  attrs[i++] = 0;
	attrs[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;   attrs[i++] = gbm_bo_get_stride(bo);
	attrs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
	attrs[i++] = (EGLint)(gbm_bo_get_modifier(bo) & 0xffffffff);
	attrs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
	attrs[i++] = (EGLint)(gbm_bo_get_modifier(bo) >> 32);
	attrs[i++] = EGL_NONE;

	img = pCreateImage(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
	printf("EGLImage=%p\n", (void *)img);
	if (img == EGL_NO_IMAGE) {
		printf("eglCreateImageKHR fallo: 0x%x\n", eglGetError()); return 1;
	}

	pGenTextures = (glGenTextures_fn)eglGetProcAddress("glGenTextures");
	pBindTexture = (glBindTexture_fn)eglGetProcAddress("glBindTexture");
	pGenFramebuffers = (glGenFramebuffers_fn)eglGetProcAddress("glGenFramebuffers");
	pBindFramebuffer = (glBindFramebuffer_fn)eglGetProcAddress("glBindFramebuffer");
	pFramebufferTexture2D = (glFramebufferTexture2D_fn)eglGetProcAddress("glFramebufferTexture2D");
	pCheckFramebufferStatus = (glCheckFramebufferStatus_fn)eglGetProcAddress("glCheckFramebufferStatus");
	pClearColor = (glClearColor_fn)eglGetProcAddress("glClearColor");
	pClear = (glClear_fn)eglGetProcAddress("glClear");
	pReadPixels = (glReadPixels_fn)eglGetProcAddress("glReadPixels");
	pGetError = (glGetError_fn)eglGetProcAddress("glGetError");
	pFinish = (glFinish_fn)eglGetProcAddress("glFinish");

	pGenTextures(1, &tex);
	pBindTexture(GL_TEXTURE_2D, tex);
	pTargetTex(GL_TEXTURE_2D, img);
	printf("textura desde EGLImage: gl=0x%x\n", pGetError());

	pGenFramebuffers(1, &fbo);
	pBindFramebuffer(GL_FRAMEBUFFER, fbo);
	pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	printf("fbo=0x%x\n", pCheckFramebufferStatus(GL_FRAMEBUFFER));

	for (i = 0; i < 5; i++) {
		pClearColor(0.0f, 0.0f, 1.0f, 1.0f);
		pClear(GL_COLOR_BUFFER_BIT);
		pFinish();
		pReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
		pFinish();
		if (px[2] == 255)
			break;
	}
	err = pGetError();
	printf("pixel=(%u,%u,%u,%u) gl_error=0x%x intentos=%d\n",
	       px[0], px[1], px[2], px[3], err, i + 1);
	printf("result_dmabuf: %s\n",
	       (err == GL_NO_ERROR && px[2] == 255) ? "OK" : "FALLO");
	return 0;
}
