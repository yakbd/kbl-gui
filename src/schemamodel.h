// The bundled kbl JSON Schemas (one per format version), read in file order, turned into field lists for generated
// forms. The forms of a file use the schema of the file's `kbl` version; a member that version lacks (a target
// added by a later version) is described by the newest schema that has it.
#pragma once

#include "jsontree.h"

#include <QObject>
#include <QVariant>

namespace kbl {

class SchemaModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString schemaId READ schemaId NOTIFY versionChanged)
    // The format version whose schema the forms use first ("0.1", "0.2"); an unknown version uses the newest.
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(QStringList versions READ versions CONSTANT)

public:
    explicit SchemaModel(QObject *parent = nullptr);
    bool loadFile(const QString &path);

    QString schemaId() const;
    QString version() const { return m_version; }
    void setVersion(const QString &v);
    QStringList versions() const;

    // The fields of the object schema at a pointer into the schema (for example "/properties/metadata").
    // Each field: name, kind (string, boolean, integer, number, enum, enumSet, stringList, object, json),
    // description, required, pattern, minimum, maximum, options, schema (pointer of the field's schema).
    Q_INVOKABLE QVariantList fields(const QString &schemaPointer) const;
    // The schema pointer for a document pointer (follows properties, items and additionalProperties).
    Q_INVOKABLE QString schemaFor(const QString &documentPointer) const;
    Q_INVOKABLE QString describe(const QString &documentPointer) const;
    // The lowest bundled version whose schema has the document pointer ("" when none has it).
    Q_INVOKABLE QString versionFor(const QString &documentPointer) const;

signals:
    void versionChanged();

private:
    struct Schema {
        QString version;
        QString id;
        JPtr root;
    };
    const Schema *current() const;
    // The schema to read a schema pointer in: the current one when it has the pointer, else the newest that has it.
    const Schema *schemaWith(const QString &schemaPointer) const;
    static JPtr deref(const Schema &s, const JPtr &n);
    static QString schemaFor(const Schema &s, const QString &documentPointer);
    std::vector<Schema> m_schemas; // oldest first
    QString m_version;
};

} // namespace kbl
