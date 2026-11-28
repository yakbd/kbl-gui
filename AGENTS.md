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
    sh tools/check-doc-links.sh                  # release links: releases/latest; doc links: blob/main (CI checks)

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
- `qml/`: the QML module `Yakbd` (static plugin). `Main.qml` holds the actions, menus and dialogs.
- `VERIFIED.md`: what was checked where, with run ids; keep it current when a platform check runs.
- `tests/`: Qt Test (`tst_jsontree`, `tst_document`, `tst_cli`), Qt Quick Test (`tst_qml` + `qml/tst_views.qml`),
  `screenshots.sh` + `pngstat`.
- `packaging/{linux,windows,macos}`, `.github/workflows/`. Every package carries the CLI's LICENSE and NOTICE
  (`yakbd-cli-*`, from the CLI release's archives via `tools/fetch-cli.cmake` or `packaging/cli-release.sh`); the Linux packaging
  also writes the AppImage runtime and libfuse source archives that a release with an AppImage must attach.
- `tools/make-icons/`: renders the SVG icon into the `.ico` and `.icns`.

## Rules

- No Python anywhere. Workflows run on push to `main` and `workflow_dispatch` only; Windows and macOS jobs are
  `workflow_dispatch` only (they are slow and use more CI minutes; run them when a check or a release needs them). Never poll `gh` in loops.
- Public-ready: no local paths, no names of internal rounds or agents in files or commit messages.
- Every Windows decision stays in the CLI. If the editor seems to need a Windows fact, ask the CLI (a report entry,
  a command) instead.
- Each user edit is one undo step; text fields commit on Enter or focus loss, not per keystroke.
- Format versions (SPEC §1.1) change only by edits, in `KblDocument::applyEdits`: the first 0.2-only member raises
  `kbl`/`$schema` in the same undo step, removing the last lowers them. Opening and saving never change bytes.

## Gotchas

- `set_source_files_properties(Ui.qml ... QT_QML_SINGLETON_TYPE TRUE)` must come before `qt_add_qml_module`, or
  `Ui.*` is undefined at run time.
- `layer` is a FINAL property of every Item: use `layerIndex`.
- QML bindings on document data must depend on `doc.revision`: `(doc.revision, doc.valueAt(p))`.
- A component that loads itself (SchemaForm via SchemaField) must get its pointer before it creates children
  (`Loader.setSource(url, props)`), or it recurses over the whole schema.
- `offscreen` reports no colour scheme; light/dark screenshots need xcb under Xvfb with `QT_QPA_PLATFORMTHEME=gtk3`.
- Windows: `package.ps1` moves Qt and the VC runtime into `qt\` (a private side-by-side assembly the exe's manifest
  names: `-DYAKBD_QT_SUBFOLDER=ON`; PowerShell variable names ignore case, so none may shadow `-Arch`);
  `yakbd-gui-setup.exe` must be built with the Go of the CLI tag's `installer/stub-386.version` and from an LF checkout,
  or the `yakbd.exe` it carries differs from the release's (`make-setup.sh` refuses that). Windows builds are MSVC
  (x64 and arm64 natively, one toolchain; Qt has arm64 for MSVC only); `package.ps1` copies the VC runtime DLLs
  from the developer shell's `VCToolsRedistDir`. Tests need LF test data
  (`.gitattributes`).
- macOS: the app is "YAKBD Editor" because "YAKBD" and the CLI "yakbd" collide on a case-insensitive file system.
- Qt's deployment script brings only the xcb platform plugin on Linux; `packaging/linux/package.sh` adds Wayland and
  offscreen and removes the styles and test libraries the editor does not use.
