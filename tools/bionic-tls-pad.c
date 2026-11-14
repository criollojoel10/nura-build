/* Reserve the bionic TLS slots in a glibc process that loads Android libraries.
 *
 * Android code reads and writes fixed offsets from the thread pointer
 * (tpidr_el0): TLS_SLOT_OPENGL (+24), TLS_SLOT_OPENGL_API (+32),
 * TLS_SLOT_STACK_GUARD (+40) ... up to +72.  On glibc aarch64 the first static
 * TLS block starts right after the 16-byte TCB, so those writes land inside
 * real thread-local storage; the usual victim is libc's current-locale
 * pointer, and the next sscanf()/strtod() crashes.
 *
 * LD_PRELOAD this object: preloaded objects get their static TLS right after
 * the executable's, so this padding covers tp+16 onwards when the executable
 * has no TLS of its own and absorbs those writes.
 *
 * Taken from the Moto Pad 2026 port (tools/src/bionic-tls-pad.c), used there to
 * run the Mali blob under glibc.
 */
__attribute__((tls_model("initial-exec"), used))
__thread char nura_bionic_tls_slots[256];
