# YAKBD Editor @VERSION@

The first preview of the desktop editor for `.kbl.json` keyboard layouts. It shows a layout as a keyboard, layer by
layer, edits keys, dead keys, metadata and the Windows build choices, and saves only what you changed. Checking,
converting (`.klc`, CLDR XML, layout DLLs) and building are done by the yakbd command-line tool, which ships with it:
**yakbd-cli [@CLI_TAG@](https://github.com/yakbd/yakbd-cli/releases/tag/@CLI_TAG@)**, unchanged.

## Windows (x64)

- **`yakbd-setup.exe`** is the yakbd tool's own installer with the editor added. It installs the editor and
  `yakbd.exe` into one folder, `C:\Program Files\YAKBD\yakbd\`, and puts that folder on the PATH. It is the same
  product as the CLI-only `yakbd-setup.exe`: either one replaces the other in place. **Installed apps** lists it as
  "yakbd" with the **CLI's version (@CLI_VERSION@)**, not the editor's. Unattended: `yakbd-setup.exe /quiet /norestart`;
  uninstall from Installed apps or with `yakbd-setup.exe /x /quiet`.
- **There is no Start-menu shortcut yet.** Start the editor as
  `C:\Program Files\YAKBD\yakbd\yakbd-gui.exe` (for example with Win+R), or pin it to the taskbar once it runs.
- The setup carries the **x64** build only. On ARM64 or 32-bit Windows it stops with exit code 1633 and changes
  nothing; the CLI-only setup of yakbd-cli covers those machines.
- **`yakbd-gui-@VERSION@-windows-x64.zip`**: the same files without installation; unpack and run `yakbd-gui.exe`.
- Not signed: Windows may warn (SmartScreen, "Unknown publisher"). Check the file as below first.

## macOS (Apple silicon and Intel, macOS 14.4 or later)

- **`yakbd-gui-@VERSION@-macos-universal.dmg`** holds **YAKBD Editor.app**, with the `yakbd` tool inside
  (`YAKBD Editor.app/Contents/MacOS/yakbd`).
- The app is **not signed or notarized**, so macOS refuses it the first time. Open it once, then go to
  **System Settings › Privacy & Security**, find the message about "YAKBD Editor" and click **Open Anyway** (within
  about an hour), and confirm with your password.
- Built and tested on GitHub's macOS runners only; nobody has used it by hand on a Mac yet (see VERIFIED.md).

## Linux (x86-64, glibc 2.35 or later)

- **`yakbd-gui-@VERSION@-linux-x86_64.AppImage`**: make it executable and run it (it needs FUSE 2, or run it with
  `--appimage-extract-and-run`).
- **`yakbd-gui-@VERSION@-linux-x86_64.tar.gz`**: unpack anywhere and run `bin/yakbd-gui`; `bin/yakbd` is the CLI.
- On X11, Qt needs the xcb utility libraries, which not every desktop installs (`libxcb-cursor0` is the one most
  often missing): `sudo apt install libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0
  libxcb-shape0 libxcb-xkb1 libxkbcommon-x11-0`, or on Fedora `sudo dnf install xcb-util-cursor xcb-util-wm
  xcb-util-image xcb-util-keysyms xcb-util-renderutil libxkbcommon-x11`. The README lists the other libraries.
- **`type2-runtime-20251108-source.tar.gz`** and **`fuse-3.15.0.tar.xz`**: the source of the AppImage runtime and
  of the libfuse (LGPL-2.1) built into it, as NOTICE says.

## Check a download

    sha256sum -c --ignore-missing SHA256SUMS
    gh attestation verify <file> -R yakbd/kbl-gui

Every file has a build provenance attestation from this repository's Packages workflow. Qt 6.12.1 is used under the
LGPL v3 (source URLs in [NOTICE](https://github.com/yakbd/kbl-gui/blob/main/NOTICE)); what was tested where is in
[VERIFIED.md](https://github.com/yakbd/kbl-gui/blob/main/VERIFIED.md).
