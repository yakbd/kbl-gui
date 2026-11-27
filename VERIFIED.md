# What is verified

Status on 2026-11-27: Qt 6.12.1, bundled yakbd CLI 0.1.0-preview.2 (0.1.0-preview.1 for the first Windows guest
run). The sections below were checked on 0.1.0 development builds in the development repository before publication
(date and runner image given). From the first release, 0.1.0-preview.1, runs of this repository's workflows are
cited by their id (`https://github.com/yakbd/yakbd-gui/actions/runs/<id>`).

## Release 0.1.0-preview.1 (history: that release and its files were removed when 0.1.0-preview.2 shipped)

| check | where | result |
|---|---|---|
| build and all tests on Linux (Ubuntu 22.04 container), Windows (`windows-2025`) and macOS (`macos-15`); each package's `--self-test` reports `YAKBD Editor 0.1.0-preview.1` and validates with the bundled yakbd 0.1.0-preview.2; Windows and macOS window grabs light and dark; `yakbd-setup.exe` carries the released `yakbd.exe` (SHA-256 compared) | Packages run [46003103416](https://github.com/yakbd/yakbd-gui/actions/runs/46003103416), 2026-11-27 | pass |
| the release files: `SHA256SUMS`, a build provenance attestation for every file (`gh attestation verify`), the editor's LICENSE, NOTICE and licence texts and the CLI's LICENSE and NOTICE in the zip, tarball, AppImage and `.dmg` (the setup's payload is the zip's folder) | the draft release, checked after download | pass |
| a rehearsal of the same workflow (`0.1.0-rehearsal.1`, draft and tag deleted afterwards) | Packages run [46002079436](https://github.com/yakbd/yakbd-gui/actions/runs/46002079436) | pass |

The repository was renamed from `yakbd/kbl-gui` to `yakbd/yakbd-gui` after this release; releases from
0.1.0-preview.2 on verify with `gh attestation verify <file> -R yakbd/yakbd-gui`.

From 0.1.0-preview.2 a release has fewer files: one `yakbd-setup.exe` for every Windows architecture (the editor for
x64 and ARM64, built with MSVC; the sections below used MinGW and x64 only), the `.dmg`, the AppImage and its runtime
sources. The Windows zip and the Linux tarball are gone; the distribution checks below ran the tarball, which held the
same files as the AppImage.

The release's `yakbd-setup.exe` in the Windows 11 guest (install, swap with the CLI-only setup, uninstall) is
checked before the release is published; its result is added here.

## After 0.1.0-preview.1 (development builds, 2026-11-27)

| check | where | result |
|---|---|---|
| Windows x64 and ARM64 built natively with MSVC (`windows-2025`, `windows-11-arm`), all tests on both, each payload's `--self-test` and FluentWinUI3 grabs light and dark | Packages run [46018326475](https://github.com/yakbd/yakbd-gui/actions/runs/46018326475) | pass |
| one `yakbd-setup.exe` made on Linux by the CLI tag's `toolsetup`: `yakbd.exe` for x86, x64 and ARM64, each byte-identical to the one in the 0.1.0-preview.2 release zips, and the editor for x64 and ARM64 (219 files, 208 MB) | same run | pass |
| that setup installed `/quiet` on an x64 and on an ARM64 runner: the files in Program Files, the installed editor's `--self-test` with the `yakbd.exe` beside it, uninstall `/x /quiet` removes the folder | same run | pass on both |
| macOS, the main window with the native platform (cocoa): ⌘Z / ⇧⌘Z undo and redo, the appearance switches at once (`tst_window`; it checks ⌘, for Settings from the next run on, where an empty key sequence no longer skips) | Packages run [46020289966](https://github.com/yakbd/yakbd-gui/actions/runs/46020289966), `macos-15` | pass |
| macOS, the packaged app driven by System Events (UI scripting was allowed on the runner): the menu bar is the system one (Apple, YAKBD, File, Edit, View, Layout, Help); the application menu holds About, Preferences… and Quit, moved there by Qt from Help, Edit and File | same run (`tools/macos-desktop-check.sh`) | pass (the menu then said "YAKBD"; the bundle name is "YAKBD Editor" since) |
| macOS, the system switched to dark while the app ran: the app followed without a restart | same run | pass |
| macOS, File › Open: the native open panel (NSOpenPanel, with Favorites, iCloud and tags), screenshot by `screencapture` | same run | pass |
| KDE Plasma on X11 (Fedora 43 container, Xvfb): the AppImage follows the Breeze Light and Breeze Dark colour schemes of `kdeglobals` (Qt's built-in KDE theme; the Plasma platform-theme plugin is built against the system's Qt and does not load into the AppImage's Qt) | `tools/desktop-check.sh`, rootless Docker | pass |
| KDE's own file dialog through the XDG desktop portal (`QT_QPA_PLATFORMTHEME=xdgdesktopportal`, xdg-desktop-portal-kde) | same | pass (a window of xdg-desktop-portal-kde; screenshot) |
| Wayland: weston (headless) and KWin (`--virtual`), `QT_QPA_PLATFORM=wayland`: the editor's window renders; on KWin it follows Breeze Light/Dark | same | pass |
| the AppImage on Ubuntu 22.04, 24.04, 26.04, Debian 12 and Fedora 43: self-test, offscreen window, X11 + GTK light and dark | `tools/distro-check.sh` | pass |

## Linux

| check | where | result |
|---|---|---|
| unit tests: byte-exact round trip of every spec example, one-line edits, undo/redo, JSON Pointers, parse errors with line and column | CI on push, Ubuntu 22.04 container (the release build host), every push since the first | pass |
| editor views and edits: layers, keyboard, bindings, Caps Lock fallback, dead keys, schema forms, pin merge, report pointers (format 1 and the 0.1.0-preview.1 shape) | same | pass |
| QML (Qt Quick Test, offscreen): arrow-key navigation, Enter edits, code points, generated form, dead keys, navigation from a report pointer | same | pass |
| with the released CLI: open, edit keys, save, reopen, `yakbd validate -windows` passes; import `.klc`, CLDR XML and a DLL; pin (then `-strict-defaults` passes); compile; installer | same, yakbd 0.1.0-preview.2 (and 0.1.0-preview.1 locally) | pass |
| screenshots light and dark (Fusion, GTK 3 platform theme, Xvfb), checked for contrast and light/dark | same; artifact `screenshots-linux` | pass |
| tarball and AppImage: `--self-test`, offscreen window, X11 + GTK light and dark windows from the unpacked tarball, the AppImage's self-test | Ubuntu 22.04, 24.04, 26.04, Debian 12, Fedora 43 in containers (`tools/distro-check.sh`) | pass |

The full test suite runs on Ubuntu 22.04 only; on the other distributions the packages were run, not the tests.

## Windows

| check | where | result |
|---|---|---|
| build (MinGW 15.1), all tests, zip, package `--self-test`, FluentWinUI3 window grabs light and dark | Packages workflow, `windows-2025` runner, 2026-11-27 (two runs: zip; zip and `yakbd-setup.exe`) | pass |
| `yakbd-setup.exe` carries the released `yakbd.exe` (byte-identical to the 0.1.0-preview.2 release zip's, checked in the workflow) | Packages workflow, `windows-2025`, 2026-11-27 | pass |
| the zip of the first of those runs in an offline Windows 11 x64 guest without GPU: self-test (open, edit, save, reopen, validate with the bundled CLI); default renderer works (Direct3D 11 on the Microsoft Basic Render Driver; no fallback needed); follows the Windows app light/dark setting in three views; the native File Open dialog | Hyper-V guest, scripted, run as the signed-in user | 10 passed, 0 failed |
| `yakbd-setup.exe` of the second run in the same guest: quiet install on a clean machine (editor and CLI in one folder, folder on the machine Path, 111 editor files recorded), `where yakbd` and `yakbd version` in a new program, the installed editor's self-test, its window light and dark (Direct3D 11, Basic Render Driver; 992x672, fits the 1024x720 work area), the CLI-only setup over it (editor files removed) and back (restored), self-test again, uninstall leaves no entry, folder or Path entry | Hyper-V guest, scripted | 8 passed, 3 failed: the three failures are a bug of the check script, not of the installer (Windows PowerShell 5.1 gives a single registry item no `.Count`, so "one Installed apps entry" printed an empty count; the same lines show one entry's version, and the uninstall check found none). The script is fixed; the run was not repeated |

## macOS

| check | where | result |
|---|---|---|
| universal build (x86_64 + arm64), all tests offscreen, `.dmg`, the CLI joined with `lipo` into a universal binary inside the app, ad-hoc signature, `--self-test` from the mounted `.dmg`, macOS-style window grabs light and dark | Packages workflow, `macos-15` runner, 2026-11-27 | pass |

An earlier run the same day found that the app's executable "YAKBD" and the CLI "yakbd" collide on the
case-insensitive file system; the app is "YAKBD Editor.app" since.

## Not verified yet

- **macOS, by a person**: the "Open Anyway" flow for the unsigned app; everyday use with a trackpad and the
  system's accessibility settings. The application menu, the native open panel, ⌘ shortcuts and the live
  light/dark switch ran on a GitHub macOS runner (above), not on a person's Mac.
- **Linux**: a real KDE Plasma or GNOME session (the checks above ran in containers: X11 under Xvfb, headless
  Wayland compositors, a portal started by hand); GNOME on Wayland; Flatpak; an ARM64 AppImage (none yet).
- **Windows**: the Start-menu shortcut (it comes with yakbd-cli 0.1.0-preview.3); live light/dark switching while
  the editor runs (each check started a new process); x86 Windows, where the setup installs the CLI only.
- **Accessibility** beyond keyboard navigation and accessible names (no screen-reader run).
