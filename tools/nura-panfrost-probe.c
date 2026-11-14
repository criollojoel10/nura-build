/* Panfrost render test: EGL on the GPU render node (not the display card).
 *
 * Opens /dev/dri/renderD128 through GBM, creates an EGL pbuffer context,
 * clears to red and to blue and reads the pixels back.  A renderer string
 * alone proves nothing; the readback does.
 *
 * Exit 0 only when both colors come back correct and glGetError is clean.
 */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int check_pixel(const char *name, GLfloat r, GLfloat b)
{
	GLubyte px[4] = {0};
	GLenum err;

	glClearColor(r, 0.0f, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
	err = glGetError();
	printf("%s=(%u,%u,%u,%u) gl_error=0x%x\n", name,
	       px[0], px[1], px[2], px[3], err);
	return err == GL_NO_ERROR &&
	       px[0] == (r ? 255 : 0) && px[1] == 0 &&
	       px[2] == (b ? 255 : 0) && px[3] == 255;
}

int main(void)
{
	const char *node = "/dev/dri/renderD128";
	const EGLint cfg_attrs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8, EGL_NONE
	};
	const EGLint surf_attrs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display;
	EGLDisplay dpy = EGL_NO_DISPLAY;
	EGLSurface surf = EGL_NO_SURFACE;
	EGLContext ctx = EGL_NO_CONTEXT;
	EGLConfig cfg;
	EGLint n = 0, major = 0, minor = 0;
	struct gbm_device *gbm;
	const GLubyte *renderer, *version;
	int fd, red, blue, result = 1;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	fd = open(node, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror(node);
		return 1;
	}
	gbm = gbm_create_device(fd);
	if (!gbm) {
		fprintf(stderr, "gbm_create_device failed\n");
		return 1;
	}
	get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
		eglGetProcAddress("eglGetPlatformDisplayEXT");
	if (!get_platform_display) {
		fprintf(stderr, "no eglGetPlatformDisplayEXT\n");
		return 1;
	}
	dpy = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm, NULL);
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
		fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError());
		return 1;
	}
	printf("EGL %d.%d on %s\n", major, minor, node);
	printf("EGL_VENDOR=%s\n",
	       eglQueryString(dpy, EGL_VENDOR) ?: "(null)");
	if (!eglBindAPI(EGL_OPENGL_ES_API) ||
	    !eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &n) || n != 1) {
		fprintf(stderr, "no EGL config: 0x%x\n", eglGetError());
		goto out;
	}
	surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs);
	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(dpy, surf, surf, ctx)) {
		fprintf(stderr, "surface/context failed: 0x%x\n", eglGetError());
		goto out;
	}
	renderer = glGetString(GL_RENDERER);
	version = glGetString(GL_VERSION);
	printf("GL_RENDERER=%s\nGL_VERSION=%s\n",
	       renderer ? (const char *)renderer : "(null)",
	       version ? (const char *)version : "(null)");
	red = check_pixel("rojo", 1.0f, 0.0f);
	blue = check_pixel("azul", 0.0f, 1.0f);
	result = !(renderer && red && blue &&
		   !strstr((const char *)renderer, "llvmpipe") &&
		   !strstr((const char *)renderer, "softpipe"));
	printf("result_render: %s\n", result ? "FALLO" : "OK");

out:
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (ctx != EGL_NO_CONTEXT)
		eglDestroyContext(dpy, ctx);
	if (surf != EGL_NO_SURFACE)
		eglDestroySurface(dpy, surf);
	eglTerminate(dpy);
	gbm_device_destroy(gbm);
	close(fd);
	return result;
}
