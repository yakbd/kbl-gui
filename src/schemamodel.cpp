#include "schemamodel.h"

#include <QFile>

namespace kbl {

SchemaModel::SchemaModel(QObject *parent) : QObject(parent)
{
    loadFile(QStringLiteral(":/qt/qml/Yakbd/schema/kbl.schema.json"));
}

bool SchemaModel::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    ParseError e;
    m_root = parseValue(f.readAll(), &e);
    if (!m_root)
        return false;
    if (JPtr id = m_root->member(QStringLiteral("$id")))
        m_id = id->str;
    return true;
}

JPtr SchemaModel::deref(const JPtr &n) const
{
    JPtr cur = n;
    for (int guard = 0; cur && guard < 8; ++guard) {
        JPtr ref = cur->member(QStringLiteral("$ref"));
        if (!ref || !ref->str.startsWith(QLatin1Char('#')))
            break;
        JPtr target = resolve(m_root, ref->str.mid(1));
        if (!target)
            break;
        // A $ref with siblings (description): merge, the siblings win.
        JPtr merged = target->deepCopy();
        for (const auto &m : cur->members)
            if (m.key != QLatin1String("$ref")) {
                const int at = merged->indexOf(m.key);
                if (at >= 0)
                    merged->members[size_t(at)].value = m.value;
                else
                    merged->members.push_back(m);
            }
        cur = merged;
    }
    return cur;
}

static QString str(const JPtr &n, const char *key)
{
    JPtr v = n ? n->member(QLatin1String(key)) : JPtr();
    return v && v->type == JNode::String ? v->str : QString();
}

QVariantList SchemaModel::fields(const QString &schemaPointer) const
{
    QVariantList out;
    JPtr obj = deref(resolve(m_root, schemaPointer));
    if (!obj)
        return out;
    JPtr props = obj->member(QStringLiteral("properties"));
    if (!props || props->type != JNode::Object)
        return out;
    QStringList required;
    if (JPtr r = obj->member(QStringLiteral("required")); r && r->type == JNode::Array)
        for (const auto &x : r->items)
            required << x->str;
    for (const auto &m : props->members) {
        const QString fieldPtr = schemaPointer + QStringLiteral("/properties/") + escapePointerToken(m.key);
        JPtr f = deref(m.value);
        QVariantMap field;
        field[QStringLiteral("name")] = m.key;
        field[QStringLiteral("schema")] = fieldPtr;
        field[QStringLiteral("description")] = str(f, "description");
        field[QStringLiteral("required")] = required.contains(m.key);
        field[QStringLiteral("pattern")] = str(f, "pattern");
        const QString type = str(f, "type");
        QString kind = QStringLiteral("json");
        JPtr en = f->member(QStringLiteral("enum"));
        if (en && en->type == JNode::Array) {
            QStringList opts;
            for (const auto &x : en->items)
                opts << x->str;
            field[QStringLiteral("options")] = opts;
            kind = QStringLiteral("enum");
        } else if (type == QLatin1String("string")) {
            kind = QStringLiteral("string");
        } else if (type == QLatin1String("boolean")) {
            kind = QStringLiteral("boolean");
        } else if (type == QLatin1String("integer") || type == QLatin1String("number")) {
            kind = type;
            if (JPtr mn = f->member(QStringLiteral("minimum")))
                field[QStringLiteral("minimum")] = mn->number;
            if (JPtr mx = f->member(QStringLiteral("maximum")))
                field[QStringLiteral("maximum")] = mx->number;
        } else if (type == QLatin1String("object") && f->member(QStringLiteral("properties"))) {
            kind = QStringLiteral("object");
        } else if (type == QLatin1String("array")) {
            JPtr items = deref(f->member(QStringLiteral("items")));
            JPtr ien = items ? items->member(QStringLiteral("enum")) : JPtr();
            if (ien && ien->type == JNode::Array) {
                QStringList opts;
                for (const auto &x : ien->items)
                    opts << x->str;
                field[QStringLiteral("options")] = opts;
                kind = QStringLiteral("enumSet");
            } else if (items && str(items, "type") == QLatin1String("string")) {
                kind = QStringLiteral("stringList");
            }
        }
        field[QStringLiteral("kind")] = kind;
        out << field;
    }
    return out;
}

QString SchemaModel::schemaFor(const QString &documentPointer) const
{
    QString ptr;
    JPtr cur = m_root;
    for (const QString &t : kbl::splitPointer(documentPointer)) {
        cur = deref(cur);
        if (!cur)
            return {};
        if (JPtr props = cur->member(QStringLiteral("properties")); props && props->member(t)) {
            ptr += QStringLiteral("/properties/") + escapePointerToken(t);
            cur = props->member(t);
        } else if (JPtr items = cur->member(QStringLiteral("items"))) {
            ptr += QStringLiteral("/items");
            cur = items;
        } else if (JPtr ap = cur->member(QStringLiteral("additionalProperties")); ap && ap->type == JNode::Object) {
            ptr += QStringLiteral("/additionalProperties");
            cur = ap;
        } else {
            return {};
        }
    }
    return ptr;
}

QString SchemaModel::describe(const QString &documentPointer) const
{
    const QString sp = schemaFor(documentPointer);
    if (sp.isEmpty() && !documentPointer.isEmpty())
        return {};
    return str(deref(resolve(m_root, sp)), "description");
}

} // namespace kbl
