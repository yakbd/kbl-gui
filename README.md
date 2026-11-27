# YAKBD Editor

A desktop editor for [`.kbl.json`](https://github.com/yakbd/kbl-spec) keyboard layouts, part of
[YAKBD](https://github.com/yakbd). It shows the layout as a keyboard, layer by layer, and lets you change what each
key types, edit dead keys, metadata and the Windows build choices. Checking, converting and building are done by the
[`yakbd` command-line tool](https://github.com/yakbd/yakbd-cli), which ships with the editor: the editor never
makes a Windows decision itself.

**Status: preview** (first release 0.1.0-preview.1). Built and tested on Linux, Windows and macOS (CI), and in a Windows 11 guest; what is
still open is in [VERIFIED.md](VERIFIED.md).

## What it does

- **Open** `.kbl.json` files; MSKLC `.klc` files, CLDR keyboard XML and Windows layout DLLs are converted by
  `yakbd convert` and open as a new, unsaved layout with the conversion report.
- **Save** writes your edits and nothing else: an untouched file saves byte for byte, and an edit changes only the
  lines of the edited value, in the file's own formatting.
- **Keyboard**: the file's positions on a keyboard picture, one layer at a time; dead keys, keys that fall back to
  the layer without Caps Lock, and explicit "nothing" are marked. Arrow keys move between keys, Enter edits.
- **Key panel**: what the key does in the current layer (text, code points, dead key, key reference, nothing, no
  binding), its scan code and name, and every layer at a glance.
- **Dead keys**: the composition tables, the character each dead key stands for and what happens without a match.
- **Metadata, Behaviour, Windows Target, Other Targets**: forms generated from the format's JSON Schema; an empty
  field leaves the choice to the format's default or the tool's default rule.
- **JSON**: any part of the file as JSON, for what the forms do not cover.
- **Problems panel**: `yakbd validate -windows` runs shortly after every change; problems, losses and the choices
  taken from default rules link to the field they concern. **Pin Windows Defaults** writes those choices into the
  file (one undo step). **Build Layout DLLs** and **Build Installer** run `yakbd compile` and `yakbd installer`.
- Undo/redo for every edit, recent files, unsaved-changes prompts, light and dark appearance (follows the system,
  or View › Appearance).

The editor looks for `yakbd` next to its own executable, then on the PATH; Preferences can name another copy. It
needs yakbd 0.1 or a later 0.x release.

## Install

Download from the [releases](https://github.com/yakbd/yakbd-gui/releases) of this repository (made by the `Packages`
workflow; each file has a `SHA256SUMS` line and a build provenance attestation: `gh attestation verify <file> -R
yakbd/yakbd-gui`).

- **Windows**: one `yakbd-setup.exe` for every Windows PC. It is the yakbd tool's own installer with the editor
  added: it installs `yakbd.exe` for the PC's architecture (x86, x64 or ARM64) and, on x64 and ARM64, the editor
  next to it, in `C:\Program Files\YAKBD\yakbd\`, puts that folder on the PATH, and replaces a CLI-only install in
  place (and the other way round). Qt 6 has no 32-bit Windows build, so on x86 it installs the CLI only. It takes
  `msiexec`-style switches (`/quiet /norestart`). Start the editor as `C:\Program Files\YAKBD\yakbd\yakbd-gui.exe`
  (a Start-menu shortcut comes with yakbd-cli 0.1.0-preview.3).
- **macOS** (Apple silicon and Intel, macOS 14.4 or later): `YAKBD-Editor-<version>.dmg` with `YAKBD Editor.app`, and `yakbd` inside the app.
  The app is **not signed or notarized**, so macOS blocks it the first time: open it once, then go to
  **System Settings › Privacy & Security**, find the message about YAKBD and click **Open Anyway** (within about an
  hour), and confirm. The command-line tool is at `YAKBD Editor.app/Contents/MacOS/yakbd`.
- **Linux** (x86-64, glibc 2.35 or later: Ubuntu 22.04+, Debian 12+, Fedora): `YAKBD-Editor-<version>-x86_64.AppImage`;
  make it executable and run it (the CLI is inside: `--appimage-extract` shows `usr/bin/yakbd`). It needs the libraries
  every desktop has: OpenGL/EGL, libxkbcommon, fontconfig, D-Bus, GLib and GSSAPI (Kerberos); on a minimal system
  `apt install libgl1 libegl1 libxkbcommon0 libfontconfig1 libdbus-1-3 libglib2.0-0 libgssapi-krb5-2`. On X11 Qt
  also needs the xcb utility libraries, which desktops do not all install (`libxcb-cursor0` is the one most often
  missing): `apt install libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0
  libxcb-shape0 libxcb-xkb1 libxkbcommon-x11-0`, or on Fedora `dnf install xcb-util-cursor xcb-util-wm
  xcb-util-image xcb-util-keysyms xcb-util-renderutil libxkbcommon-x11`. GTK 3 gives native file dialogs and the
  desktop's light/dark setting on GNOME. The AppImage needs FUSE 2, or run it with `--appimage-extract-and-run`.

## Build

Qt 6.12.1 is pinned: `tools/getqt.cmake` downloads Qt's official open-source archives (LGPL) from download.qt.io
and checks each against the SHA-1 committed in `tools/qt-pins.cmake` (no Qt account, no installer, no Python).
`tools/update-qt-pins.cmake` regenerates the pins for a Qt update. CMake 3.25 or later and a C++17 compiler are
needed.

**Linux** (the release build host is Ubuntu 22.04; `tools/docker/build.Dockerfile` is that environment, the same
as CI):
`tools/linux-build-deps.sh` lists the packages (a C++ compiler, Ninja, OpenGL/EGL and xkbcommon headers, the X11
and GTK runtime libraries, Xvfb for the screenshot test) and installs a pinned CMake.

    cmake -DPLATFORM=linux_gcc_64 -DDEST=$HOME/qt-6.12.1 -P tools/getqt.cmake
    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/qt-6.12.1
    cmake --build build
    ctest --test-dir build --output-on-failure
    ./build/yakbd-gui
    sh packaging/linux/package.sh build build/cli/yakbd dist     # AppImage + runtime sources

**Windows** (x64 or ARM64, natively; Visual Studio 2022 with the C++ tools of that architecture):

    cmake -DPLATFORM=win64_msvc2022_64 -DDEST=C:/qt -P tools/getqt.cmake      # ARM64: win64_msvc2022_arm64
    # in a "Developer PowerShell for VS" of the same architecture:
    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/qt -DYAKBD_CLI=C:/path/to/yakbd.exe
    cmake --build build
    pwsh packaging/windows/package.ps1 -Build build -Arch x64 -CliDocs <folder with yakbd-cli-LICENSE and -NOTICE> -Out dist

That gives `dist\payload-x64\` (the editor, Qt, the Visual C++ runtime and the licence texts); run its
`yakbd-gui.exe`. `packaging/windows/make-setup.sh` (Linux or macOS, with Go) turns the x64 and ARM64 payloads into
`yakbd-setup.exe` with a checkout of yakbd-cli at the release tag, whose `tools/toolsetup` is the installer
machinery. Use the Go version named in the checkout's `installer/stub-386.version`: only then is every `yakbd.exe` it
carries byte-identical to the release's, which the script checks against the release's zips.

**macOS** (Xcode command-line tools):

    cmake -DPLATFORM=clang_64 -DDEST=$HOME/qt-6.12.1 -P tools/getqt.cmake
    cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/qt-6.12.1 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DYAKBD_CLI=/path/to/yakbd
    cmake --build build --parallel
    sh packaging/macos/package.sh build yakbd-darwin-amd64 yakbd-darwin-arm64 dist

Run `open "build/YAKBD Editor.app"`; the packaged app holds the CLI next to the editor's executable.
The tests run the released CLI: on Linux x86-64 the configure step fetches the release pinned in
`tools/cli-pins.cmake` (checked against its `SHA256SUMS`); elsewhere pass `-DYAKBD_CLI`.

## Look and feel per platform

| | Windows | macOS | Linux |
|---|---|---|---|
| style | FluentWinUI3 | macOS (native controls) | Fusion, with the desktop's platform theme where it has one (GTK, portal) |
| menu bar | in the window | the system menu bar; About, Settings and Quit move to the app menu | in the window |
| file dialogs | native | native | GTK or portal dialogs where available, otherwise Qt's |
| light/dark | follows Windows | follows macOS | follows the platform theme; View › Appearance forces light or dark |

`QT_QUICK_CONTROLS_STYLE` overrides the style. Options for testing: `--self-test FILE` (open, edit, save, reopen
and validate without a window), `--grab out.png` (screenshot and quit), `--appearance light|dark`, `--page`,
`--select POINTER`, `--validate`.

## What is verified

[`VERIFIED.md`](VERIFIED.md) records what was checked where (Linux distributions, a Windows 11 guest without GPU,
the macOS runner), with the run ids, and what still needs a person or a platform that has not run yet.

## Licence

MIT-0 (MIT No Attribution), see [`LICENSE`](LICENSE). The editor uses Qt under the LGPL v3 (dynamically linked);
third-party material and its licences are listed in [`NOTICE`](NOTICE). Where the content came from:
[`CLEAN-ROOM.md`](CLEAN-ROOM.md).
