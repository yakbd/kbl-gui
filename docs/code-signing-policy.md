# Releases are not code-signed

No file of an editor release is code-signed: not `yakbd-gui-setup.exe` or the programs it installs (`yakbd-gui.exe`,
`yakbd.exe`), not the macOS `.dmg` (the app has only the ad-hoc signature arm64 code needs to run), not the Linux
AppImage. Like most software on GitHub, YAKBD has no free code-signing certificate available to it (the free
open-source signing programme it applied to found that it does not qualify), so signing is dropped until further
notice. The yakbd tool's page says the same for the tool's own releases
([yakbd-cli](https://github.com/yakbd/yakbd-cli/blob/main/docs/code-signing-policy.md)).

## How to check a download

Every release has a `SHA256SUMS` file and a build provenance attestation for each file, made by the Packages
workflow (`.github/workflows/release.yml`) on GitHub-hosted runners:

    sha256sum -c --ignore-missing SHA256SUMS                      # Linux; macOS: shasum -a 256 -c --ignore-missing SHA256SUMS
    (Get-FileHash .\yakbd-gui-setup.exe).Hash.ToLower()            # Windows: compare with the line in SHA256SUMS
    gh attestation verify yakbd-gui-setup.exe -R yakbd/yakbd-gui   # GitHub CLI

## What the operating system shows

- **Windows.** SmartScreen shows "Windows protected your PC": check the file name, then choose **More info** →
  **Run anyway**. The administrator prompt is yellow and names no verified publisher. Where **Smart App Control** is on
  (some Windows 11 PCs), it blocks unsigned installers and cannot make an exception for one program
  ([Windows security prompts](https://github.com/yakbd/yakbd-cli/blob/main/docs/windows-security-prompts.md)).
- **macOS.** Gatekeeper is expected to block the first start: open the app once, then click **Open Anyway** in
  **System Settings › Privacy & Security** (not yet checked by a person, [`VERIFIED.md`](../VERIFIED.md)).
- **Linux.** Nothing to click: make the AppImage executable and run it.

## Privacy

The editor's own code contains no network code: no Qt Network classes, sockets or HTTP requests, and it loads its
user interface only from its own resources. The only links it opens are those of its Help menu and About box, in
the user's web browser, when the user chooses them. Qt's network library is in the package because Qt's QML
engine links it; the editor does not use it. The yakbd tool it runs has no network code either.
