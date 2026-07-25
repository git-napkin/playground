#!/bin/sh
set -eu

VERSION="1.0.0"
OUTPUT="${1:-PluginPlayground-${VERSION}.pkg}"
SRC="$(cd "$(dirname "$0")" && pwd)"

echo "[+] Building..."
mkdir -p "$SRC/.build"
cmake -S "$SRC" -B "$SRC/.build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$SRC/.build"

echo "[+] Staging..."
STAGING="$(mktemp -d)"
trap "rm -rf '$STAGING'" EXIT

PKG_ROOT="$STAGING/root/opt/pluginplayground"
mkdir -p "$PKG_ROOT/bin" "$PKG_ROOT/lib" "$PKG_ROOT/tweaks"
mkdir -p "$STAGING/root/Applications"

cp "$SRC/.build/grant"                    "$PKG_ROOT/bin/"
cp "$SRC/.build/libfangs_hook.dylib"      "$PKG_ROOT/lib/"
cp "$SRC/.build/libplayground_opener.dylib" "$PKG_ROOT/lib/"
cp "$SRC/fridagum.dylib"                   "$PKG_ROOT/lib/"
cp -R "$SRC/.build/configurator.app"       "$STAGING/root/Applications/Plugin Playground.app/"

# ownership and permissions
chown -R 0:0 "$STAGING/root"
chmod 755 "$PKG_ROOT/bin/grant"
chmod 755 "$PKG_ROOT/lib/libfangs_hook.dylib"
chmod 755 "$PKG_ROOT/lib/libplayground_opener.dylib"
chmod 755 "$PKG_ROOT/lib/fridagum.dylib"
chmod 755 "$PKG_ROOT/tweaks"

echo "[+] Building component package..."
pkgbuild --root "$STAGING/root" \
         --identifier "com.pluginplayground.core" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGING/PluginPlaygroundCore.pkg" > /dev/null

echo "[+] Building distribution package..."
productbuild --distribution "$SRC/installer/Distribution.xml" \
             --package-path "$STAGING" \
             --resources "$SRC/installer" \
             "$SRC/$OUTPUT"

echo "[+] Created $OUTPUT"
echo "    Install: sudo installer -pkg \"$OUTPUT\" -target /"
