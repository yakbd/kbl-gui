#!/bin/sh
# Linux packages: a tarball and an AppImage, each with the yakbd CLI next to the editor.
#   packaging/linux/package.sh <build dir> <yakbd binary> <output dir> [<folder with yakbd-cli-LICENSE and -NOTICE>]
# The last folder defaults to the binary's (tools/fetch-cli.cmake puts them there). The output also gets the source
# of the AppImage runtime and of the libfuse it is built from (LGPL-2.1): release them with the AppImage (NOTICE).
# Needs curl, tar and (for the AppImage) FUSE or APPIMAGE_EXTRACT_AND_RUN=1. The AppImage tools are pinned by
# version and SHA-256 below.
set -eu
build=$(cd "$1" && pwd)
cli=$2
out=$3
clidocs=${4:-$(dirname "$cli")}
for doc in LICENSE NOTICE; do
  [ -f "$clidocs/yakbd-cli-$doc" ] || { echo "missing $clidocs/yakbd-cli-$doc (the CLI's $doc must ship with it)"; exit 1; }
done
mkdir -p "$out"
out=$(cd "$out" && pwd)
ver=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")
arch=x86_64
name="yakbd-gui-$ver-linux-$arch"
tmpdir="$out/.work"
rm -rf "$tmpdir"
mkdir -p "$tmpdir"

# 1. Install with Qt's deployment script (Qt libraries, plugins and QML modules next to the binary).
stage="$tmpdir/$name"
cmake --install "$build" --prefix "$stage" >/dev/null
# Qt deploys every Qt Quick Controls style and the test libraries it finds; the editor uses Fusion (and the
# FluentWinUI3 style can be chosen with QT_QUICK_CONTROLS_STYLE), so the others are left out.
for style in Material Imagine Universal; do
  rm -rf "$stage/qml/QtQuick/Controls/$style"
  rm -f "$stage"/lib/libQt6QuickControls2"$style"*.so*
done
rm -rf "$stage/qml/QtTest"
rm -f "$stage"/lib/libQt6Test.so* "$stage"/lib/libQt6QuickTest.so*
# The deployment script brings only the xcb platform plugin: add Wayland, and offscreen for headless checks.
qtprefix=$(cd "$(sed -n "s/^Qt6_DIR:PATH=//p" "$build/CMakeCache.txt")/../../.." && pwd)
for p in libqwayland.so libqoffscreen.so libqminimal.so; do
  cp "$qtprefix/plugins/platforms/$p" "$stage/plugins/platforms/"
done
rm -rf "$stage/plugins/qmltooling" "$stage/plugins/egldeviceintegrations"
rm -f "$stage"/lib/libQt6EglFS*.so* "$stage"/lib/libQt6EglFs*.so*
install -m 0755 "$cli" "$stage/bin/yakbd"
install -m 0644 "$clidocs/yakbd-cli-LICENSE" "$clidocs/yakbd-cli-NOTICE" "$stage/bin/"
"$stage/bin/yakbd" version

# 2. Tarball: unpack anywhere and run bin/yakbd-gui.
tar -C "$tmpdir" -czf "$out/$name.tar.gz" "$name"

# 3. AppImage
tools="$tmpdir/tools"
mkdir -p "$tools"
fetch() { # url sha256 file
  curl -fsSL -o "$3" "$1"
  echo "$2  $3" | sha256sum -c -
}
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
  ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 "$tools/appimagetool"
fetch https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
  2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d "$tools/runtime"
chmod +x "$tools/appimagetool"
# Source of what the AppImage embeds: the runtime at its tag (build scripts, libfuse patch) and libfuse 3.15.0.
fetch https://github.com/libfuse/libfuse/releases/download/fuse-3.15.0/fuse-3.15.0.tar.xz \
  70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0 "$out/fuse-3.15.0.tar.xz"
curl -fsSL -o "$out/type2-runtime-20251108-source.tar.gz" \
  https://github.com/AppImage/type2-runtime/archive/refs/tags/20251108.tar.gz
appdir="$tmpdir/AppDir"
mkdir -p "$appdir/usr"
cp -a "$stage/." "$appdir/usr/"
cp "$stage/share/applications/io.github.yakbd.gui.desktop" "$appdir/"
cp "$stage/share/icons/hicolor/scalable/apps/io.github.yakbd.gui.svg" "$appdir/"
cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
here=$(dirname "$(readlink -f "$0")")
exec "$here/usr/bin/yakbd-gui" "$@"
EOF
chmod +x "$appdir/AppRun"
ARCH=$arch "$tools/appimagetool" --appimage-extract-and-run --runtime-file "$tools/runtime" --no-appstream \
  "$appdir" "$out/$name.AppImage"

rm -rf "$tmpdir"
(cd "$out" && sha256sum "$name.tar.gz" "$name.AppImage" fuse-3.15.0.tar.xz type2-runtime-20251108-source.tar.gz > "$name.SHA256SUMS")
ls -l "$out"
