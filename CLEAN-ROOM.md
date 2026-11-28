# Clean-room record

The editor is a graphical shell for the `.kbl.json` format. It knows the format and nothing about Windows: every
Windows decision (virtual keys, scan-code flags, DLL tables, installers) is made by the `yakbd` command-line tool,
which the editor runs. This file records what the editor's authors read.

## Sources read

| source | what was used |
|---|---|
| [`yakbd/kbl-spec`](https://github.com/yakbd/kbl-spec) at `ba415d5` | SPEC.md (the format: bindings, layers, modifiers, dead keys, the Caps Lock fallback of §9.3, the loss report of §17, Appendix A position names and CLDR's implied forms), `schema/0.1/kbl.schema.json` (copied to `qml/schema/0.1/`, read at run time for the generated forms), `targets/windows.md` §1 (which field overrides which default rule, used only to point a `windowsDefault.*` report entry of yakbd 0.1.0-preview.1 at its field), the examples (copied to `tests/data/`) |
| [`yakbd/kbl-spec`](https://github.com/yakbd/kbl-spec) branch `spec-0.2` at `886856b` | SPEC.md §1.1 (format versions: which members need 0.2, how a tool raises and lowers `kbl` and `$schema`), `schema/0.2/kbl.schema.json` (copied to `qml/schema/0.2/` for the forms of `targets.macos` and `targets.xkb`), `targets/xkb.md` §5 (what the XKB build writes, for the wording of the page and menus only), the two 0.2 examples (copied to `tests/data/`) |
| [`yakbd/yakbd-cli`](https://github.com/yakbd/yakbd-cli) at `main` (before its 0.2 release) | `docs/targets.md` (the `-target` flag, and the exit code 2 for an unknown target or a command a target lacks, which the editor uses to ask which targets the tool has), `docs/macos.md`, the commands' `-h` output (the installer's default file name, read for its suffix) |
| [`yakbd/yakbd-cli`](https://github.com/yakbd/yakbd-cli) at `v0.1.0-preview.1` and `v0.1.0-preview.2` | README, `docs/report-file.md` (the report contract), `docs/installer-cli.md` §12 (the bundled installer), the commands' `-h` output, and the released binaries run as black boxes |
| Qt documentation (doc.qt.io, 6.12) and Qt's public source repository `qt/qtdeclarative` (one code search: the role of native menu items) | Qt Quick Controls, dialogs, styles, deployment, CMake API |
| GitHub releases of CMake, AppImage `appimagetool` and `type2-runtime`, libfuse 3.15.0 | build and packaging tools, pinned by SHA-256; the runtime's LICENSE and libfuse's LGPL-2.1 text copied to `third_party/licenses/`; the runtime's build script read to list the libraries it embeds |
| gnu.org, the Unicode licence | the licence texts in `third_party/licenses/`, unchanged |

## Not read

- The YAKBD reference specification (the clean specification of Windows DLLs, Windows behaviour, installation and
  `.klc` files), its MSKLC observations and its notes on public sources: **not read**. The editor needs none of it.
- No Windows, MSKLC or other keyboard-tool source code, decompiled or otherwise.

## Facts the editor holds

- The keyboard picture draws the file's own `forms` rows, or CLDR's implied `us` or `iso` form as SPEC Appendix A
  translates them, and adds the keys around that block (Tab, Shift, Space and so on) by their W3C `code` names.
  Key widths are ordinary presentation choices, not data.
- Which positions are shown as modifiers (Shift, Control, Alt, Meta, Caps Lock by W3C name, and the file's
  `customModifiers`) only changes how a key is drawn.
- The editor never writes a value it computed for a target: "Pin Windows Defaults" (and "Pin macOS Defaults", ...)
  runs `yakbd pin` for the picked target and copies what the tool wrote.
- For each target it holds only words: its name in the menus (Windows, macOS, Linux (XKB)) and the file type of
  its installer or archive in the Save dialog. Which targets and commands exist, and every build decision, come
  from the tool.
- The format version rule (SPEC §1.1) is a format fact: an edit that adds the first 0.2-only member raises `kbl`,
  one that removes the last lowers it.
