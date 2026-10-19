#!/bin/sh
# Stage a macOS .app bundle from an existing bakajiten binary and zip
# it. Run from the repo root (macOS only):
#   ./frontends/qt/packaging/bundle-macos-app.sh <binary> <zip>
# Requires: Xcode command-line tools (sips, iconutil, codesign) and
# Qt 6.10.3 exposed through Qt6_DIR (macdeployqt runs from there).
# Produces a self-contained bakajiten.app (Qt frameworks inside via
# macdeployqt, ad-hoc signed for arm64) zipped with ditto (which
# preserves the Framework symlinks plain zip mangles).
set -e
cd "$(dirname "$0")/../../.."

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <bakajiten-binary> <output-zip>" >&2
    exit 1
fi
BIN="$1"
OUT="$2"
for tool in sips iconutil codesign ditto; do
    command -v "$tool" >/dev/null || {
        echo "missing: $tool (Xcode command-line tools)" >&2
        exit 1
    }
done
if [ -z "${Qt6_DIR:-}" ]; then
    echo "missing: Qt6_DIR must point at the Qt6 CMake config dir" >&2
    exit 1
fi
MACDEPLOYQT="$Qt6_DIR/../../../bin/macdeployqt"
if [ ! -x "$MACDEPLOYQT" ]; then
    echo "missing: $MACDEPLOYQT" >&2
    exit 1
fi

VERSION="$(sed -n 's/^project(bakajiten_qt VERSION \([0-9.]*\)).*/\1/p' frontends/qt/CMakeLists.txt)"
if [ -z "$VERSION" ]; then
    echo "could not read version from CMakeLists.txt" >&2
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT INT TERM
APP="$WORK/bakajiten.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/bakajiten"
chmod +x "$APP/Contents/MacOS/bakajiten"

sed "s/@VERSION@/$VERSION/g" frontends/qt/packaging/macos/Info.plist \
    > "$APP/Contents/Info.plist"

# .icns from the 256px desktop icon (macOS tolerates the missing
# 512/1024 sizes; upscaling would only blur).
ICONSET="$WORK/icon.iconset"
mkdir -p "$ICONSET"
for s in 16 32 128 256; do
    sips -z "$s" "$s" frontends/qt/packaging/desktop/bakajiten.png \
        --out "$ICONSET/icon_${s}x${s}.png" >/dev/null
done
sips -z 32 32 frontends/qt/packaging/desktop/bakajiten.png \
    --out "$ICONSET/icon_16x16@2x.png" >/dev/null
sips -z 64 64 frontends/qt/packaging/desktop/bakajiten.png \
    --out "$ICONSET/icon_32x32@2x.png" >/dev/null
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/bakajiten.icns"

echo "==> running macdeployqt"
"$MACDEPLOYQT" "$APP"

echo "==> ad-hoc signing"
codesign --force --deep --sign - "$APP"

echo "==> zipping $OUT"
rm -f "$OUT"
ditto -c -k --keepParent "$APP" "$OUT"
