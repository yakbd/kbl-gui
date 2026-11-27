#!/bin/sh
# Screenshots of the editor in light and dark, under Xvfb with the GTK platform theme (as on a GNOME desktop).
#   screenshots.sh <yakbd-gui> <output folder> <test data folder>
# Fails when a shot is missing or blank, or when light and dark look the same.
set -eu
app=$1
out=$2
data=$3
stat="$(dirname "$app")/tests/pngstat"
[ -x "$stat" ] || stat="$(dirname "$app")/pngstat"
mkdir -p "$out"
export QT_QPA_PLATFORMTHEME=gtk3
unset QT_QPA_PLATFORM
fail=0
shot() { # name appearance page file [extra args]
  name=$1; mode=$2; page=$3; file=$4; shift 4
  if [ "$mode" = dark ]; then gtk=Adwaita:dark; else gtk=Adwaita; fi
  GTK_THEME=$gtk xvfb-run -a -s "-screen 0 1440x900x24" \
    "$app" --no-settings --appearance "$mode" --size 1360x860 --page "$page" --grab "$out/$name-$mode.png" \
    --grab-delay 2500 "$@" "$data/$file" || { echo "FAIL $name-$mode: the app did not save a shot"; fail=1; return; }
}
for mode in light dark; do
  shot keyboard "$mode" keyboard latin-altgr-deadkeys.kbl.json --select /hardware/layers/4/bindings/KeyE
  shot deadkeys "$mode" deadkeys latin-altgr-deadkeys.kbl.json --select /deadKeys/acute
  shot windows-target "$mode" windows windows-features.kbl.json
  shot problems "$mode" keyboard minimal-qwerty.kbl.json --validate --select /hardware/layers/1/bindings/KeyA
done
for name in keyboard deadkeys windows-target problems; do
  "$stat" "$out/$name-light.png" "$out/$name-dark.png" || fail=1
done
exit $fail
