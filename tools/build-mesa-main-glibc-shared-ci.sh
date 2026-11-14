#!/bin/sh
# Build Mesa main (27.0.0-devel) for glibc.  Main already contains the
# Mali-G68 row, so no patch is needed; the point is to see whether its CLC
# builds with Debian's LLVM (26.2.6 segfaulted in mesa_clc).
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends build-essential ca-certificates curl \
    python3 meson ninja-build pkg-config bison flex gettext zstd xz-utils \
    libdrm-dev libwayland-dev wayland-protocols libwayland-egl-backend-dev \
    libx11-dev libxcb1-dev libxshmfence-dev libxdamage-dev libxfixes-dev \
    x11proto-dev libelf-dev libdisplay-info-dev libzstd-dev zlib1g-dev \
    libexpat1-dev libudev-dev llvm-dev libclang-dev glslang-tools \
    python-is-python3 python3-mako python3-yaml python3-packaging
for p in clang clang-19 libclang-cpp19 libclang-cpp-dev libclang-common-19-dev \
         libclang-common-dev libclc libclc-19 libclc-dev opencl-c-headers \
         spirv-tools libspirv-tools-dev libllvmspirvlib-dev libllvmspirvlib-19-dev \
         libpolly-19-dev libpolly-dev; do
    apt-get install -y --no-install-recommends "$p" >/dev/null 2>&1 || true
done
mkdir -p /work
cd /work
curl -fsSL --max-time 900 -o mesa.tar.gz \
  https://gitlab.freedesktop.org/mesa/mesa/-/archive/767fbf7c6492f64549c90e9a080ab5417895a9ec/mesa-767fbf7c6492f64549c90e9a080ab5417895a9ec.tar.gz
tar -xzf mesa.tar.gz
cd mesa-767fbf7c6492f64549c90e9a080ab5417895a9ec
if ! grep -q 'PAN_PROD_ID(9, 2, 4)' src/panfrost/model/pan_model.c; then
    echo "ERROR: main no trae la fila G68" >&2; exit 1
fi
meson setup output --prefix=/opt/nura-mesa --buildtype=release \
    -Dplatforms=wayland -Dgallium-drivers=panfrost,llvmpipe \
    -Dvulkan-drivers= -Dgallium-rusticl=false -Dgallium-va=disabled \
    -Dglx=disabled -Degl=enabled -Dgbm=enabled -Dgles2=enabled \
    -Dllvm=enabled -Dshared-llvm=enabled -Dpanfrost-rust=false \
    -Ddri-drivers-path=/opt/nura-mesa/dri
ninja -C output -j4
mkdir -p /work/stage/opt/nura-mesa/lib/dri
cp -a output/src/gallium/targets/dri/libgallium*.so* /work/stage/opt/nura-mesa/lib/ 2>/dev/null || true
cp -a output/src/gallium/targets/dri/*_dri.so /work/stage/opt/nura-mesa/lib/dri/ 2>/dev/null || true
cp -a output/src/egl/libEGL.so* /work/stage/opt/nura-mesa/lib/ 2>/dev/null || true
cp -a output/src/mesa/glapi/es2api/libGLESv2.so* /work/stage/opt/nura-mesa/lib/ 2>/dev/null || true
cp -a output/src/gbm/libgbm.so* /work/stage/opt/nura-mesa/lib/ 2>/dev/null || true
mkdir -p /work/stage/opt/nura-mesa/lib/aarch64-linux-gnu/gbm /work/stage/opt/nura-mesa/lib/gbm
cp -a output/src/gbm/backends/dri/dri_gbm.so /work/stage/opt/nura-mesa/lib/aarch64-linux-gnu/gbm/ 2>/dev/null || true
cp -a output/src/gbm/backends/dri/dri_gbm.so /work/stage/opt/nura-mesa/lib/gbm/ 2>/dev/null || true
ls /work/stage/opt/nura-mesa/lib/gbm/ 2>/dev/null
for f in /usr/lib/aarch64-linux-gnu/libLLVM-19.so*; do
    [ -e "$f" ] && cp -a "$f" /work/stage/opt/nura-mesa/lib/ || true
done
ls /work/stage/opt/nura-mesa/lib | head -20
n=$(grep -a -c 'Mali-G68' /work/stage/opt/nura-mesa/lib/*.so 2>/dev/null | awk -F: '{s+=$2} END{print s+0}')
echo "Mali-G68 en libgallium: $n"
[ "$n" -ge 1 ] || { echo "sin la fila G68" >&2; exit 1; }
printf '%s\n' 'Mesa main (27.0.0-devel) glibc, commit 767fbf7c6492f64549c90e9a080ab5417895a9ec.' \
    'Usar con LIBGL_DRIVERS_PATH=/opt/nura-mesa/lib/dri y LD_LIBRARY_PATH=/opt/nura-mesa/lib.' \
    > /work/stage/BUILD-INFO.txt
tar -C /work/stage -czf /out/mesa-glibc-g68.tar.gz .
cd /out && sha256sum mesa-glibc-g68.tar.gz > SHA256SUMS
