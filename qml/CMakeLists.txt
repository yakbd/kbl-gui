set_source_files_properties(Ui.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(yakbdui STATIC)
qt_add_qml_module(yakbdui
  URI Yakbd
  VERSION 1.0
  QML_FILES
    Main.qml
    KeyboardPage.qml
    KeyboardView.qml
    KeyCap.qml
    KeyPanel.qml
    DeadKeysPage.qml
    SchemaForm.qml
    SchemaField.qml
    FormPage.qml
    JsonPage.qml
    ProblemsPanel.qml
    PreferencesDialog.qml
    AboutDialog.qml
    SectionHeader.qml
    SeverityMark.qml
    Ui.qml
  RESOURCES
    templates/new.kbl.json
    schema/0.1/kbl.schema.json
    schema/0.2/kbl.schema.json
    icons/yakbd.svg
)
target_link_libraries(yakbdui PRIVATE Qt6::Quick Qt6::QuickControls2)
