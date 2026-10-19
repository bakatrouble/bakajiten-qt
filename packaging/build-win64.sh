#!/bin/sh
# Static Windows cross build. Run from the repo root:
#   ./frontends/qt/packaging/build-win64.sh [clean]
# Configures in frontends/qt/build-win64/builddir and
# produces frontends/qt/build-win64/bakajiten-win64.zip
# (bakajiten.exe inside): one file, Windows system DLLs only. The
# build tree is container-owned (root); `clean` wipes it from inside
# the container (host rm fails on those files). First run builds the
# image (Qt, ~40-60 min, cached in its own layer):
#   docker build -f frontends/qt/packaging/docker/Dockerfile.win64 \
#     -t bakajiten-cross:win64 .
set -e
cd "$(dirname "$0")/../../.."
QTAPP="$PWD/frontends/qt"
if [ "$1" = "clean" ]; then
    docker run --rm -v "$QTAPP:/qtapp" bakajiten-cross:win64 \
        sh -c 'rm -rf /qtapp/build-win64'
    exit 0
fi
mkdir -p "$QTAPP/build-win64/builddir"
# Persistent ccache (CI caches this dir across runs; the container
# paths it hashes are stable, so hits survive fresh checkouts).
CCACHE_HOST="$QTAPP/build-ccache/win64"
mkdir -p "$CCACHE_HOST"
docker run --rm -v "$PWD:/work" -v "$QTAPP/build-win64/builddir:/build" \
    -v "$CCACHE_HOST:/ccache" -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=1G \
    bakajiten-cross:win64 \
    sh -c 'export HOME=/root; command -v ccache >/dev/null || dnf install -y ccache; cmake -S /work/frontends/qt -B /build \
        --toolchain /opt/toolchain/win64-toolchain.cmake \
        -DQt6_DIR=/opt/qt6-static/lib/cmake/Qt6 \
        -Dyaml-cpp_DIR=/opt/yaml-cpp-win/lib/cmake/yaml-cpp \
        -DZLIB_INCLUDE_DIR=/usr/x86_64-w64-mingw32/sys-root/mingw/include \
        -DZLIB_LIBRARY=/usr/x86_64-w64-mingw32/sys-root/mingw/lib/libz.a \
        -DZLIB_LIBRARY_RELEASE=/usr/x86_64-w64-mingw32/sys-root/mingw/lib/libz.a \
        -DRust_CARGO_TARGET=x86_64-pc-windows-gnu \
        -DBAKAJITEN_BUNDLE_CJK_FONT=ON \
        -DBAKAJITEN_UPDATE_ARTIFACT=win64 \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DBUILD_QT_TESTS=OFF -DCMAKE_EXE_LINKER_FLAGS=-static \
        && cmake --build /build && ccache -s'
# The cache is container-written (root); hand it back so the host
# (and CI's cache save) can read it.
docker run --rm -v "$CCACHE_HOST:/ccache" bakajiten-cross:win64 \
    chown -R "$(id -u):$(id -g)" /ccache
# Zip in the container (python3 is already there for aqt): the exe
# needs no permission bits preserved. The font ships inside the
# binary (BAKAJITEN_BUNDLE_CJK_FONT); only its license rides along.
rm -f "$QTAPP/build-win64/bakajiten-win64.zip"
docker run --rm -v "$QTAPP/build-win64:/out" -v "$QTAPP/build-win64/builddir:/build" bakajiten-cross:win64 \
    python3 -c "import zipfile; z = zipfile.ZipFile('/out/bakajiten-win64.zip', 'w', zipfile.ZIP_DEFLATED); z.write('/build/bakajiten.exe', 'bakajiten.exe'); z.write('/opt/fonts/OFL.txt', 'OFL.txt'); z.close()"
