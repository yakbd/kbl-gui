// The bundled kbl JSON Schema, read in file order, turned into field lists for generated forms.
#pragma once

#include "jsontree.h"

#include <QObject>
#include <QVariant>

namespace kbl {

class SchemaModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString schemaId READ schemaId CONSTANT)

public:
    explicit SchemaModel(QObject *parent = nullptr);
    bool loadFile(const QString &path);

    QString schemaId() const { return m_id; }

    // The fields of the object schema at a pointer into the schema (for example "/properties/metadata").
    // Each field: name, kind (string, boolean, integer, number, enum, enumSet, stringList, object, json),
    // description, required, pattern, minimum, maximum, options, schema (pointer of the field's schema).
    Q_INVOKABLE QVariantList fields(const QString &schemaPointer) const;
    // The schema pointer for a document pointer (follows properties, items and additionalProperties).
    Q_INVOKABLE QString schemaFor(const QString &documentPointer) const;
    Q_INVOKABLE QString describe(const QString &documentPointer) const;

private:
    JPtr deref(const JPtr &n) const;
    JPtr m_root;
    QString m_id;
};

} // namespace kbl
