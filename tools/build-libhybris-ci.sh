#!/bin/sh
# Run in an ephemeral Alpine aarch64 container, never with Android vendor blobs.
set -eu
apk add --no-cache build-base git autoconf automake libtool pkgconf bsd-compat-headers \
    linux-headers libdrm-dev wayland-dev wayland-protocols \
    libx11-dev libxcb-dev mesa-dev
mkdir -p /work
cd /work
git clone https://github.com/libhybris/libhybris.git libhybris
git -C libhybris checkout 7079712a42ea2754adf747e70c6cc75764c8596e
git clone https://github.com/Halium/android-headers.git android-headers
git -C android-headers checkout 217a38e92cc64a96b4d1ac7469f30827d84485d3
cd /work/libhybris/hybris
export CFLAGS='-O2 -D_GNU_SOURCE'
export CXXFLAGS='-O2 -D_GNU_SOURCE'
./autogen.sh --prefix=/opt/nura-hybris --enable-arch=arm64 \
    --with-android-headers=/work/android-headers --enable-mali-quirks \
    --enable-wayland --with-default-egl-platform=null \
    --with-default-hybris-ld-library-path=/vendor/lib64:/system/lib64:/odm/lib64
make -j4
make install DESTDIR=/work/stage
cc -O2 -Wall -Wextra -Werror /src/tools/nura-hybris-probe.c \
    -I/work/stage/opt/nura-hybris/include \
    -L/work/stage/opt/nura-hybris/lib \
    -Wl,-rpath,/opt/nura-hybris/lib -lEGL -lGLESv2 \
    -o /work/nura-hybris-probe
mkdir -p /work/stage/opt/nura-hybris/bin
cp /work/nura-hybris-probe /work/stage/opt/nura-hybris/bin/
printf '%s\n' 'libhybris 7079712a42ea2754adf747e70c6cc75764c8596e' \
    'android-headers 217a38e92cc64a96b4d1ac7469f30827d84485d3' \
    'Header branch halium-16.0 declares ANDROID_VERSION_MAJOR 11.' \
    'Android 17 runtime compatibility is not validated.' > /work/stage/BUILD-INFO.txt
tar -C /work/stage -czf /out/libhybris-alpine-experimental.tar.gz .
cd /out
sha256sum libhybris-alpine-experimental.tar.gz > SHA256SUMS
