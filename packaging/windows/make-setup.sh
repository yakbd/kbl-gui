#!/bin/sh
# yakbd-setup.exe for every Windows architecture: the yakbd tool's own installer (yakbd-cli docs/installer-cli.md
# §12.1) with yakbd.exe for x86, x64 and arm64, and the editor's payloads for x64 and arm64 as extra files (Qt 6
# has no 32-bit Windows build, so on x86 the setup installs the CLI only).
#   packaging/windows/make-setup.sh <yakbd-cli checkout at the tag> <payload-x64> <payload-arm64> <CLI release dir> <out.exe>
# The CLI release dir holds that tag's yakbd-<v>-windows-{x86,x64,arm64}.zip: every yakbd.exe the setup carries
# must be byte-identical to the released one, which needs the Go named in the checkout's installer/stub-386.version.
# Runs on Linux or macOS (toolsetup cross-builds); needs go and unzip.
set -eu
src=$(cd "$1" && pwd)
x64=$(cd "$2" && pwd)
arm64=$(cd "$3" && pwd)
rel=$(cd "$4" && pwd)
out=$5
case "$out" in /*) ;; *) out="$(pwd)/$out" ;; esac

want=$(tr -d '[:space:]' < "$src/installer/stub-386.version")
have=$(go env GOVERSION)
[ "$have" = "$want" ] || { echo "go is $have; the CLI release was built with $want"; exit 1; }

log=$(mktemp)
(cd "$src" && go run ./tools/toolsetup -o "$out" -extra "x64=$x64" -extra "arm64=$arm64") > "$log"
cat "$log"

# Every carried yakbd.exe is the released one.
tmp=$(mktemp -d)
for b in x86 x64 arm64; do
  zip=$(ls "$rel"/yakbd-*-windows-$b.zip)
  mkdir -p "$tmp/$b"
  unzip -q -o "$zip" -d "$tmp/$b"
  released=$(find "$tmp/$b" -name yakbd.exe -exec sha256sum {} \; | cut -d' ' -f1)
  carried=$(sed -n "s/^yakbd.exe $b \([0-9a-f]\{64\}\)\$/\1/p" "$log")
  [ -n "$carried" ] && [ "$carried" = "$released" ] || { echo "yakbd.exe $b: setup carries '$carried', release has $released"; exit 1; }
  echo "yakbd.exe $b in the setup is the released one ($released)"
done
rm -rf "$tmp" "$log"
