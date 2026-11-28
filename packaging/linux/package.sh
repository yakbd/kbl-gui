#!/bin/sh
# Linux package: an AppImage, with the yakbd CLI next to the editor: yakbd-gui-x86_64.AppImage (version-free, so
# releases/latest/download links work).
#   packaging/linux/package.sh <build dir> <yakbd binary> <output dir> [<folder with yakbd-cli-LICENSE and -NOTICE>]
# The last folder defaults to the binary's (tools/fetch-cli.cmake puts them there). The output also gets
# appimage-runtime-sources-<version>.tar.gz: the source of the AppImage runtime and of the libfuse (LGPL-2.1) built
# into it, which every release with the AppImage carries (NOTICE).
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
ver=$(sed -n "s/^YAKBD_GUI_VERSION_FULL:STRING=//p" "$build/CMakeCache.txt")
arch=x86_64
name="yakbd-gui-$ver-linux-$arch"
tmpdir="$out/.work"
rm -rf "$tmpdir"
mkdir -p "$tmpdir"

# 1. Install with Qt's deployment script (Qt libraries, plugins and QML modules next to the binary).
stage="$tmpdir/$name"
cmake --install "$build" --prefix "$stage" >/dev/null
# Qt deploys every Qt Quick Controls style, the test libraries and plugins it finds; the editor uses only Fusion
# (on Basic), has no network code and loads only SVG and ICO images, so the rest is left out.
for style in Material Imagine Universal FluentWinUI3; do
  rm -rf "$stage/qml/QtQuick/Controls/$style"
  rm -f "$stage"/lib/libQt6QuickControls2"$style"*.so*
done
rm -rf "$stage/qml/QtTest"
rm -f "$stage"/lib/libQt6Test.so* "$stage"/lib/libQt6QuickTest.so*
# The deployment script brings only the xcb platform plugin: add Wayland, and offscreen for headless checks.
qtprefix=$(cd "$(sed -n "s/^Qt6_DIR:PATH=//p" "$build/CMakeCache.txt")/../../.." && pwd)
for p in libqwayland.so libqoffscreen.so; do
  cp "$qtprefix/plugins/platforms/$p" "$stage/plugins/platforms/"
done
rm -rf "$stage/plugins/qmltooling" "$stage/plugins/egldeviceintegrations" "$stage/plugins/tls" \
  "$stage/plugins/networkinformation" "$stage/plugins/generic"
rm -f "$stage/plugins/imageformats/libqjpeg.so" "$stage/plugins/imageformats/libqgif.so"
rm -f "$stage"/lib/libQt6EglFS*.so* "$stage"/lib/libQt6EglFs*.so*
install -m 0755 "$cli" "$stage/bin/yakbd"
install -m 0644 "$clidocs/yakbd-cli-LICENSE" "$clidocs/yakbd-cli-NOTICE" "$stage/bin/"
"$stage/bin/yakbd" version

# 2. AppImage
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
srcdir="$tmpdir/appimage-runtime-sources-$ver"
mkdir -p "$srcdir"
fetch https://github.com/libfuse/libfuse/releases/download/fuse-3.15.0/fuse-3.15.0.tar.xz \
  70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0 "$srcdir/fuse-3.15.0.tar.xz"
curl -fsSL -o "$srcdir/type2-runtime-20251108-source.tar.gz" \
  https://github.com/AppImage/type2-runtime/archive/refs/tags/20251108.tar.gz
cat > "$srcdir/README.txt" <<EOF
Source of the AppImage type 2 runtime that yakbd-gui-x86_64.AppImage (version $ver) begins with:
- type2-runtime-20251108-source.tar.gz: https://github.com/AppImage/type2-runtime at tag 20251108 (MIT): the runtime,
  its build scripts (scripts/) and the patch it applies to libfuse (patches/libfuse/mount.c.diff).
- fuse-3.15.0.tar.xz: libfuse 3.15.0 (LGPL-2.1), the release the runtime is built from, unchanged
  (SHA-256 70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0).
The other libraries built into the runtime are listed in the editor's NOTICE.
EOF
tar -C "$tmpdir" -czf "$out/appimage-runtime-sources-$ver.tar.gz" "appimage-runtime-sources-$ver"
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
  --mksquashfs-opt -b --mksquashfs-opt 1M --mksquashfs-opt -Xcompression-level --mksquashfs-opt 19 \
  "$appdir" "$out/yakbd-gui-x86_64.AppImage"

rm -rf "$tmpdir"
(cd "$out" && sha256sum "yakbd-gui-x86_64.AppImage" "appimage-runtime-sources-$ver.tar.gz" > "$name.SHA256SUMS")
ls -l "$out"
