#!/bin/sh
# macOS package: YAKBD Editor.app (universal: Qt's clang_64 build is x86_64 + arm64) with a universal yakbd CLI made with
# lipo from the release's two binaries, in a .dmg. Unsigned (ad-hoc signature only): see README, "Open Anyway".
#   packaging/macos/package.sh <build dir> <yakbd-darwin-amd64> <yakbd-darwin-arm64> <output dir> [<CLI docs folder>]
# The CLI docs folder holds yakbd-cli-LICENSE and yakbd-cli-NOTICE (default: the folder of the amd64 binary).
set -eu
build=$(cd "$1" && pwd)
amd64=$2
arm64=$3
clidocs=${5:-$(dirname "$amd64")}
mkdir -p "$4"
out=$(cd "$4" && pwd)
ver=$(sed -n "s/^YAKBD_GUI_VERSION_FULL:STRING=//p" "$build/CMakeCache.txt")
name="YAKBD-Editor-$ver"  # universal (x86_64 + arm64)
tmpdir="$out/.work"
rm -rf "$tmpdir"
mkdir -p "$tmpdir/stage"

# 1. Install: Qt's deployment script (macdeployqt) puts the frameworks, plugins and QML modules into the bundle.
cmake --install "$build" --prefix "$tmpdir/stage" >/dev/null
app="$tmpdir/stage/YAKBD Editor.app"
[ -d "$app" ] || { echo "no YAKBD Editor.app in the install"; exit 1; }
# Only what the editor uses: the macOS style (on Fusion, Basic and Qt Quick Native Style); no network code; SVG and
# ICO images only.
for style in Material Imagine Universal FluentWinUI3 Windows; do  # not iOS: the macOS style uses its impl
  rm -rf "$app/Contents/Resources/qml/QtQuick/Controls/$style"
  rm -rf "$app/Contents/Frameworks/QtQuickControls2$style".framework "$app/Contents/Frameworks/QtQuickControls2$style"StyleImpl.framework
done
rm -rf "$app/Contents/Resources/qml/QtTest" "$app/Contents/Frameworks/QtTest.framework" \
       "$app/Contents/Frameworks/QtQuickTest.framework" "$app/Contents/PlugIns/qmltooling" \
       "$app/Contents/PlugIns/tls" "$app/Contents/PlugIns/networkinformation" "$app/Contents/PlugIns/generic" \
       "$app/Contents/PlugIns/imageformats/libqjpeg.dylib" "$app/Contents/PlugIns/imageformats/libqgif.dylib"

# 2. The CLI, universal, next to the editor's executable.
lipo -create "$amd64" "$arm64" -output "$app/Contents/MacOS/yakbd"
chmod 0755 "$app/Contents/MacOS/yakbd"
lipo -info "$app/Contents/MacOS/yakbd"
"$app/Contents/MacOS/yakbd" version

# 3. Licences inside the bundle.
mkdir -p "$app/Contents/Resources/licenses"
cp LICENSE NOTICE "$app/Contents/Resources/"
cp "$clidocs/yakbd-cli-LICENSE" "$clidocs/yakbd-cli-NOTICE" "$app/Contents/Resources/"
cp third_party/licenses/* "$app/Contents/Resources/licenses/"

# 4. Ad-hoc signature (arm64 code must be signed to run at all; this is not a Developer ID signature).
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

# 5. Disk image with the app and a link to /Applications.
dmgdir="$tmpdir/dmg"
mkdir -p "$dmgdir"
cp -R "$app" "$dmgdir/"
ln -s /Applications "$dmgdir/Applications"
hdiutil create -volname "YAKBD Editor $ver" -srcfolder "$dmgdir" -ov -format ULMO "$out/$name.dmg"
rm -rf "$tmpdir"
(cd "$out" && shasum -a 256 "$name.dmg" > "$name.SHA256SUMS")
ls -l "$out"
