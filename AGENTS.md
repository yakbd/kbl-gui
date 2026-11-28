# yakbd-gui: agent instructions

The YAKBD desktop editor for `.kbl.json` keyboard layouts: C++ (document model, CLI bridge) and QML (views), Qt 6.12,
Qt Quick Controls. It edits the JSON document and runs the `yakbd` CLI for everything else (validate, convert,
pin, compile, installer). It holds no Windows facts (CLEAN-ROOM.md).

## Commands

    cmake -DPLATFORM=linux_gcc_64 -DDEST=$PWD/../qt -P tools/getqt.cmake   # pinned Qt, no Python, no account
    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$PWD/../qt
    cmake --build build
    ctest --test-dir build --output-on-failure      # unit, QML, CLI integration, screenshots (Xvfb)
    sh packaging/linux/package.sh build build/cli/yakbd dist   # AppImage + runtime sources
    tools/distro-check.sh dist                      # the AppImage on 5 distributions in Docker/Podman
    tools/desktop-check.sh dist                     # the AppImage on KDE Plasma (X11, portal dialog) and Wayland (weston, KWin)
    tools/targets-check.sh build/yakbd-gui out      # each target end to end against yakbd's own verdict: out/summary.txt
    sh tools/check-doc-links.sh                     # release links: releases/latest; doc links: blob/main (CI checks)

The build host for Linux is Ubuntu 22.04 (`tools/docker/build.Dockerfile`, the same as CI) so that the binaries run
on glibc 2.35 and later. CMake 3.25+ is needed (Qt 6.12's minimum). Configuring fetches the pinned CLI release
(`tools/cli-pins.cmake`) into `build/cli/` for the tests; pass `-DYAKBD_CLI=/path/to/yakbd` to use another.

## Layout

- `src/jsontree.*`: the order-preserving JSON tree with byte spans. Unedited parts are written back byte for byte;
  an edit rewrites only the edited value (or the container whose members changed, in its own style).
- `src/kbldocument.*`, `src/kbllayout.cpp`: the open document, undo/redo (QUndoStack of pointer edits), views for
  QML (layers, keyboard rows, bindings, dead keys).
- `src/cli.*`: QProcess runs of `yakbd`; `src/reportmodel.*`: the `-report` JSON (format 1, with a fallback for
  0.1.0-preview.1); `src/schemamodel.*`: forms from the bundled schemas (`qml/schema/<version>/`, the file's
  version first, the newest for members an older version lacks). Targets (`docs/targets.md`): `Cli` asks the CLI
  which ones it has (exit code 2 = unknown target or command) and passes `-target`; Windows keeps the command lines
  of the CLI before targets (`validate -windows`, no `-target`), so the pinned release still works.
- `qml/`: the QML module `Yakbd` (own AGENTS.md). `tests/`: own AGENTS.md. `packaging/`, `.github/workflows/`:
  packages and the release (own AGENTS.md). `tools/`: Qt/CLI pins and fetchers, platform checks, `make-icons/`.
- `docs/`: `targets.md`, `building.md`, `code-signing-policy.md`, `images/`; `VERIFIED.md`: what was checked where,
  with run ids; keep it current when a platform check runs.

## Documentation

- The documentation people read lives on the site, https://yakbd.github.io/ (repo `yakbd/yakbd.github.io`). It
  fetches `docs/targets.md`, `docs/building.md`, `docs/code-signing-policy.md` and `VERIFIED.md` from this repo at
  the commit pinned in its `sources.json`; the editor's overview and install pages are written on the site itself.
- README.md stays a short front door: one paragraph, the screenshot, platforms, download links, 3-6 site links,
  licence. New detail goes into a `docs/` file the site fetches, or onto a site page, not into the README.
- After every release that adds, removes or changes functionality, update the docs as part of that release: the
  files here (README platform line, `docs/*.md`, `VERIFIED.md`) AND the site: re-pin `yakbd-gui` in `sources.json`
  of `yakbd/yakbd.github.io`, update or add the editor and install pages, the What's new entry and the platform
  tags on the landing page. A release is not finished until the site shows it.
- Release notes come from `packaging/RELEASE-NOTES.md` (placeholders `@VERSION@`, `@CLI_VERSION@`, `@CLI_TAG@`):
  a few lines, download names, one line for checksums and attestation; details belong on the site.
- Links to releases are `releases/latest` (downloads `releases/latest/download/<name>`), links to files `blob/main`;
  `tools/check-doc-links.sh` enforces it.

## Rules

- No Python anywhere. Workflows run on push to `main` and `workflow_dispatch` only; Windows and macOS jobs are
  `workflow_dispatch` only (they are slow and use more CI minutes; run them when a check or a release needs them).
  Never poll `gh` in loops.
- Public-ready: no local paths, no names of internal rounds or agents in files or commit messages.
- Every Windows decision stays in the CLI. If the editor seems to need a Windows fact, ask the CLI (a report entry,
  a command) instead.
- Each user edit is one undo step; text fields commit on Enter or focus loss, not per keystroke.
- Format versions (SPEC §1.1) change only by edits, in `KblDocument::applyEdits`: the first 0.2-only member raises
  `kbl`/`$schema` in the same undo step, removing the last lowers them. Opening and saving never change bytes.
- No CLAUDE.md: this file and the nested AGENTS.md files are the only agent instructions.
