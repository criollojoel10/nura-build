/* Compatibility symbols for the bionic linker bundled by libhybris.
 *
 * Loading libhybris' modern linker (linker/q.so) aborts with:
 *   q.so: error: symbol lookup error: undefined symbol:
 *         android_dl_unwind_find_exidx (fatal)
 * even though q.so has no such undefined relocation: the bionic linker looks
 * it up at run time and treats a miss as fatal.
 *
 * On aarch64 there is no .ARM.exidx exception table (that is 32-bit ARM only),
 * so returning "no table" is the correct answer.  Same for the shared-globals
 * accessor, which aarch64 never uses.
 */
#include <stddef.h>

void *android_dl_unwind_find_exidx(void *pc, int *pcount)
{
	(void)pc;
	if (pcount)
		*pcount = 0;
	return NULL;
}

void *__loader_dl_unwind_find_exidx(void *pc, int *pcount)
{
	return android_dl_unwind_find_exidx(pc, pcount);
}

void *android_shared_globals(void)
{
	return NULL;
}

void *__loader_shared_globals(void)
{
	return NULL;
}
