#!/bin/sh
# Build Mesa main for glibc inside an Arch Linux ARM container.
# Arch ships a recent LLVM (the Debian trixie build segfaults in mesa_clc with
# LLVM 19), and the resulting packages are glibc/Arch, i.e. ABI-compatible with
# the Kupfer rootfs.
set -eu
pacman -Syu --noconfirm --needed >/dev/null
pacman -S --noconfirm --needed base-devel meson ninja python-mako python-yaml \
    python-packaging libdrm wayland wayland-protocols libx11 libxcb \
    libxshmfence libxdamage libxfixes xorgproto elfutils libdisplay-info \
    zstd zlib expat llvm clang glslang libclc python >/dev/null
mkdir -p /work
cd /work
curl -fsSL --max-time 900 -o mesa.tar.gz \
  https://gitlab.freedesktop.org/mesa/mesa/-/archive/767fbf7c6492f64549c90e9a080ab5417895a9ec/mesa-767fbf7c6492f64549c90e9a080ab5417895a9ec.tar.gz
tar -xzf mesa.tar.gz
cd mesa-767fbf7c6492f64549c90e9a080ab5417895a9ec
grep -q 'PAN_PROD_ID(9, 2, 4)' src/panfrost/model/pan_model.c || {
    echo "ERROR: sin fila G68" >&2; exit 1; }
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
n=$(grep -a -c 'Mali-G68' /work/stage/opt/nura-mesa/lib/libgallium*.so 2>/dev/null || true)
echo "Mali-G68: $n"
[ "${n:-0}" -ge 1 ] || { echo "sin fila G68" >&2; exit 1; }
printf '%s\n' 'Mesa main glibc (Arch ARM), commit 767fbf7c.' \
    'LD_LIBRARY_PATH=/opt/nura-mesa/lib LIBGL_DRIVERS_PATH=/opt/nura-mesa/lib/dri' \
    > /work/stage/BUILD-INFO.txt
tar -C /work/stage -czf /out/mesa-glibc-g68.tar.gz .
cd /out && sha256sum mesa-glibc-g68.tar.gz > SHA256SUMS
ls -l /out
