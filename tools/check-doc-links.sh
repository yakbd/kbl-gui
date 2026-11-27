#!/bin/sh
# Documentation links to releases always point at the latest (releases/latest, releases/latest/download/<name>) and
# to docs on main (blob/main/...): never a tagged release page or a tag-pinned file. Covers every tracked .md file,
# the release notes template (packaging/RELEASE-NOTES.md) among them. Build pins (tools/*.cmake) are not docs.
set -eu
cd "$(dirname "$0")/.."
if git grep -nE 'releases/tag/|blob/v[0-9]' -- '*.md'; then
  echo "link to a tagged release or a tag-pinned file (use releases/latest or blob/main)"
  exit 1
fi
echo "doc links: ok"
