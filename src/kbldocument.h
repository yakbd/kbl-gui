// The open .kbl.json document: its JSON tree, the file on disk, undo/redo, and the views the editor needs.
// It knows the format (SPEC.md) only; every Windows decision is the CLI's.
#pragma once

#include "jsontree.h"

#include <QObject>
#include <QUndoStack>
#include <QVariant>

namespace kbl {

struct Edit {
    QString parent;      // pointer of the container
    QString key;         // object member name (object parents)
    int index = -1;      // array position, or the member position in an object (-1 = append)
    JPtr before;         // null: the edit inserts
    JPtr after;          // null: the edit removes
};

class KblDocument : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY filePathChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit KblDocument(QObject *parent = nullptr);

    QString filePath() const { return m_path; }
    QString displayName() const;
    bool loaded() const { return bool(m_doc.root); }
    bool dirty() const { return m_forceDirty || !m_undo.isClean(); }
    bool canUndo() const { return m_undo.canUndo(); }
    bool canRedo() const { return m_undo.canRedo(); }
    QString undoText() const { return m_undo.undoText(); }
    QString redoText() const { return m_undo.redoText(); }
    int revision() const { return m_revision; }
    QString lastError() const { return m_error; }
    QUndoStack *undoStack() { return &m_undo; }

    // Files
    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QString &path);
    Q_INVOKABLE void newDocument();
    Q_INVOKABLE void close();
    bool loadBytes(const QByteArray &bytes, const QString &path, bool untitled);
    QByteArray bytes() const { return serialize(m_doc); }
    // Name for a temporary copy handed to the CLI.
    Q_INVOKABLE QString fileNameForCli() const;

    // Undo
    Q_INVOKABLE void undo() { m_undo.undo(); }
    Q_INVOKABLE void redo() { m_undo.redo(); }

    // Generic access by JSON Pointer
    JPtr node(const QString &pointer) const { return resolve(m_doc.root, pointer); }
    Q_INVOKABLE bool exists(const QString &pointer) const { return bool(node(pointer)); }
    Q_INVOKABLE QString typeAt(const QString &pointer) const;
    Q_INVOKABLE QVariant valueAt(const QString &pointer) const;
    Q_INVOKABLE QStringList keysAt(const QString &pointer) const;
    Q_INVOKABLE int countAt(const QString &pointer) const;
    Q_INVOKABLE QString jsonAt(const QString &pointer) const;
    Q_INVOKABLE QString setJsonAt(const QString &pointer, const QString &json, const QString &label = {});
    Q_INVOKABLE void setValueAt(const QString &pointer, const QVariant &value, const QString &label = {});
    Q_INVOKABLE void removeAt(const QString &pointer, const QString &label = {});
    Q_INVOKABLE void navigate(const QString &pointer) { emit navigateRequested(pointer); }

    // Edits. Each call is one undo step.
    bool setNode(const QString &pointer, const JPtr &value, const QString &label);
    bool applyEdits(const std::vector<Edit> &edits, const QString &label);
    // Builds the edit that sets pointer to value, creating missing parents as objects.
    bool planSet(const QString &pointer, const JPtr &value, Edit *out) const;
    bool planRemove(const QString &pointer, Edit *out) const;

    // Layout views (format knowledge only)
    Q_INVOKABLE QVariantList layers() const;
    Q_INVOKABLE QVariantList keyboardRows(int layer) const;
    Q_INVOKABLE QVariantMap bindingAt(int layer, const QString &position) const;
    Q_INVOKABLE QVariantList bindingsForPosition(const QString &position) const;
    Q_INVOKABLE void setBinding(int layer, const QString &position, const QString &kind, const QString &value);
    Q_INVOKABLE QVariantMap positionInfo(const QString &position) const;
    Q_INVOKABLE QStringList deadKeyIds() const;
    Q_INVOKABLE QStringList keyIds() const;
    Q_INVOKABLE QVariantMap deadKeyInfo(const QString &id) const;
    Q_INVOKABLE QVariantList deadKeyEntries(const QString &id) const;
    Q_INVOKABLE void setDeadKeyEntry(const QString &id, int index, const QString &base, const QString &result);
    Q_INVOKABLE void addDeadKeyEntry(const QString &id, const QString &base, const QString &result);
    Q_INVOKABLE void removeDeadKeyEntry(const QString &id, int index);
    Q_INVOKABLE QString addDeadKey(const QString &id, const QString &spacing);
    Q_INVOKABLE void removeDeadKey(const QString &id);
    Q_INVOKABLE QString displayText(const QString &text) const;
    Q_INVOKABLE QString pointerFor(const QStringList &tokens) const { return joinPointer(tokens); }
    Q_INVOKABLE QStringList splitPointer(const QString &pointer) const { return kbl::splitPointer(pointer); }

    // Merges values that another version of the document has and this one lacks (the CLI's "pin" output) as
    // one undo step; returns the number of values added.
    int mergeMissing(const JPtr &other, const QString &label);

    // The file's `kbl` value, and the lowest format version that holds a document (SPEC §1.1: "0.2" when it uses a
    // 0.2-only member, else "0.1").
    Q_INVOKABLE QString formatVersion() const;
    static QString requiredVersion(const JPtr &root);
    static const QStringList kVersion02Members; // the 0.2-only objects (a test checks them against the schemas)

signals:
    void filePathChanged();
    void loadedChanged();
    void dirtyChanged();
    void undoStateChanged();
    void changed();
    void lastErrorChanged();
    void navigateRequested(const QString &pointer);

private:
    friend class EditCommand;
    bool apply(const Edit &e, bool forward);
    void addVersionEdits(std::vector<Edit> &edits) const;
    void touched();
    void setError(const QString &e);
    void resetUndo();
    QString bindingDisplay(const JPtr &binding, QString *kind) const;
    JPtr fallbackBinding(int layer, const QString &position) const;

    JDocument m_doc;
    QString m_path;
    bool m_untitled = true;
    bool m_forceDirty = false;
    QUndoStack m_undo;
    int m_revision = 0;
    QString m_error;
};

} // namespace kbl
