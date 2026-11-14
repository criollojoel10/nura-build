#!/bin/sh
# Build Mesa 26.2.6 for glibc with the Mali-G68 model row (same version as
# Arch's package, so the DRI ABI matches the rootfs).
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends build-essential ca-certificates curl \
    python3 meson ninja-build pkg-config bison flex gettext zstd xz-utils \
    libdrm-dev libwayland-dev wayland-protocols libwayland-egl-backend-dev libspirv-tools-dev libx11-dev libxcb1-dev \
    libxshmfence-dev libxdamage-dev libxfixes-dev x11proto-dev \
    libelf-dev libdisplay-info-dev libzstd-dev zlib1g-dev libexpat1-dev \
    libudev-dev llvm-dev libclang-dev glslang-tools python-is-python3 \
    python3-mako python3-yaml python3-packaging
for p in libpolly-19-dev libpolly-dev; do
    apt-get install -y --no-install-recommends "$p" >/dev/null 2>&1 && { echo "polly: $p"; break; }
done
for p in libllvmspirvlib-dev libllvmspirvlib-19-dev libllvmspirvlib-18-dev spirv-llvm-translator-dev; do
    if apt-get install -y --no-install-recommends "$p" >/dev/null 2>&1; then
        echo "instalado $p"; break
    fi
done
mkdir -p /work
cd /work
curl -fsSL --max-time 900 -o mesa.tar.xz https://archive.mesa3d.org/mesa-26.2.6.tar.xz
tar -xJf mesa.tar.xz
cd mesa-26.2.6
python3 - <<'PY'
from pathlib import Path
p = Path('src/panfrost/model/pan_model.c')
s = p.read_text()
row = ('   VALHALL_MODEL(PAN_PROD_ID(9, 2, 4), 0, "G68", "G78", MODEL_ANISO(ALL), '
       'MODEL_TB_SIZES(16384, 8192), MODEL_RATES_X(2, 4, 8, 32, 32, 8)),\n')
if 'PAN_PROD_ID(9, 2, 4)' in s:
    print('la fila G68 ya estaba')
else:
    marker = '   VALHALL_MODEL(PAN_PROD_ID(10, 8, 7)'
    i = s.index(marker)
    p.write_text(s[:i] + row + s[i:])
    print('fila G68 insertada')
PY
meson setup output --prefix=/opt/nura-mesa --buildtype=release \
    -Dplatforms=wayland -Dgallium-drivers=panfrost,llvmpipe \
    -Dvulkan-drivers= -Dgallium-rusticl=false -Dgallium-va=disabled \
    -Dglx=disabled -Degl=enabled -Dgbm=enabled -Dgles2=enabled \
    -Dllvm=enabled -Dshared-llvm=disabled -Ddri-drivers-path=/opt/nura-mesa/dri
ninja -C output -j4
mkdir -p /work/stage/opt/nura-mesa/lib/dri
cp -a output/src/gallium/targets/dri/libgallium*.so* /work/stage/opt/nura-mesa/lib/ 2>/dev/null || true
cp -a output/src/gallium/targets/dri/*_dri.so /work/stage/opt/nura-mesa/lib/dri/ 2>/dev/null || true
n=$(grep -a -c 'Mali-G68' /work/stage/opt/nura-mesa/lib/dri/panfrost_dri.so || true)
echo "Mali-G68 en panfrost_dri.so: $n"
[ "$n" -ge 1 ] || { echo "sin la fila G68" >&2; exit 1; }
printf '%s\n' 'Mesa 26.2.6 glibc con la fila Mali-G68 (PAN_PROD_ID 9,2,4).' \
    'ABI igual a Arch 26.2.6; usar con LIBGL_DRIVERS_PATH y LD_LIBRARY_PATH.' \
    > /work/stage/BUILD-INFO.txt
tar -C /work/stage -czf /out/mesa-glibc-g68.tar.gz .
cd /out && sha256sum mesa-glibc-g68.tar.gz > SHA256SUMS
