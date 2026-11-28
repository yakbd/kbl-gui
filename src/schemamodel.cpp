#include "schemamodel.h"

#include <QFile>

namespace kbl {

SchemaModel::SchemaModel(QObject *parent) : QObject(parent)
{
    loadFile(QStringLiteral(":/qt/qml/Yakbd/schema/0.1/kbl.schema.json"));
    loadFile(QStringLiteral(":/qt/qml/Yakbd/schema/0.2/kbl.schema.json"));
}

bool SchemaModel::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    ParseError e;
    Schema s;
    s.root = parseValue(f.readAll(), &e);
    if (!s.root)
        return false;
    if (JPtr id = s.root->member(QStringLiteral("$id")))
        s.id = id->str;
    if (JPtr kbl = resolve(s.root, QStringLiteral("/properties/kbl/const")))
        s.version = kbl->str;
    m_schemas.push_back(s);
    return true;
}

QStringList SchemaModel::versions() const
{
    QStringList out;
    for (const auto &s : m_schemas)
        out << s.version;
    return out;
}

void SchemaModel::setVersion(const QString &v)
{
    if (v == m_version)
        return;
    m_version = v;
    emit versionChanged();
}

const SchemaModel::Schema *SchemaModel::current() const
{
    if (m_schemas.empty())
        return nullptr;
    for (const auto &s : m_schemas)
        if (s.version == m_version)
            return &s;
    return &m_schemas.back();
}

QString SchemaModel::schemaId() const
{
    const Schema *s = current();
    return s ? s->id : QString();
}

const SchemaModel::Schema *SchemaModel::schemaWith(const QString &schemaPointer) const
{
    const Schema *cur = current();
    if (!cur || resolve(cur->root, schemaPointer))
        return cur;
    for (auto it = m_schemas.rbegin(); it != m_schemas.rend(); ++it)
        if (resolve(it->root, schemaPointer))
            return &*it;
    return cur;
}

JPtr SchemaModel::deref(const Schema &s, const JPtr &n)
{
    JPtr cur = n;
    for (int guard = 0; cur && guard < 8; ++guard) {
        JPtr ref = cur->member(QStringLiteral("$ref"));
        if (!ref || !ref->str.startsWith(QLatin1Char('#')))
            break;
        JPtr target = resolve(s.root, ref->str.mid(1));
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
    const Schema *s = schemaWith(schemaPointer);
    if (!s)
        return out;
    JPtr obj = deref(*s, resolve(s->root, schemaPointer));
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
        JPtr f = deref(*s, m.value);
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
            JPtr items = deref(*s, f->member(QStringLiteral("items")));
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

QString SchemaModel::schemaFor(const Schema &s, const QString &documentPointer)
{
    QString ptr;
    JPtr cur = s.root;
    for (const QString &t : kbl::splitPointer(documentPointer)) {
        cur = deref(s, cur);
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

QString SchemaModel::schemaFor(const QString &documentPointer) const
{
    const Schema *cur = current();
    if (!cur)
        return {};
    const QString p = schemaFor(*cur, documentPointer);
    if (!p.isEmpty() || documentPointer.isEmpty())
        return p;
    for (auto it = m_schemas.rbegin(); it != m_schemas.rend(); ++it)
        if (const QString q = schemaFor(*it, documentPointer); !q.isEmpty())
            return q;
    return {};
}

QString SchemaModel::versionFor(const QString &documentPointer) const
{
    for (const auto &s : m_schemas)
        if (documentPointer.isEmpty() || !schemaFor(s, documentPointer).isEmpty())
            return s.version;
    return {};
}

QString SchemaModel::describe(const QString &documentPointer) const
{
    const QString sp = schemaFor(documentPointer);
    if (sp.isEmpty() && !documentPointer.isEmpty())
        return {};
    const Schema *s = schemaWith(sp);
    return s ? str(deref(*s, resolve(s->root, sp)), "description") : QString();
}

} // namespace kbl
