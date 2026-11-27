# YAKBD Editor @VERSION@

A preview of the desktop editor for `.kbl.json` keyboard layouts. It shows a layout as a keyboard, layer by layer,
edits keys, dead keys, metadata and the Windows build choices, and saves only what you changed. Checking, converting
(`.klc`, CLDR XML, layout DLLs) and building are done by the yakbd command-line tool, which ships with it:
**yakbd-cli @CLI_VERSION@** ([latest yakbd-cli release](https://github.com/yakbd/yakbd-cli/releases/latest)), unchanged.

## Windows

- **`yakbd-gui-setup.exe`**, one installer for every Windows PC, with the components **YAKBD Editor** (x64 and
  ARM64) and **YAKBD CLI** (yakbd @CLI_VERSION@ for x86, x64 and ARM64; the editor needs it), both ticked. It installs
  into `C:\Program Files\YAKBD\` (`Editor\` and `CLI\`) and adds the Start-menu shortcut **YAKBD Editor**. "Add
  YAKBD CLI to PATH" is offered, off by default (kept on when the PATH entry is there already). On **32-bit** Windows
  the editor is greyed out (Qt 6 has no 32-bit Windows build) and the CLI can be installed alone.
- One product with the CLI's `yakbd-cli-setup.exe`: **Installed apps** shows one entry, "YAKBD", with the editor's
  version (@VERSION@). Either installer sees what is installed, upgrades it and never downgrades. Uninstall from
  Installed apps: both components are ticked; untick the CLI to remove only the editor; "Remove my settings" is
  offered, unticked. Earlier installs (`C:\Program Files\YAKBD\yakbd\`) are taken over.
- Unattended: `yakbd-gui-setup.exe /quiet /norestart` (the wizard's defaults), with `ADDLOCAL=CLI`, `ADDTOPATH=1`,
  `REMOVE=Editor`, `REMOVEUSERDATA=1` as needed; uninstall with `yakbd-gui-setup.exe /x /quiet`.
- Not signed: Windows may warn (SmartScreen, "Unknown publisher"). Check the file as below first.

## macOS (Apple silicon and Intel, macOS 14.4 or later)

- **`YAKBD-Editor-@VERSION@.dmg`** holds **YAKBD Editor.app**, with the `yakbd` tool inside
  (`YAKBD Editor.app/Contents/MacOS/yakbd`).
- The app is **not signed or notarized**, so macOS refuses it the first time. Open it once, then go to
  **System Settings › Privacy & Security**, find the message about "YAKBD Editor" and click **Open Anyway** (within
  about an hour), and confirm with your password.
- Built and tested on GitHub's macOS runners only; nobody has used it by hand on a Mac yet (see VERIFIED.md).

## Linux (x86-64, glibc 2.35 or later)

- **`YAKBD-Editor-@VERSION@-x86_64.AppImage`**: make it executable and run it (it needs FUSE 2, or run it with
  `--appimage-extract-and-run`). The yakbd CLI is inside (`usr/bin/yakbd`). An ARM64 AppImage may follow later.
- On X11, Qt needs the xcb utility libraries, which not every desktop installs (`libxcb-cursor0` is the one most
  often missing): `sudo apt install libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0
  libxcb-shape0 libxcb-xkb1 libxkbcommon-x11-0`, or on Fedora `sudo dnf install xcb-util-cursor xcb-util-wm
  xcb-util-image xcb-util-keysyms xcb-util-renderutil libxkbcommon-x11`. The README lists the other libraries.
- **`appimage-runtime-sources-@VERSION@.tar.gz`**: the source of the AppImage runtime and of the libfuse (LGPL-2.1)
  built into it, as NOTICE says.

## Check a download

    sha256sum -c --ignore-missing SHA256SUMS
    gh attestation verify <file> -R yakbd/yakbd-gui

Every file has a build provenance attestation from this repository's Packages workflow. Qt 6.12.1 is used under the
LGPL v3 (source URLs in [NOTICE](https://github.com/yakbd/yakbd-gui/blob/main/NOTICE)); what was tested where is in
[VERIFIED.md](https://github.com/yakbd/yakbd-gui/blob/main/VERIFIED.md).

## Code signing policy

These files are not signed yet; signing through SignPath Foundation is planned (free code signing provided by
[SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org)). The policy:
[docs/code-signing-policy.md](https://github.com/yakbd/yakbd-gui/blob/main/docs/code-signing-policy.md).
