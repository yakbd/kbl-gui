#!/bin/sh
# The AppImage on KDE Plasma (X11) and on Wayland (weston and KWin, headless), in a Fedora container with the KDE
# packages (tools/docker/desktops.Dockerfile); rootless Docker or Podman is enough. One RESULT line per check;
# screenshots and logs in <dist>/desktop-check/.
#   tools/desktop-check.sh <folder with yakbd-gui-x86_64.AppImage>
# With TARGETS_CLI=<a Linux yakbd that has targets> it also runs tools/targets-check.sh on KDE Plasma (X11) with that
# yakbd: <folder>/desktop-check/targets/summary.txt.
set -eu
if [ "${1:-}" != --inner ]; then
  dist=$(cd "$1" && pwd)
  here=$(cd "$(dirname "$0")" && pwd)
  engine=${CONTAINER_ENGINE:-docker}
  image=${DESKTOPS_IMAGE:-yakbd-gui-desktops:fc43}
  $engine build -q -t "$image" -f "$here/docker/desktops.Dockerfile" "$here/docker" >/dev/null
  mkdir -p "$dist/desktop-check"
  cli=""
  [ -z "${TARGETS_CLI:-}" ] || cli="-v $(cd "$(dirname "$TARGETS_CLI")" && pwd)/$(basename "$TARGETS_CLI"):/cli/yakbd:ro"
  exec $engine run --rm -v "$dist:/dist" -v "$here:/tools:ro" -v "$here/../tests/data:/data:ro" $cli \
    "$image" dbus-run-session -- sh /tools/desktop-check.sh --inner
fi

# ---- inside the container, in a D-Bus session ----
out=/dist/desktop-check
export XDG_RUNTIME_DIR=/tmp/xdg HOME=/tmp/home LANG=C.UTF-8
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$HOME/.config"
cd /tmp
cp /dist/yakbd-gui-x86_64.AppImage app
./app --appimage-extract >/dev/null
gui=/tmp/squashfs-root/AppRun
layout=/data/latin-altgr-deadkeys.kbl.json
result() { echo "RESULT $1: $2"; }
grab() { # name extra-env... : runs the editor, grabs its window, prints the editor's grab line
  name=$1; shift
  env "$@" "$gui" --no-settings --size 1360x860 --grab "$out/$name.png" --grab-delay 4000 \
    --select /hardware/layers/4/bindings/KeyE "$layout" > "$out/$name.log" 2>&1
  grep '^grab' "$out/$name.log" || echo "no grab (see $name.log)"
}

# Open, edit, save, reopen, validate (no display needed).
if "$gui" --self-test "$layout" > "$out/selftest.log" 2>&1; then result self-test "pass"; else result self-test "FAIL"; fi

# 1. KDE Plasma on X11: Qt's built-in KDE theme reads the colour scheme from kdeglobals (Breeze Light, then Breeze
#    Dark), without the Plasma platform-theme plugin (that plugin is built against the system's Qt, not ours).
Xvfb :1 -screen 0 1440x900x24 >/dev/null 2>&1 &
sleep 2
export DISPLAY=:1 XDG_CURRENT_DESKTOP=KDE KDE_SESSION_VERSION=6 KDE_FULL_SESSION=true
cp /usr/share/color-schemes/BreezeLight.colors "$HOME/.config/kdeglobals"
result kde-x11-light "$(grab kde-x11-light)"
cp /usr/share/color-schemes/BreezeDark.colors "$HOME/.config/kdeglobals"
result kde-x11-dark "$(grab kde-x11-dark)"
grep -h "platformtheme\|PlatformTheme" "$out"/kde-x11-*.log | head -3 > "$out/kde-x11-theme.txt" || true

# 1b. The target picker end to end on KDE Plasma (X11), with the yakbd given as TARGETS_CLI (docs/targets.md).
if [ -x /cli/yakbd ]; then
  YAKBD_CLI=/cli/yakbd sh /tools/targets-check.sh "$gui" "$out/targets" /data/macos-option-deadkeys.kbl.json \
    /data/xkb-caps-deadkeys.kbl.json /data/minimal-qwerty.kbl.json > "$out/targets.log" 2>&1
  result targets "$(cat "$out/targets/totals.txt" 2>/dev/null || echo "no summary (see targets.log)") (desktop-check/targets/summary.txt)"
fi

# 2. KDE's file dialog through the XDG desktop portal (Qt's xdgdesktopportal theme; the KDE backend draws it).
/usr/libexec/xdg-desktop-portal-kde >"$out/portal-kde.log" 2>&1 &
/usr/libexec/xdg-desktop-portal -r >"$out/portal.log" 2>&1 &
sleep 4
QT_QPA_PLATFORMTHEME=xdgdesktopportal "$gui" --no-settings --show-open-dialog "$layout" >"$out/portal-dialog.log" 2>&1 &
pid=$!
sleep 10
xwd -root -silent | magick xwd:- "$out/kde-portal-open-dialog.png"
owner=$(xwininfo -root -tree | grep -o '("xdg-desktop-portal-kde" "[^"]*")' | head -1)
if [ -n "$owner" ]; then
  result kde-portal-dialog "KDE's file dialog through the portal (window class $owner; kde-portal-open-dialog.png)"
else
  result kde-portal-dialog "FAIL: no window of xdg-desktop-portal-kde (kde-portal-open-dialog.png shows what opened)"
fi
kill $pid 2>/dev/null || true
unset DISPLAY XDG_CURRENT_DESKTOP KDE_SESSION_VERSION KDE_FULL_SESSION

# 3. Wayland, weston (headless, software rendering), no desktop environment: Fusion with its own light palette.
weston --backend=headless --renderer=pixman --socket=wl-weston --width=1440 --height=900 >"$out/weston.log" 2>&1 &
sleep 3
result wayland-weston "$(grab wayland-weston QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY=wl-weston)"

# 4. Wayland, KWin (KDE's compositor, virtual output), with Breeze Dark in kdeglobals.
export XDG_CURRENT_DESKTOP=KDE KDE_SESSION_VERSION=6 KDE_FULL_SESSION=true
# A copy: the installed binary has file capabilities a rootless container may not grant.
cp /usr/bin/kwin_wayland /tmp/kwin_wayland
/tmp/kwin_wayland --virtual --socket wl-kwin --width 1440 --height 900 >"$out/kwin.log" 2>&1 &
sleep 5
result wayland-kwin-dark "$(grab wayland-kwin-dark QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY=wl-kwin)"
cp /usr/share/color-schemes/BreezeLight.colors "$HOME/.config/kdeglobals"
result wayland-kwin-light "$(grab wayland-kwin-light QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY=wl-kwin)"
