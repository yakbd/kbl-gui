# What is verified

Status on 2026-11-27: Qt 6.12.1, bundled yakbd CLI 0.1.0-preview.2 (0.1.0-preview.1 for the first Windows guest
run). The sections below were checked on 0.1.0 development builds in the development repository before publication
(date and runner image given). From the first release, 0.1.0-preview.1, runs of this repository's workflows are
cited by their id (`https://github.com/yakbd/yakbd-gui/actions/runs/<id>`).

## Release 0.1.0-preview.1

| check | where | result |
|---|---|---|
| build and all tests on Linux (Ubuntu 22.04 container), Windows (`windows-2025`) and macOS (`macos-15`); each package's `--self-test` reports `YAKBD Editor 0.1.0-preview.1` and validates with the bundled yakbd 0.1.0-preview.2; Windows and macOS window grabs light and dark; `yakbd-setup.exe` carries the released `yakbd.exe` (SHA-256 compared) | Packages run [46003103416](https://github.com/yakbd/yakbd-gui/actions/runs/46003103416), 2026-11-27 | pass |
| the release files: `SHA256SUMS`, a build provenance attestation for every file (`gh attestation verify`), the editor's LICENSE, NOTICE and licence texts and the CLI's LICENSE and NOTICE in the zip, tarball, AppImage and `.dmg` (the setup's payload is the zip's folder) | the draft release, checked after download | pass |
| a rehearsal of the same workflow (`0.1.0-rehearsal.1`, draft and tag deleted afterwards) | Packages run [46002079436](https://github.com/yakbd/yakbd-gui/actions/runs/46002079436) | pass |

The repository was renamed from `yakbd/kbl-gui` to `yakbd/yakbd-gui` after this release. Its attestations name the old
repository, so they verify with `gh attestation verify <file> --owner yakbd` or `-R yakbd/kbl-gui`, not with
`-R yakbd/yakbd-gui`. Releases from 0.1.0-preview.2 on verify with `-R yakbd/yakbd-gui`.

From 0.1.0-preview.2 a release has fewer files: one `yakbd-setup.exe` for every Windows architecture (the editor for
x64 and ARM64, built with MSVC; the sections below used MinGW and x64 only), the `.dmg`, the AppImage and its runtime
sources. The Windows zip and the Linux tarball are gone; the distribution checks below ran the tarball, which held the
same files as the AppImage.

The release's `yakbd-setup.exe` in the Windows 11 guest (install, swap with the CLI-only setup, uninstall) is
checked before the release is published; its result is added here.

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

- **macOS, by a person**: that About, Settings and Quit appear in the application menu (Qt places them by their
  titles); the native Open and Save dialogs; the "Open Anyway" flow for the unsigned app; switching light/dark
  while the app runs; ⌘ shortcuts.
- **Linux desktops** other than GNOME-like GTK under X11 (Xvfb): KDE Plasma, a Wayland session (the Wayland plugin is
  packaged but has not run), the XDG portal file dialogs, Flatpak.
- **Windows**: ARM64 (no editor build yet); live light/dark switching while the editor runs (each check started a
  new process); a Start-menu shortcut does not exist (the CLI's installer has no facts for one yet).
- **Accessibility** beyond keyboard navigation and accessible names (no screen-reader run).
