#!/usr/bin/env python3
"""Add Mali-G68 to Mesa's panfrost model table, at package-build time.

Why: Kernel reports `panfrost 13000000.mali: mali-g68 id 0x9204` and binds, but
Mesa 26.2.6's table has entries only for (9,0,1) and (9,0,3); the phone's chip
decodes to (9,2,4), so `pan_get_model()` finds nothing and Mesa hangs inside
eglGetPlatformDisplay (measured with nura-gpu-probe: "gbm device: ok" and then
silence). Mesa main (27.0.0-devel) has the row, so this is a backport of one
table entry, not new driver work.

Usage: tools/patch-mesa-g68.py <ruta al APKBUILD de mesa>

Idempotent: exits 0 doing nothing if the row is already there.
"""
import re
import sys
from pathlib import Path

# Copiada de src/panfrost/model/pan_model.c en Mesa main, en UNA linea: el
# macro no necesita el salto, y asi se inserta con awk (portable en busybox) en
# vez de con el "sed /i\\" multilinea, que cambia entre GNU y busybox.
ROW = ('   VALHALL_MODEL(PAN_PROD_ID(9, 2, 4), 0, "G68", "G78", '
       'MODEL_ANISO(ALL), MODEL_TB_SIZES(16384, 8192), '
       'MODEL_RATES_X(2, 4, 8, 32, 32, 8)),')

# Se inserta justo antes de la fila del G610, como en upstream.
ANCHOR = 'VALHALL_MODEL(PAN_PROD_ID(10, 8, 7)'

# Bloque de shell que se inserta en prepare(). Comprueba el *fuente*, no el
# APKBUILD: si Alpine ya subio a una Mesa con G68 (upstream lo tiene desde 27.0),
# no hay que tocar nada.
SED = (
    "if ! grep -q 'PAN_PROD_ID(9, 2, 4)' src/panfrost/model/pan_model.c; then\n"
    "\t\tawk -v row='%s' '{ if (!done && $0 ~ /%s/) { print row; done = 1 } print }' \\\n"
    "\t\t\tsrc/panfrost/model/pan_model.c > .pan_model.new \\\n"
    "\t\t\t&& mv .pan_model.new src/panfrost/model/pan_model.c\n"
    "\tfi"
    % (ROW, ANCHOR.replace('(', r'\(').replace(')', r'\)'))
)

# Mesa 26.x arrastra Rust por dos cosas que aqui no sirven: rusticl (OpenCL) y
# el driver vulkan nouveau (compilador `nak`). El chroot de CI no tiene rustc y
# compilarlo bajo qemu es caro; para panfrost/gallium no hace falta nada de eso.
# Se quitan del APKBUILD:
NO_RUST = [
    # 1) rusticl fuera de aarch64 (el case que mete `rust` en makedepends)
    (r'^armv7\|aarch64\|x86\|x86_64\|loongarch64\)$',
     'armv7|x86|x86_64|loongarch64)'),
    # 2) nouveau fuera de los drivers vulkan de aarch64
    ('asahi,broadcom,freedreno,imagination,intel,nouveau,panfrost,virtio',
     'asahi,broadcom,freedreno,imagination,intel,panfrost,virtio'),
    # 3) y su subpaquete
    (r'^[ \t]*\$pkgname-vulkan-nouveau:_vulkan[ \t]*\n', ''),
]


# CLC (el compilador OpenCL C) NO es Rust, pero meson lo activa en cuanto se
# construye panfrost (`with_driver_using_cl` incluye with_gallium_panfrost), y
# pide LLVMSPIRVLib, SPIRV-Tools y clang. Sin esto el configure muere en
# "Dependency LLVMSPIRVLib not found".
DEPS = ('# nura: panfrost activa CLC; estas libs las necesita (no Rust).\n'
        'makedepends="$makedepends clang${_llvmver}-dev libclc-dev~${_llvmver} '
        'spirv-llvm-translator-dev spirv-tools-dev"\n')


