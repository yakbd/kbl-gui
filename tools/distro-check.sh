#!/bin/sh
# Runs the Linux packages on several distributions in containers (Docker or Podman, rootless is fine):
# installs only the runtime libraries a desktop has anyway, then from the extracted AppImage the self-test (open,
# edit, save, reopen, yakbd validate), an offscreen screenshot and a screenshot under Xvfb with the GTK platform
# theme (light and dark), lists the X11 libraries the xcb plugin still lacks without GTK, and runs the AppImage's
# self-test. Screenshots go to <dist>/distro-shots/.
#   tools/distro-check.sh <folder with the .AppImage> [image ...]
set -eu
dist=$(cd "$1" && pwd)
shift
images=${*:-"ubuntu:22.04 ubuntu:24.04 ubuntu:26.04 debian:12 fedora:43"}
data=$(cd "$(dirname "$0")/../tests/data" && pwd)
engine=${CONTAINER_ENGINE:-docker}
appimage=$(ls "$dist"/yakbd-gui-x86_64.AppImage | head -1)
mkdir -p "$dist/distro-shots"
status=0
for image in $images; do
  tag=$(echo "$image" | tr ':/' '--')
  case "$image" in
    fedora*)
      base="dnf -y -q install mesa-libGL mesa-libEGL libxkbcommon fontconfig dbus-libs glib2 dejavu-sans-fonts tar gzip >/dev/null"
      x11="dnf -y -q install xorg-x11-server-Xvfb xorg-x11-xauth which >/dev/null"
      xcb="dnf -y -q install xcb-util-cursor xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil libxkbcommon-x11 >/dev/null"
      gtk="dnf -y -q install gtk3 >/dev/null" ;;
    *)
      apt="DEBIAN_FRONTEND=noninteractive apt-get -qq install -y --no-install-recommends"
      base="apt-get -qq update && $apt libgl1 libegl1 libxkbcommon0 libfontconfig1 libdbus-1-3 libgssapi-krb5-2 \$(apt-cache show libglib2.0-0t64 >/dev/null 2>&1 && echo libglib2.0-0t64 || echo libglib2.0-0) fonts-dejavu-core >/dev/null"
      x11="$apt xvfb xauth >/dev/null"
      xcb="$apt libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-shape0 libxcb-xkb1 libxkbcommon-x11-0 >/dev/null"
      gtk="$apt \$(apt-cache show libgtk-3-0t64 >/dev/null 2>&1 && echo libgtk-3-0t64 || echo libgtk-3-0) >/dev/null" ;;
  esac
  echo "== $image"
  log=$(mktemp)
  if $engine run --rm -v "$dist:/dist" -v "$data:/data:ro" "$image" sh -c "
      set -e
      $base
      mkdir /t && cp /dist/$(basename "$appimage") /t/app && cd /t && ./app --appimage-extract >/dev/null && cd squashfs-root/usr
      ./bin/yakbd-gui --self-test /data/latin-altgr-deadkeys.kbl.json
      QT_QPA_PLATFORM=offscreen ./bin/yakbd-gui --grab /tmp/shot.png --grab-delay 1500 /data/minimal-qwerty.kbl.json
      $x11
      echo 'X11 libraries missing before GTK:' \$(ldd plugins/platforms/libqxcb.so lib/libQt6XcbQpa.so.6 | grep 'not found' | sort -u | awk '{print \$1}')
      $gtk
      $xcb
      echo 'X11 libraries still missing:' \$(ldd plugins/platforms/libqxcb.so lib/libQt6XcbQpa.so.6 | grep -c 'not found' || true)
      for mode in light dark; do
        [ \$mode = dark ] && export GTK_THEME=Adwaita:dark || export GTK_THEME=Adwaita
        QT_QPA_PLATFORMTHEME=gtk3 xvfb-run -a -s '-screen 0 1440x900x24' ./bin/yakbd-gui --no-settings --appearance \$mode \
          --size 1360x860 --grab /dist/distro-shots/$tag-\$mode.png --grab-delay 2500 \
          --select /hardware/layers/4/bindings/KeyE /data/latin-altgr-deadkeys.kbl.json 2>/dev/null | grep grab
      done
      /t/squashfs-root/AppRun --self-test /data/minimal-qwerty.kbl.json
    " >"$log" 2>&1; then
    grep -E "^(PASS|grab|X11)" "$log"
    echo "PASS $image"
  else
    grep -vE "locale|UTF-8|reconfigure|for more|^qt\.qpa|DBus" "$log" | tail -20
    echo "FAIL $image"
    status=1
  fi
  rm -f "$log"
done
exit $status
