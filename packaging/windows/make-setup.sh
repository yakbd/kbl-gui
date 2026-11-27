#!/bin/sh
# yakbd-gui-setup.exe, the editor release's installer (yakbd-cli docs/installer-cli.md §12): the components YAKBD
# Editor (x64 and arm64, installed in YAKBD\Editor\) and YAKBD CLI (yakbd.exe for x86, x64 and arm64, in YAKBD\CLI\),
# built by the tagged CLI's own tools/toolsetup with -editor and the editor's declaration (-editor-json: version,
# Start-menu shortcut, per-user settings). Qt 6 has no 32-bit Windows build: on x86 the setup offers the CLI only.
#   packaging/windows/make-setup.sh <yakbd-cli checkout at the tag> <payload-x64> <payload-arm64> <exe-sha256.txt> <version> <out.exe>
# exe-sha256.txt ("<build> <sha256>" per line) comes from packaging/cli-release.sh: every yakbd.exe the setup carries
# must be byte-identical to the released one, which needs the Go named in the checkout's installer/stub-386.version.
# Runs on Linux or macOS (toolsetup cross-builds); needs go.
set -eu
src=$(cd "$1" && pwd)
x64=$(cd "$2" && pwd)
arm64=$(cd "$3" && pwd)
sums=$4
version=$5
out=$6
case "$out" in /*) ;; *) out="$(pwd)/$out" ;; esac
echo "$version" | grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?' || { echo "version '$version' is not a semantic version"; exit 1; }

want=$(tr -d '[:space:]' < "$src/installer/stub-386.version")
have=$(go env GOVERSION)
[ "$have" = "$want" ] || { echo "go is $have; the CLI release was built with $want"; exit 1; }

# The editor's declaration. Its per-user settings (src/main.cpp: organisation "YAKBD", application "YAKBD Editor";
# QSettings in the registry, Qt's caches in the local application data folder) go when the user asks for it.
tmp=$(mktemp -d)
cat > "$tmp/editor.json" <<JSON
{"version": "$version",
 "shortcuts": [{"name": "YAKBD Editor", "target": "yakbd-gui.exe"}],
 "userData": [{"kind": "key", "path": "HKCU\\\\Software\\\\YAKBD\\\\YAKBD Editor", "parentIfEmpty": true},
              {"kind": "dir", "path": "%LOCALAPPDATA%\\\\YAKBD\\\\YAKBD Editor", "parentIfEmpty": true}]}
JSON
cat "$tmp/editor.json"

(cd "$src" && go run ./tools/toolsetup -o "$out" -editor "x64=$x64" -editor "arm64=$arm64" \
  -editor-json "$tmp/editor.json") > "$tmp/log"
cat "$tmp/log"

for b in x86 x64 arm64; do
  carried=$(sed -n "s/^yakbd.exe $b \([0-9a-f]\{64\}\)\$/\1/p" "$tmp/log")
  released=$(sed -n "s/^$b //p" "$sums")
  [ -n "$carried" ] && [ "$carried" = "$released" ] || { echo "yakbd.exe $b: setup carries '$carried', release has '$released'"; exit 1; }
  echo "yakbd.exe $b in the setup is the released one ($released)"
done
rm -rf "$tmp"
