#!/usr/bin/env bash
# Build a Bakajiten AppImage (x86_64) via linuxdeploy. Everything —
# Release configure + build as well as deploy — runs inside the
# bakajiten-appimage container, so the Qt the app links is the Qt
# linuxdeploy bundles (no host toolchain or Qt involved).
#
#   frontends/qt/packaging/build-appimage.sh [APPDIR] [OUTPUT] [clean]
#
# Defaults live under frontends/qt/build-appimage (AppDir +
# builddir + Bakajiten-x86_64.AppImage). SKIP_BUILD=1 skips
# configure + build and deploys the given APPDIR as-is. Data
# artifacts (*.bin) are never bundled:
# the app resolves them at runtime from /usr/share/bakajiten or the
# dev-tree CWD fallbacks.
#
# Local image build (Qt download ~10 min, cached in its own layer):
#   docker build -f frontends/qt/packaging/docker/Dockerfile.appimage -t bakajiten-appimage .
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
QTAPP="$REPO/frontends/qt"
if ! docker image inspect bakajiten-appimage >/dev/null 2>&1; then
    echo "missing image: build it first (see header)" >&2
    exit 1
fi
if [ "${1:-}" = "clean" ] || [ "${2:-}" = "clean" ]; then
    # Container-side: the tree is root-owned, host rm fails on it.
    docker run --rm -v "$REPO:/work" bakajiten-appimage \
        sh -c 'rm -rf /work/frontends/qt/build-appimage'
    exit 0
fi
APPDIR="${1:-$QTAPP/build-appimage/AppDir}"
OUTPUT="${2:-$QTAPP/build-appimage/Bakajiten-x86_64.AppImage}"
BUILD="$QTAPP/build-appimage/builddir"
# Persistent ccache (CI caches this dir across runs; the container
# paths it hashes are stable, so hits survive fresh checkouts).
CCACHE_HOST="$QTAPP/build-ccache/appimage"
mkdir -p "$CCACHE_HOST"
# The repo mounts at /work inside; translate host paths.
to_container_path() {
    case "$1" in
        "$REPO"/*) echo "/work/${1#"$REPO"/}" ;;
        *) echo "$1" ;;
    esac
}
C_APPDIR="$(to_container_path "$APPDIR")"
C_OUTPUT="$(to_container_path "$OUTPUT")"
C_BUILD="$(to_container_path "$BUILD")"

if [ "${SKIP_BUILD:-0}" != "1" ]; then
    echo "==> configuring Release build in $BUILD"
    # Fresh tree every build (container-side: root-owned, host rm
    # fails): a stale cache disagrees with the container paths and
    # CMake refuses it.
    docker run --rm -v "$REPO:/work" bakajiten-appimage \
        sh -c 'rm -rf /work/frontends/qt/build-appimage/builddir /work/frontends/qt/build-appimage/AppDir'
    docker run --rm -v "$REPO:/work" -v "$CCACHE_HOST:/ccache" \
        -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=1G bakajiten-appimage \
        sh -c "export HOME=/root; cmake -S /work/frontends/qt -B $C_BUILD \
            -DQt6_DIR=/opt/qt/lib/cmake/Qt6 \
            -Dyaml-cpp_DIR=/opt/yaml-cpp-app/lib/cmake/yaml-cpp \
            -DCMAKE_BUILD_TYPE=Release -DBUILD_QT_TESTS=OFF \
            -DBAKAJITEN_UPDATE_ARTIFACT=appimage \
            -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
            && cmake --build $C_BUILD --target bakajiten -j\"\$(nproc)\" && ccache -s"
    echo "==> installing to $APPDIR"
    docker run --rm -v "$REPO:/work" bakajiten-appimage \
        sh -c "DESTDIR=\"$C_APPDIR\" cmake --install $C_BUILD --prefix /usr"
    # The cache is container-written (root); hand it back so the
    # host (and CI's cache save) can read it.
    docker run --rm -v "$CCACHE_HOST:/ccache" bakajiten-appimage \
        chown -R "$(id -u):$(id -g)" /ccache
else
    echo "==> SKIP_BUILD=1, deploying the given $APPDIR"
fi

echo "==> deploying AppImage"
exec docker run --rm -v "$REPO:/work" bakajiten-appimage \
    appimage-deploy "$C_APPDIR" "$C_OUTPUT"
