# Building the editor from source

The editor is C++17 and QML on Qt 6. Qt 6.12.1 is pinned: `tools/getqt.cmake` downloads Qt's official open-source
archives (LGPL) from download.qt.io and checks each against the SHA-1 committed in `tools/qt-pins.cmake` (no Qt
account, no installer, no Python). `tools/update-qt-pins.cmake` regenerates the pins for a Qt update. CMake 3.25 or
later and a C++17 compiler are needed.

The tests run the released `yakbd` tool: on Linux x86-64 the configure step fetches the release pinned in
`tools/cli-pins.cmake` (checked against its `SHA256SUMS`); elsewhere pass `-DYAKBD_CLI=/path/to/yakbd`.

## Linux

The release build host is Ubuntu 22.04, so the binaries run on glibc 2.35 and later; `tools/docker/build.Dockerfile`
is that environment, the same as CI. `tools/linux-build-deps.sh` lists the packages (a C++ compiler, Ninja,
OpenGL/EGL and xkbcommon headers, the X11 and GTK runtime libraries, Xvfb for the screenshot test) and installs a
pinned CMake.

    cmake -DPLATFORM=linux_gcc_64 -DDEST=$HOME/qt-6.12.1 -P tools/getqt.cmake
    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/qt-6.12.1
    cmake --build build
    ctest --test-dir build --output-on-failure
    ./build/yakbd-gui
    sh packaging/linux/package.sh build build/cli/yakbd dist     # AppImage + runtime sources

## Windows

x64 or ARM64, natively, with Visual Studio 2022 or later and the C++ tools of that architecture:

    cmake -DPLATFORM=win64_msvc2022_64 -DDEST=C:/qt -P tools/getqt.cmake      # ARM64: win64_msvc2022_arm64
    # in a "Developer PowerShell for VS" of the same architecture:
    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/qt -DYAKBD_CLI=C:/path/to/yakbd.exe -DYAKBD_QT_SUBFOLDER=ON
    cmake --build build
    pwsh packaging/windows/package.ps1 -Build build -Arch x64 -Out dist

That gives `dist\payload-x64\`, the installed `YAKBD\Editor\` folder: `yakbd-gui.exe`, `qt.conf` and the licence
texts, with Qt and the Visual C++ runtime in `qt\`, a private side-by-side assembly the exe's manifest names (so
Windows finds it without PATH; `YAKBD_QT_SUBFOLDER=ON` adds that manifest, and such an exe starts only with `qt\`
beside it). The editor finds the CLI at `..\CLI\yakbd.exe`, beside itself, or on PATH.

`packaging/cli-release.sh <tag> cli` (Linux or macOS, with gh and Go) fetches and checks a yakbd-cli release and
rebuilds its Windows `yakbd.exe` from the tag; `packaging/windows/make-setup.sh` then turns the x64 and ARM64
payloads into `yakbd-gui-setup.exe` with the tag's own `tools/toolsetup` (`-editor`, `-editor-json`). Use the Go
version named in the tag's `installer/stub-386.version`: only then is every `yakbd.exe` byte-identical to the one
the release's `yakbd-cli-setup.exe` carries, which both scripts check.

## macOS

With the Xcode command-line tools:

    cmake -DPLATFORM=clang_64 -DDEST=$HOME/qt-6.12.1 -P tools/getqt.cmake
    cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/qt-6.12.1 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DYAKBD_CLI=/path/to/yakbd
    cmake --build build --parallel
    sh packaging/macos/package.sh build cli/macos/yakbd dist cli     # the release's universal yakbd (cli-release.sh)

Run `open "build/YAKBD Editor.app"`; the packaged app holds the CLI next to the editor's executable.

## Options for testing

`QT_QUICK_CONTROLS_STYLE` overrides the style (FluentWinUI3 on Windows, macOS on macOS, Fusion on Linux). The editor
takes these options:

| option | what it does |
|---|---|
| `--self-test FILE` | open, edit, save, reopen and validate without a window |
| `--grab out.png` | take a screenshot and quit |
| `--appearance light\|dark` | force the appearance |
| `--page NAME`, `--select POINTER` | open a page, select a key or field |
| `--validate` | validate at start |
| `--target NAME`, `--build DIR` | pick a target; build everything for it into a folder ([targets](targets.md)) |

Platform checks: `tools/distro-check.sh` (the AppImage on several distributions in containers),
`tools/desktop-check.sh` (KDE Plasma and Wayland), `tools/macos-desktop-check.sh` (the packaged app on a Mac) and
`tools/targets-check.sh` (each target end to end, one `summary.txt`). What ran where is in [VERIFIED.md](../VERIFIED.md).