# Recorte del build. En Mesa el tiempo se va en drivers que este telefono no
# usa; con panfrost + llvmpipe de respaldo basta. Se quitan TODOS los
# subpaquetes vulkan (turnip incluido) y se dejan solo esos dos drivers
# gallium. En x86 sin qemu esto no aplica; con qemu es la diferencia entre
# ~1.5-3 h y algo manejable.
NO_VULKAN = (r'^[ \t]*\$pkgname-(vulkan-[a-z0-9-]*:_vulkan(_layers)?|va-gallium:_va)[ \t]*\n', '')
# Sin radeonsi no hay driver VAAPI que empaquetar; desactivarlo evita otro fallo.
NO_VA = ('-Dgallium-va=enabled', '-Dgallium-va=disabled')
TRIM = ('# nura: solo los drivers que usa ruby (panfrost) + llvmpipe de respaldo\n'
        '_gallium_drivers="llvmpipe,panfrost"\n'
        '_vulkan_drivers=""\n'
        '_vulkan_layers=""\n')


def main(path):
    p = Path(path)
    s = p.read_text()

    if 'PAN_PROD_ID(9, 2, 4)' in s:
        print("ya parcheado: PAN_PROD_ID(9, 2, 4) presente")
        return 0

    # 0) quitar las partes que exigen Rust
    for pat, rep in NO_RUST:
        new_s, n = re.subn(pat, rep, s, flags=re.M)
        if n:
            s = new_s
            print("sin rust: %s -> %r (%d)" % (pat, rep, n))

    # 0a) recortar drivers: nada de vulkan, solo panfrost + llvmpipe
    s, n = re.subn(NO_VULKAN[0], NO_VULKAN[1], s, flags=re.M)
    print("drivers: %d subpaquetes vulkan/va fuera" % n)
    if NO_VA[0] in s:
        s = s.replace(NO_VA[0], NO_VA[1])
        print("va: desactivado")

    # 0b) dependencias de CLC, antes de prepare()/build()
    i = min(x for x in (s.find('prepare()'), s.find('build()')) if x >= 0)
    s = s[:i] + TRIM + DEPS + s[i:]

    # 1) meter el sed dentro de prepare()
    m = re.search(r'^prepare\(\)[^\n]*\{', s, re.M)
    if m:
        tail = s[m.end():m.end() + 300]
        extra = '' if 'default_prepare' in tail else '\n\tdefault_prepare'
        s = s[:m.end()] + extra + '\n\t' + SED + s[m.end():]
        how = 'dentro de prepare() existente'
    else:
        block = ('prepare() {\n\tdefault_prepare\n\t' + SED + '\n}\n\n')
        m = re.search(r'^build\(\)\s*\{', s, re.M)
        if not m:
            print("ERROR: no encuentro prepare() ni build()", file=sys.stderr)
            return 1
        s = s[:m.start()] + block + s[m.start():]
        how = 'prepare() nueva antes de build()'

    # 2) pkgrel+1 para que apk prefiera este paquete al de edge
    m = re.search(r'^pkgrel=(\d+)$', s, re.M)
    if not m:
        print("ERROR: no encuentro pkgrel=", file=sys.stderr)
        return 1
    s = s[:m.start()] + 'pkgrel=%d' % (int(m.group(1)) + 1) + s[m.end():]

    # 3) el sed tiene que quedar DENTRO de prepare(), no despues de build()
    prep = s.find('prepare()')
    g68 = s.find('PAN_PROD_ID(9, 2, 4)')
    build = s.find('build()')
    if not (0 <= prep < g68 < build):
        print("ERROR: la fila no quedo dentro de prepare()", file=sys.stderr)
        return 1

    p.write_text(s)
    print("parcheado (%s); pkgrel ahora %s" % (how, re.search(r'^pkgrel=(.*)$',
          s, re.M).group(1)))
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
