# YAKBD Editor

A desktop editor for keyboard layouts in [YAKBD](https://yakbd.github.io/)'s open `.kbl.json` format: see the layout
as a keyboard, layer by layer, change what each key types, edit dead keys and build choices, and build installable
layouts with the bundled [`yakbd`](https://github.com/yakbd/yakbd-cli) tool.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/images/editor-dark.png">
  <img alt="The editor showing a layout's AltGr layer as a keyboard, with the key panel and the problems panel" src="docs/images/editor-light.png">
</picture>

**Runs on** Windows (x64, ARM64), macOS (Apple silicon and Intel) and Linux (x86-64).
**Builds layouts for** Windows today; macOS and Linux (XKB, Wayland and X11) in the next release; Android, iOS,
ChromeOS and Keyman planned. Status: preview.

## Download

From the [latest release](https://github.com/yakbd/yakbd-gui/releases/latest):
[`yakbd-gui-setup.exe`](https://github.com/yakbd/yakbd-gui/releases/latest/download/yakbd-gui-setup.exe) for Windows,
the `.dmg` for macOS, the `.AppImage` for Linux. Each one carries the `yakbd` tool.

## Documentation

- [Installing the editor](https://yakbd.github.io/guide/#the-editor)
- [What the editor does](https://yakbd.github.io/editor/)
- [Building for Windows, macOS and Linux](https://yakbd.github.io/editor/targets)
- [Building the editor from source](https://yakbd.github.io/editor/building)
- [What is verified](https://yakbd.github.io/editor/verified)
- [What's new](https://yakbd.github.io/releases)

## Licence

MIT-0, see [LICENSE](LICENSE). Qt is used under the LGPL v3; third-party material is listed in [NOTICE](NOTICE).
[Contributing](https://yakbd.github.io/contributing) ·
[Code signing policy](https://yakbd.github.io/editor/code-signing-policy) (planned: free code signing provided by
[SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org)).
