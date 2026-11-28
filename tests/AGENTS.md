# tests/

Qt Test (`tst_jsontree`, `tst_document`, `tst_cli`, `tst_window`), Qt Quick Test (`tst_qml` + `qml/tst_views.qml`)
and `screenshots.sh` + `pngstat` (light and dark shots must exist, not be blank and differ). Run all with
`ctest --test-dir build --output-on-failure`; one with `ctest --test-dir build -R tst_document --output-on-failure`.

- Test data is `data/*.kbl.json`, found through `YAKBD_TESTDATA`; it must stay LF (`.gitattributes`) because save
  tests compare bytes.
- `tst_cli` and the screenshots run the real `yakbd` from `YAKBD_CLI` (the pinned release on Linux x86-64); without
  one `tst_cli` skips (QSKIP), so a green run without a CLI proves less.
- Tests run `offscreen`, which reports no colour scheme: light/dark shots need xcb under Xvfb with
  `QT_QPA_PLATFORMTHEME=gtk3` (`screenshots.sh` does that). `tst_window` uses the native platform on Windows and
  macOS (⌘ shortcuts, appearance switch) and writes its results to a file, because Windows hides a GUI program's
  output from ctest.
- A behaviour that depends on the CLI is asserted against what the CLI itself reports, not against a copied fact.
