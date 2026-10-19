#!/bin/sh
# Static Linux build. Run from the repo root:
#   ./frontends/qt/packaging/build-linuxstatic.sh [clean]
# Configures in frontends/qt/build-linuxstatic/builddir and
# produces
# frontends/qt/build-linuxstatic/bakajiten-linux64-static.tar.gz
# (bakajiten inside): one file with Qt linked statically (system
# glibc/XCB stay dynamic — fully static glibc breaks DNS/TLS at
# runtime, so no `-static` link here, unlike the Windows cross
# build). The build tree is container-owned (root); `clean` wipes it
# from inside the container (host rm fails on it). First run builds
# the image (Qt, ~40-60 min, cached in its own layer):
#   docker build -f frontends/qt/packaging/docker/Dockerfile.linux-static \
#     -t bakajiten-cross:linux-static .
set -e
cd "$(dirname "$0")/../../.."
QTAPP="$PWD/frontends/qt"
if [ "$1" = "clean" ]; then
    docker run --rm -v "$QTAPP:/qtapp" bakajiten-cross:linux-static \
        sh -c 'rm -rf /qtapp/build-linuxstatic'
    exit 0
fi
mkdir -p "$QTAPP/build-linuxstatic/builddir"
# Persistent ccache (CI caches this dir across runs; the container
# paths it hashes are stable, so hits survive fresh checkouts).
CCACHE_HOST="$QTAPP/build-ccache/linux-static"
mkdir -p "$CCACHE_HOST"
docker run --rm -v "$PWD:/work" -v "$QTAPP/build-linuxstatic/builddir:/build" \
    -v "$CCACHE_HOST:/ccache" -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=1G \
    bakajiten-cross:linux-static \
    sh -c 'export HOME=/root; cmake -S /work/frontends/qt -B /build \
        -DQt6_DIR=/opt/qt6-static/lib/cmake/Qt6 \
        -Dyaml-cpp_DIR=/opt/yaml-cpp-linux/lib/cmake/yaml-cpp \
        -DOPENSSL_ROOT_DIR=/opt/openssl \
        -DBAKAJITEN_UPDATE_ARTIFACT=linux-static \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DBUILD_QT_TESTS=OFF \
        && cmake --build /build && ccache -s'
# The cache is container-written (root); hand it back so the host
# (and CI's cache save) can read it.
docker run --rm -v "$CCACHE_HOST:/ccache" bakajiten-cross:linux-static \
    chown -R "$(id -u):$(id -g)" /ccache
rm -f "$QTAPP/build-linuxstatic/bakajiten-linux64-static.tar.gz"
tar -czf "$QTAPP/build-linuxstatic/bakajiten-linux64-static.tar.gz" -C "$QTAPP/build-linuxstatic/builddir" bakajiten
