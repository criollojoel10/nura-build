#!/bin/sh
# Isolated Debian build: no vendor binaries or firmware are copied into CI.
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends build-essential ca-certificates git \
    autoconf automake libtool pkg-config linux-libc-dev libdrm-dev \
    libwayland-dev wayland-protocols libx11-dev libxcb1-dev \
    libegl-dev libgles-dev libgbm-dev
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
    --disable-wayland --with-default-egl-platform=null \
    --with-default-hybris-ld-library-path=/vendor/lib64:/system/lib64:/odm/lib64
make -j4
make install DESTDIR=/work/stage
cc -O2 -Wall -Wextra -Werror /src/tools/nura-hybris-probe.c \
    -I/work/stage/opt/nura-hybris/include \
    -L/work/stage/opt/nura-hybris/lib \
    -Wl,-rpath,/opt/nura-hybris/lib -lEGL -lGLESv2 -lhybris-common \
    -o /work/nura-hybris-probe
mkdir -p /work/stage/opt/nura-hybris/bin
cp /work/nura-hybris-probe /work/stage/opt/nura-hybris/bin/
cc -O1 -shared -fPIC /src/tools/bionic-tls-pad.c \
    -o /work/stage/opt/nura-hybris/lib/nura-bionic-tls-pad.so
readelf -lW /work/stage/opt/nura-hybris/lib/nura-bionic-tls-pad.so | grep -q TLS
echo "== compat symbols"
cc -O1 -shared -fPIC /src/tools/hybris-linker-compat.c \
    -o /work/stage/opt/nura-hybris/lib/nura-hybris-linker-compat.so
echo "cc rc=$?"
readelf --dyn-syms -W /work/stage/opt/nura-hybris/lib/nura-hybris-linker-compat.so \
    | grep 'android_' | head -5
test -f /work/stage/opt/nura-hybris/lib/nura-hybris-linker-compat.so
echo "== panfrost probe"
cc -O2 -Wall -Wextra /src/tools/nura-panfrost-probe.c \
    -o /work/stage/opt/nura-hybris/bin/nura-panfrost-probe \
    -lEGL -lGLESv2 -lgbm
echo "panfrost probe rc=$?"
cc -O2 -Wall -Wextra /src/tools/nura-hybris-blobtest.c \
    -o /work/stage/opt/nura-hybris/bin/nura-hybris-blobtest -ldl
echo "blob test rc=$?"
cc -O2 -Wall -Wextra /src/tools/nura-egl-probe.c \
    -o /work/stage/opt/nura-hybris/bin/nura-egl-probe -lEGL -lGLESv2
echo "egl probe rc=$?"
cc -O2 -Wall -Wextra /src/tools/nura-es3-probe.c \
    -o /work/stage/opt/nura-hybris/bin/nura-es3-probe -lEGL -lGLESv2
echo "es3 probe rc=$?"
cc -O2 -Wall -Wextra /src/tools/nura-draw-probe.c \
    -o /work/stage/opt/nura-hybris/bin/nura-draw-probe -lEGL -lGLESv2
echo "draw probe rc=$?"
printf '%s\n' 'Experimental glibc build; not executable directly on musl.' \
    'libhybris 7079712a42ea2754adf747e70c6cc75764c8596e' \
    'android-headers 217a38e92cc64a96b4d1ac7469f30827d84485d3 (reports Android 11)' \
    'Android runtime compatibility, TLS safety and vendor services are untested.' \
    > /work/stage/BUILD-INFO.txt
tar -C /work/stage -czf /out/libhybris-glibc-experimental.tar.gz .
cd /out
sha256sum libhybris-glibc-experimental.tar.gz > SHA256SUMS
