#!/bin/sh
# The checks a person would make on a Mac, scripted as far as a CI runner allows (the Packages workflow runs it on
# macOS). It drives the packaged app with System Events (AppleScript) and screencapture; where macOS privacy
# controls (TCC) refuse UI scripting or screen capture, the line says so: the in-app tests (tst_window: ⌘
# shortcuts, appearance switch, menu titles) cover what they can, and "Open Anyway" stays a human check.
#   tools/macos-desktop-check.sh "<path to YAKBD Editor.app>/Contents/MacOS" <layout.kbl.json> <output dir>
# Prints one RESULT line per check; exit status 0 (the workflow keeps the lines, not the status).
app=$1
layout=$2
out=$3
mkdir -p "$out"
exe="$app/YAKBD Editor"
result() { echo "RESULT $1: $2"; }

osascript -e 'tell application "System Events" to tell appearance preferences to set dark mode to false' 2>/dev/null

# 1. The application menu: Qt moves About, Settings and Quit there by their titles.
"$exe" --no-settings "$layout" >/dev/null 2>&1 &
pid=$!
sleep 10
menu=$(osascript -e "tell application \"System Events\" to tell (first process whose unix id is $pid) to get name of every menu item of menu 1 of menu bar item 2 of menu bar 1" 2>&1)
case "$menu" in
  *"About YAKBD Editor"*Settings*Quit*) result app-menu "pass: $menu" ;;
  *"assistive access"*|*"not allowed"*|*"-25211"*|*"-1719"*) result app-menu "not checked: UI scripting refused by TCC ($menu)" ;;
  *) result app-menu "FAIL: $menu" ;;
esac
bar=$(osascript -e "tell application \"System Events\" to tell (first process whose unix id is $pid) to get name of every menu bar item of menu bar 1" 2>&1)
result menu-bar "$bar"
if screencapture -x "$out/macos-window.png" 2>/dev/null && [ -s "$out/macos-window.png" ]; then
  result screen-capture "pass ($out/macos-window.png)"
else
  result screen-capture "not available (TCC screen recording)"
fi
kill $pid 2>/dev/null; wait $pid 2>/dev/null

# 2. Live light/dark: the system switches to dark while the app runs; the app's grab 12 s after start says which
#    scheme it sees and shows it.
"$exe" --no-settings --size 1360x860 --grab "$out/macos-live-dark.png" --grab-delay 12000 "$layout" > "$out/live.txt" 2>&1 &
pid=$!
sleep 4
switched=$(osascript -e 'tell application "System Events" to tell appearance preferences to set dark mode to true' 2>&1 && echo ok)
wait $pid
case "$switched" in
  ok) if grep -q "dark=1" "$out/live.txt"; then result live-dark "pass: $(grep grab "$out/live.txt")"
      else result live-dark "FAIL: the system is dark, the app says $(grep grab "$out/live.txt")"; fi ;;
  *) result live-dark "not checked: System Events refused the appearance change ($switched)" ;;
esac
osascript -e 'tell application "System Events" to tell appearance preferences to set dark mode to false' 2>/dev/null

# 3. The native Open dialog (NSOpenPanel), on screen.
"$exe" --no-settings --show-open-dialog "$layout" >/dev/null 2>&1 &
pid=$!
sleep 8
if screencapture -x "$out/macos-open-dialog.png" 2>/dev/null && [ -s "$out/macos-open-dialog.png" ]; then
  windows=$(osascript -e "tell application \"System Events\" to tell (first process whose unix id is $pid) to get {name, subrole} of every window" 2>&1)
  result open-dialog "screenshot $out/macos-open-dialog.png; windows: $windows"
else
  result open-dialog "not captured (TCC screen recording)"
fi
kill $pid 2>/dev/null; wait $pid 2>/dev/null
exit 0
