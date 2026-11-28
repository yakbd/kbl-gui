# Targets: building for Windows, macOS and Linux

A **target** is a platform the editor checks and builds a layout for: **Windows** (layout DLLs and an installer),
**macOS** (a `.keylayout` file in a layout bundle, and a zip to hand out) and **Linux (XKB)** (XKB symbols and Compose
files for Wayland and X11, and an archive with an install script). The editor makes none of these decisions itself:
it runs the [`yakbd`](https://github.com/yakbd/yakbd-cli) tool with `-target` and shows what the tool reports.

## Picking the target

The target is picked in two places, which always agree:

- **Layout › Target**: Windows, macOS, Linux (XKB).
- The list beside **Validate** in the Problems panel.

The editor remembers the pick. It offers only the targets the `yakbd` tool next to it can build; a tool from before
targets (yakbd 0.1.0-preview.3 and earlier) builds for Windows only, and the editor then works as it always did.

## What follows the target

| | Windows | macOS | Linux (XKB) |
|---|---|---|---|
| **Validate** (F7, and shortly after every change) | `yakbd validate -windows` | `yakbd validate -target macos` | `yakbd validate -target xkb` |
| **Layout › Pin … Defaults** | Pin Windows Defaults | Pin macOS Defaults | Pin Linux (XKB) Defaults |
| **Layout › Build …** (Ctrl+B): into a folder | Build Layout DLLs… | Build macOS Layout… (the `.bundle`) | Build Linux (XKB) Layout… (the XKB files) |
| **Layout › Build …** (Ctrl+Shift+B): one file | Build Installer… (`.exe`) | Build macOS Archive… (`.zip`) | Build Linux (XKB) Archive… (the release archive) |
| form page | Windows Target | macOS Target | Linux Target |

The **Problems panel** shows the report of the last check or build: problems, the target's losses (`windows.*`,
`macos.*`, `xkb.*` and the general ones) and every choice taken from a default rule (`windowsDefault.*`,
`macosDefault.*`, `xkbDefault.*`). Entries that point into the file open the field they concern. After the target
changes, the panel says "changed since the last check" until the new check is done.

A command a target does not have is greyed out. The file type in the Save dialog of Build … Archive comes from the
tool too (the default file name of its `installer -o`).

## Format versions

`targets.macos` and `targets.xkb` are new in format version 0.2 (the kbl specification, §1.1). The editor reads 0.1
and 0.2 files and writes the lowest version that holds the file, as the `yakbd` tool does:

- Opening and saving never change the version: a file you did not edit is saved byte for byte.
- The forms of a file use the schema of its version. The **macOS Target** and **Linux Target** pages of a 0.1 file
  say that they are new in 0.2: the first value you set there changes `kbl` (and a `$schema` that names the 0.1
  schema) to 0.2, in the same undo step as your edit. Pinning macOS or Linux defaults into a 0.1 file does the same.
- An edit that removes the last 0.2-only part (the `targets.macos` or `targets.xkb` object, a `macos.*` or `xkb.*`
  loss policy) makes the file 0.1 again. A 0.2 file that has none of them stays 0.2 until such an edit.

## For checks without a person

- `--target NAME` picks the target for this run.
- `--build DIR` opens the layout, waits for the first check, builds into `DIR/<target>/`, builds the installer or
  archive there, saves the unedited layout there (it must be byte-identical to the opened file), prints one `check`
  line per step (`check <step> <target> PASS|FAIL ...`, with the report's counts) and quits.
- `--self-test --target NAME FILE` validates the edited copy for that target.
- `tools/targets-check.sh <editor> <out> [layouts]` runs both for each layout and target, compares every result
  with what `yakbd` itself says about the layout, and writes `<out>/summary.txt` (one PASS/FAIL line per case).
  The **Targets check** workflow (`.github/workflows/targets-check.yml`, started by hand) runs it on Linux
  (Ubuntu 22.04 under Xvfb) and on a macOS runner, with a `yakbd` built from a given yakbd-cli branch.
  `TARGETS_CLI=<yakbd> tools/desktop-check.sh dist` runs it on KDE Plasma (X11) with the AppImage, and
  `tools/macos-desktop-check.sh` with `YAKBD_CLI` set checks the Layout › Target menu by UI scripting.
