#!/usr/bin/env bash
# Deploy a prebuilt install tree as an AppImage. Runs ONLY inside the
# bakajiten-appimage container (baked in at /usr/local/bin/
# appimage-deploy): the linuxdeploy tooling lives at
# /opt/linuxdeploy and QMAKE/NO_STRIP come from the image ENV.
#   appimage-deploy <APPDIR> <OUTPUT>
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <APPDIR> <OUTPUT>" >&2
    exit 1
fi
APPDIR="$1"
OUTPUT="$2"

LD_BASE="${LINUXDEPLOY_DIR:-/opt/linuxdeploy}"
if [[ ! -x "$LD_BASE/linuxdeploy-x86_64/AppRun" ]]; then
    echo "missing linuxdeploy tooling at $LD_BASE" >&2
    exit 1
fi
LD_LINUXDEPLOY="$LD_BASE/linuxdeploy-x86_64/AppRun"
# Plugins resolve by exact name off PATH.
export PATH="$LD_BASE/linuxdeploy-x86_64/usr/bin:$LD_BASE/linuxdeploy-plugin-qt-x86_64/usr/bin:$LD_BASE/linuxdeploy-plugin-appimage-x86_64/usr/bin:$PATH"

# Ship every platform plugin, not just xcb: the Qt plugin deploys
# only libqxcb.so by default; the rest arrive via
# EXTRA_PLATFORM_PLUGINS (filenames under the Qt platforms dir —
# libqwayland* also pulls in its decoration/shell integration).
if [[ -z "${EXTRA_PLATFORM_PLUGINS:-}" ]]; then
    export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
fi
# The installed binary carries no RPATH, so the loader (and
# linuxdeploy's ldd pass) only finds the Qt it was built against via
# LD_LIBRARY_PATH. Derive it from the same qmake the Qt plugin uses.
QMAKE_BIN="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
QT_LIBDIR="$("$QMAKE_BIN" -query QT_INSTALL_LIBS)"
export LD_LIBRARY_PATH="$QT_LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo "==> running linuxdeploy"
# The Qt plugin picks Qt5's qmake by default; the app builds
# against Qt6.
if [[ -z "${QMAKE:-}" ]]; then
    export QMAKE
    QMAKE="$(command -v qmake6)"
fi
# The Qt plugin ships an old binutils that cannot strip modern
# objects (.relr.dyn); skip its strip pass (data dominates the
# bundle size anyway).
if [[ -z "${NO_STRIP:-}" ]]; then
    export NO_STRIP=1
fi
OUTPUT="$OUTPUT" \
"$LD_LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/bakajiten" \
    --desktop-file "$APPDIR/usr/share/applications/bakajiten.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/bakajiten.png" \
    --plugin qt

echo "==> restoring pristine Qt plugins"
# The Qt plugin's bundled patchelf/binutils predate RELR-era
# binaries (GCC 15 toolchain) and corrupt the deployed plugins
# (they crash at load with the fault inside their own headers).
# Restore pristine copies: with the AppRun LD_LIBRARY_PATH below
# they resolve their Qt deps unpatched, no rpath rewriting needed.
QT_PLUGINS_DIR="$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)"
cp -rf "$QT_PLUGINS_DIR"/. "$APPDIR/usr/plugins/"

echo "==> installing AppRun launcher"
# linuxdeploy leaves AppRun as a symlink to usr/bin/bakajiten, but Qt6
# resolves qt.conf (and applicationDirPath) against the unresolved
# symlink dir, so no platform plugin is found. A resolving launcher
# fixes both the plugin path and the share/bakajiten data lookup.
# It also reproduces the library path setup of linuxdeploy's own
# AppRun, so the bundled Qt wins over any host Qt.
rm -f "$APPDIR/AppRun"
cat > "$APPDIR/AppRun" <<'LAUNCHER_EOF'
#!/bin/sh
here="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$here/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$here/usr/plugins"
exec "$here/usr/bin/bakajiten" "$@"
LAUNCHER_EOF
chmod +x "$APPDIR/AppRun"

echo "==> packing AppImage"
OUTPUT="$OUTPUT" \
"$LD_LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/bakajiten.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/bakajiten.png" \
    --output appimage

echo "==> done: $OUTPUT"
