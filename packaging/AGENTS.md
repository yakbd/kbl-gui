# packaging/ and the release workflow

`linux/package.sh` (AppImage + runtime sources), `windows/package.ps1` + `windows/make-setup.sh`
(`yakbd-gui-setup.exe`), `macos/package.sh` (`.dmg`), `cli-release.sh` (fetch and check a yakbd-cli release),
`RELEASE-NOTES.md` (the template `.github/workflows/release.yml` fills for the draft release).

- A release is `release.yml` by `workflow_dispatch` with `version` and `cli` (an explicit yakbd-cli tag),
  `platforms=all`; it builds every package, attests them and makes a DRAFT release `v<version>` with
  `RELEASE-NOTES.md` as notes. A person publishes the draft. Then do the docs-after-release step in the root
  AGENTS.md (site re-pin and pages).
- Keep `RELEASE-NOTES.md` short: what it is, one line per download, the install link on the site, one line for
  checksums/attestation/NOTICE. Only `@VERSION@`, `@CLI_VERSION@`, `@CLI_TAG@` are replaced.
- Every package carries the CLI's LICENSE and NOTICE (`yakbd-cli-*`, from the CLI release's archives via
  `tools/fetch-cli.cmake` or `cli-release.sh`); a release with an AppImage must attach the AppImage runtime and
  libfuse source archive `linux/package.sh` writes.
- Linux: Qt's deployment brings only the xcb platform plugin; `linux/package.sh` adds Wayland and offscreen and
  removes the styles and test libraries the editor does not use.
- Windows: `package.ps1` moves Qt and the VC runtime into `qt\` (a private side-by-side assembly the exe's manifest
  names: `-DYAKBD_QT_SUBFOLDER=ON`; PowerShell variable names ignore case, so none may shadow `-Arch`). It copies
  the VC runtime DLLs from the developer shell's `VCToolsRedistDir`. Builds are MSVC, x64 and arm64 natively (Qt has
  arm64 for MSVC only).
- `yakbd-gui-setup.exe` must be built with the Go of the CLI tag's `installer/stub-386.version` and from an LF
  checkout, or the `yakbd.exe` it carries differs from the release's (`make-setup.sh` refuses that).
- macOS: the app is "YAKBD Editor" because "YAKBD" and the CLI "yakbd" collide on a case-insensitive file system.
  It is unsigned (ad-hoc signature only).
