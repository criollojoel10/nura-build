/* Load the vendor Mali blob with the NDK libraries already in scope.
 *
 * libhybris' linker aborts when a required symbol is missing.  The Mali blob
 * imports AHardwareBuffer_* / ANativeWindow_* (511 symbols with no definition
 * among its own NEEDED libraries); on Android those come from libandroid.so.
 *
 * This test android_dlopen()s libandroid.so and libnativewindow.so with
 * RTLD_GLOBAL first, then the blob, and reports which step fails.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef void *(*android_dlopen_fn)(const char *name, int flag);

int main(void)
{
	void *hybris, *libandroid, *libnativewindow, *blob;
	android_dlopen_fn android_dlopen;
	const char *paths[] = {
		"/opt/nura-android/system/lib64/libandroid.so",
		"/opt/nura-android/system/lib64/libnativewindow.so",
	};
	const char *blob_path =
		"/opt/nura-android/vendor/lib64/egl/libGLES_mali.so";

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	hybris = dlopen("libhybris-common.so.1", RTLD_NOW);
	if (!hybris) {
		fprintf(stderr, "dlopen libhybris-common: %s\n", dlerror());
		return 1;
	}
	android_dlopen = (android_dlopen_fn)dlsym(hybris, "android_dlopen");
	if (!android_dlopen) {
		fprintf(stderr, "android_dlopen no exportado\n");
		return 1;
	}
	printf("hybris ok\n");

	libandroid = android_dlopen(paths[0], RTLD_NOW | RTLD_GLOBAL);
	printf("libandroid=%p\n", libandroid);
	libnativewindow = android_dlopen(paths[1], RTLD_NOW | RTLD_GLOBAL);
	printf("libnativewindow=%p\n", libnativewindow);

	blob = android_dlopen(blob_path, RTLD_NOW | RTLD_GLOBAL);
	printf("blob=%p\n", blob);
	printf("resultado: %s\n", (blob && libandroid) ? "OK" : "FALLO");
	return (blob && libandroid) ? 0 : 1;
}
