# qml/: the QML module `Yakbd`

Static plugin `yakbduiplugin`, linked into the app and `tests/tst_qml`. `Main.qml` holds the actions, menus and
dialogs; `KeyboardPage`, `DeadKeysPage`, `JsonPage` and one `FormPage` per form page;
`SchemaForm`/`SchemaField` build forms from `schema/<version>/`; `Ui.qml` is the singleton of the colours the editor draws itself; `templates/new.kbl.json` is File › New.

## Gotchas

- `set_source_files_properties(Ui.qml ... QT_QML_SINGLETON_TYPE TRUE)` must come before `qt_add_qml_module`, or
  `Ui.*` is undefined at run time.
- `layer` is a FINAL property of every Item: use `layerIndex`.
- Bindings on document data must depend on `doc.revision`: `(doc.revision, doc.valueAt(p))`, or they never update.
- A component that loads itself (SchemaForm via SchemaField) must get its pointer before it creates children
  (`Loader.setSource(url, props)`), or it recurses over the whole schema.
- `schema/<version>/kbl.schema.json` are copies of the spec's generated schemas; replace them from `yakbd/kbl-spec`,
  never edit them by hand.
- User-visible text names menus and pages as they appear (`Layout › Target`); the site's editor pages and
  `docs/targets.md` quote them, so a rename updates those too.
