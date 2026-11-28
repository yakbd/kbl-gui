#!/bin/sh
# The yakbd CLI of one tagged yakbd-cli release (0.1.0-preview.3 or later), checked, in the form the packages need:
#   packaging/cli-release.sh <tag> <out dir> [--source-only]
# Downloads the release's SHA256SUMS, yakbd-cli-setup.exe and the Linux x86-64 and macOS archives, checks them
# against SHA256SUMS and their build provenance attestations (gh), and unpacks yakbd, LICENSE and NOTICE. The release
# has no separate Windows yakbd.exe: the script checks out the tag, rebuilds yakbd-cli-setup.exe with the tag's own
# tools/toolsetup and the Go named in installer/stub-386.version (it must equal the released file byte for byte),
# and builds yakbd.exe for x64 and arm64 with the same flags; each must equal what that setup carries.
# Writes into <out>: linux-amd64/yakbd, macos/yakbd (universal), windows-x64/yakbd.exe, windows-arm64/yakbd.exe,
# yakbd-cli-LICENSE, yakbd-cli-NOTICE, exe-sha256.txt ("<build> <sha256>" for x86, x64, arm64), src/ (the checkout).
# --source-only (Windows packages only, never a release): for a tag whose release is not public yet (a draft),
# everything Windows needs comes from the tag's source; nothing is compared with released files.
# Needs gh (authenticated), git, go of the right version, tar, sha256sum.
set -eu
tag=$1
mkdir -p "$2"
out=$(cd "$2" && pwd)
mode=${3:-}
v=${tag#v}
# Releases up to 0.1.0-preview.3 put the version in the archive names (yakbd-<v>-<platform>.tar.gz); later ones are
# version-free (yakbd-<platform>.tar.gz).
case $v in
  0.1.0-preview.[123]) arc=yakbd-$v- ;;
  *) arc=yakbd- ;;
esac
dl="$out/download"

if [ "$mode" != --source-only ]; then
  rm -rf "$dl" && mkdir -p "$dl"
  gh release download "$tag" -R yakbd/yakbd-cli -D "$dl" -p SHA256SUMS -p yakbd-cli-setup.exe \
    -p "${arc}linux-amd64.tar.gz" -p "${arc}macos.tar.gz"
  (cd "$dl" && sha256sum -c --ignore-missing SHA256SUMS && [ "$(sha256sum -c --ignore-missing SHA256SUMS | grep -c ': OK$')" = 3 ])
  for f in "$dl"/yakbd-cli-setup.exe "$dl"/yakbd-*.tar.gz; do
    gh attestation verify "$f" -R yakbd/yakbd-cli >/dev/null
    echo "attested $(basename "$f")"
  done
  for p in linux-amd64 macos; do
    mkdir -p "$out/$p"
    tar -xzf "$dl/$arc$p.tar.gz" -C "$out/$p"
    [ -x "$out/$p/yakbd" ] || { echo "no yakbd in $arc$p.tar.gz"; exit 1; }
  done
fi

# Windows: rebuild from the tag and compare.
rm -rf "$out/src"
git -c core.autocrlf=false -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" https://github.com/yakbd/yakbd-cli "$out/src"
want=$(tr -d '[:space:]' < "$out/src/installer/stub-386.version")
[ "$(go env GOVERSION)" = "$want" ] || { echo "go is $(go env GOVERSION); the release was built with $want"; exit 1; }
(cd "$out/src" && go run ./tools/toolsetup -o "$out/yakbd-cli-setup.rebuilt.exe") > "$out/plain-setup.log"
cat "$out/plain-setup.log"
if [ "$mode" = --source-only ]; then
  echo "source only: $tag is not compared with released files"
  # LICENSE and NOTICE travel with the CLI in every package.
  cp "$out/src/LICENSE" "$out/yakbd-cli-LICENSE"
  cp "$out/src/NOTICE" "$out/yakbd-cli-NOTICE"
else
  released=$(sha256sum < "$dl/yakbd-cli-setup.exe" | cut -d' ' -f1)
  rebuilt=$(sha256sum < "$out/yakbd-cli-setup.rebuilt.exe" | cut -d' ' -f1)
  if [ "$released" != "$rebuilt" ]; then
    # A signed release differs by its signature; then the carried yakbd.exe builds cannot be read from it this way.
    echo "the released yakbd-cli-setup.exe ($released) is not the one rebuilt from $tag ($rebuilt)"; exit 1
  fi
  echo "yakbd-cli-setup.exe rebuilt from $tag is the released one ($released)"
  cp "$out/linux-amd64/LICENSE" "$out/yakbd-cli-LICENSE"
  cp "$out/linux-amd64/NOTICE" "$out/yakbd-cli-NOTICE"
fi
sed -n 's/^yakbd.exe \([a-z0-9]*\) \([0-9a-f]\{64\}\)$/\1 \2/p' "$out/plain-setup.log" > "$out/exe-sha256.txt"
for b in x64:amd64 arm64:arm64; do
  build=${b%%:*}; goarch=${b#*:}
  mkdir -p "$out/windows-$build"
  (cd "$out/src" && env CGO_ENABLED=0 GOFLAGS= GOEXPERIMENT= GOTOOLCHAIN=local GO386=sse2 GOAMD64=v1 GOARM64=v8.0 \
    GOOS=windows GOARCH=$goarch go build -o "$out/windows-$build/yakbd.exe" -trimpath -buildvcs=false \
    -ldflags="-s -w -buildid= -X github.com/yakbd/yakbd-cli/internal/cli.Release=$v" ./cmd/yakbd)
  have=$(sha256sum < "$out/windows-$build/yakbd.exe" | cut -d' ' -f1)
  expect=$(sed -n "s/^$build //p" "$out/exe-sha256.txt")
  [ "$have" = "$expect" ] || { echo "yakbd.exe $build: built $have, the CLI setup carries $expect"; exit 1; }
  echo "yakbd.exe $build rebuilt, equal to the CLI setup's ($have)"
done
rm -rf "$dl" "$out/yakbd-cli-setup.rebuilt.exe" "$out/plain-setup.log"
