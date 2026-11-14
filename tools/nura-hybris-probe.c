/* Off-screen libhybris EGL/GLES2 smoke test; run only with kbase and a
 * separately configured Android userspace. A renderer string alone is not a
 * rendering test. No DRM or display state is modified. */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>

static int check_pixel(const char *name, GLfloat r, GLfloat b)
{
	GLubyte pixel[4] = {0};
	GLenum error;

	glClearColor(r, 0.0f, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	error = glGetError();
	printf("%s=(%u,%u,%u,%u) gl_error=0x%x\n", name,
	       pixel[0], pixel[1], pixel[2], pixel[3], error);
	return error == GL_NO_ERROR && pixel[0] == (r ? 255 : 0) &&
	       pixel[1] == 0 && pixel[2] == (b ? 255 : 0) && pixel[3] == 255;
}

int main(void)
{
	const EGLint config_attrs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_NONE
	};
	const EGLint surface_attrs[] = { EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE };
	const EGLint context_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLConfig config;
	EGLSurface surface = EGL_NO_SURFACE;
	EGLContext context = EGL_NO_CONTEXT;
	EGLint count, major, minor;
	const GLubyte *renderer;
	int red, blue, result = 1;

	/* Unbuffered: if a driver aborts the process we still see how far it got. */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) {
		fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError());
		return 1;
	}
	if (!eglBindAPI(EGL_OPENGL_ES_API) ||
	    !eglChooseConfig(display, config_attrs, &config, 1, &count) || count != 1)
		goto fail;
	surface = eglCreatePbufferSurface(display, config, surface_attrs);
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);
	if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(display, surface, surface, context))
		goto fail;
	renderer = glGetString(GL_RENDERER);
	printf("EGL=%d.%d GL_RENDERER=%s\n", major, minor,
	       renderer ? (const char *)renderer : "(null)");
	red = check_pixel("rojo", 1.0f, 0.0f);
	blue = check_pixel("azul", 0.0f, 1.0f);
	result = !(renderer && red && blue) ||
		(renderer && (strstr((const char *)renderer, "llvmpipe") ||
			      strstr((const char *)renderer, "softpipe")));
	printf("result_render: %s\n", result ? "FALLO" : "OK");
	goto cleanup;

fail:
	fprintf(stderr, "EGL setup failed: 0x%x\n", eglGetError());
cleanup:
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (context != EGL_NO_CONTEXT)
		eglDestroyContext(display, context);
	if (surface != EGL_NO_SURFACE)
		eglDestroySurface(display, surface);
	eglTerminate(display);
	return result;
}
