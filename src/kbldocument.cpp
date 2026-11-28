#include "kbldocument.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <functional>

namespace kbl {

class EditCommand : public QUndoCommand
{
public:
    EditCommand(KblDocument *doc, std::vector<Edit> edits, const QString &text)
        : QUndoCommand(text), m_doc(doc), m_edits(std::move(edits))
    {
    }
    void redo() override
    {
        for (const Edit &e : m_edits)
            m_doc->apply(e, true);
        m_doc->touched();
    }
    void undo() override
    {
        for (auto it = m_edits.rbegin(); it != m_edits.rend(); ++it)
            m_doc->apply(*it, false);
        m_doc->touched();
    }

private:
    KblDocument *m_doc;
    std::vector<Edit> m_edits;
};

KblDocument::KblDocument(QObject *parent) : QObject(parent)
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, &KblDocument::dirtyChanged);
    connect(&m_undo, &QUndoStack::canUndoChanged, this, &KblDocument::undoStateChanged);
    connect(&m_undo, &QUndoStack::canRedoChanged, this, &KblDocument::undoStateChanged);
    connect(&m_undo, &QUndoStack::undoTextChanged, this, &KblDocument::undoStateChanged);
    connect(&m_undo, &QUndoStack::redoTextChanged, this, &KblDocument::undoStateChanged);
}

QString KblDocument::displayName() const
{
    if (!loaded())
        return {};
    if (m_path.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_path).fileName();
}

QString KblDocument::fileNameForCli() const
{
    QString name = m_path.isEmpty() ? QStringLiteral("untitled.kbl.json") : QFileInfo(m_path).fileName();
    if (!name.endsWith(QLatin1String(".kbl.json")))
        name = QFileInfo(name).completeBaseName() + QStringLiteral(".kbl.json");
    return name;
}

void KblDocument::setError(const QString &e)
{
    m_error = e;
    emit lastErrorChanged();
}

void KblDocument::resetUndo()
{
    m_undo.clear();
    m_undo.setClean();
    emit undoStateChanged();
}

bool KblDocument::loadBytes(const QByteArray &bytes, const QString &path, bool untitled)
{
    ParseError err;
    JDocument d = parseDocument(bytes, &err);
    if (!d.root) {
        setError(tr("%1, line %2, column %3: %4")
                     .arg(QFileInfo(path).fileName())
                     .arg(err.line)
                     .arg(err.column)
                     .arg(err.message));
        return false;
    }
    if (d.root->type != JNode::Object) {
        setError(tr("%1: a .kbl.json file holds one JSON object").arg(QFileInfo(path).fileName()));
        return false;
    }
    const bool wasLoaded = loaded();
    m_doc = d;
    m_untitled = untitled;
    m_path = untitled ? QString() : path;
    m_forceDirty = untitled;
    resetUndo();
    setError({});
    ++m_revision;
    emit filePathChanged();
    if (!wasLoaded)
        emit loadedChanged();
    emit dirtyChanged();
    emit changed();
    return true;
}

bool KblDocument::open(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(tr("Cannot open %1: %2").arg(path, f.errorString()));
        return false;
    }
    return loadBytes(f.readAll(), QFileInfo(path).absoluteFilePath(), false);
}

bool KblDocument::saveAs(const QString &path)
{
    if (!loaded())
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        setError(tr("Cannot write %1: %2").arg(path, f.errorString()));
        return false;
    }
    f.write(bytes());
    if (!f.commit()) {
        setError(tr("Cannot write %1: %2").arg(path, f.errorString()));
        return false;
    }
    m_path = QFileInfo(path).absoluteFilePath();
    m_untitled = false;
    m_forceDirty = false;
    m_undo.setClean();
    setError({});
    emit filePathChanged();
    emit dirtyChanged();
    return true;
}

bool KblDocument::save()
{
    if (m_path.isEmpty()) {
        setError(tr("The document has no file name yet"));
        return false;
    }
    return saveAs(m_path);
}

void KblDocument::newDocument()
{
    QFile f(QStringLiteral(":/qt/qml/Yakbd/templates/new.kbl.json"));
    if (f.open(QIODevice::ReadOnly))
        loadBytes(f.readAll(), QString(), true);
}

void KblDocument::close()
{
    const bool wasLoaded = loaded();
    m_doc = JDocument();
    m_path.clear();
    m_forceDirty = false;
    resetUndo();
    ++m_revision;
    emit filePathChanged();
    if (wasLoaded)
        emit loadedChanged();
    emit dirtyChanged();
    emit changed();
}

void KblDocument::touched()
{
    ++m_revision;
    emit changed();
}

// ---------------------------------------------------------------------------------------------------------------
// Edits

static bool parseIndex(const QString &t, int *out)
{
    bool ok = false;
    const int v = t.toInt(&ok);
    if (!ok || v < 0 || (t.size() > 1 && t[0] == QLatin1Char('0')))
        return false;
    *out = v;
    return true;
}

bool KblDocument::planSet(const QString &pointer, const JPtr &value, Edit *out) const
{
    if (!loaded())
        return false;
    bool ok = false;
    QStringList tokens = kbl::splitPointer(pointer, &ok);
    if (!ok || tokens.isEmpty())
        return false;
    // Find the deepest existing container on the path.
    JPtr cur = m_doc.root;
    int depth = 0;
    for (; depth < tokens.size() - 1; ++depth) {
        JPtr next;
        if (cur->type == JNode::Object)
            next = cur->member(tokens[depth]);
        else if (cur->type == JNode::Array) {
            int i = 0;
            if (parseIndex(tokens[depth], &i) && i < int(cur->items.size()))
                next = cur->items[size_t(i)];
        }
        if (!next || !next->isContainer())
            break;
        cur = next;
    }
    // Wrap the value in fresh objects for the missing part of the path.
    JPtr v = value;
    for (int i = tokens.size() - 1; i > depth; --i) {
        JPtr o = JNode::makeObject();
        o->members.push_back(JMember{tokens[i], quoteString(tokens[i]), -1, v});
        v = o;
    }
    Edit e;
    e.parent = joinPointer(tokens.mid(0, depth));
    const QString key = tokens[depth];
    if (cur->type == JNode::Object) {
        e.key = key;
        const int at = cur->indexOf(key);
        e.index = at;
        e.before = at >= 0 ? cur->members[size_t(at)].value : JPtr();
        if (e.before && depth < tokens.size() - 1)
            return false; // a scalar is in the way
    } else if (cur->type == JNode::Array) {
        if (depth < tokens.size() - 1)
            return false;
        int i = 0;
        if (key == QLatin1String("-"))
            i = int(cur->items.size());
        else if (!parseIndex(key, &i) || i > int(cur->items.size()))
            return false;
        e.index = i;
        e.before = i < int(cur->items.size()) ? cur->items[size_t(i)] : JPtr();
    } else {
        return false;
    }
    e.after = v;
    *out = e;
    return true;
}

bool KblDocument::planRemove(const QString &pointer, Edit *out) const
{
    bool ok = false;
    QStringList tokens = kbl::splitPointer(pointer, &ok);
    if (!ok || tokens.isEmpty() || !node(pointer))
        return false;
    Edit e;
    e.parent = joinPointer(tokens.mid(0, tokens.size() - 1));
    JPtr parent = node(e.parent);
    const QString key = tokens.last();
    if (parent->type == JNode::Object) {
        e.key = key;
        e.index = parent->indexOf(key);
        e.before = parent->members[size_t(e.index)].value;
    } else {
        parseIndex(key, &e.index);
        e.before = parent->items[size_t(e.index)];
    }
    *out = e;
    return true;
}

bool KblDocument::apply(const Edit &e, bool forward)
{
    JPtr parent = node(e.parent);
    if (!parent || !parent->isContainer())
        return false;
    const JPtr from = forward ? e.before : e.after;
    const JPtr to = forward ? e.after : e.before;
    if (parent->type == JNode::Object) {
        if (!from) { // insert
            JMember m{e.key, quoteString(e.key), -1, to};
            // A member that existed in the file keeps its original key text and position.
            for (const auto &om : parent->origMembers)
                if (om.key == e.key && om.value == to)
                    m = om;
            const int at = e.index < 0 || e.index > int(parent->members.size()) ? int(parent->members.size())
                                                                                  : e.index;
            parent->members.insert(parent->members.begin() + at, m);
        } else {
            const int at = parent->indexOf(e.key);
            if (at < 0)
                return false;
            if (!to)
                parent->members.erase(parent->members.begin() + at);
            else
                parent->members[size_t(at)].value = to;
        }
    } else {
        if (!from)
            parent->items.insert(parent->items.begin() + std::min<size_t>(size_t(e.index), parent->items.size()), to);
        else if (!to)
            parent->items.erase(parent->items.begin() + e.index);
        else
            parent->items[size_t(e.index)] = to;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Format version (SPEC §1.1): a file is written as 0.2 only when it uses a 0.2-only member. An edit that adds the
// first one raises `kbl` (and a `$schema` naming the version's schema) in the same undo step; an edit that removes
// the last one lowers it again. Opening and saving never change the version.

static bool family02(const QString &kind)
{
    const QString fam = kind.section(QLatin1Char('.'), 0, 0);
    return fam == QLatin1String("macos") || fam == QLatin1String("macosDefault") || fam == QLatin1String("xkb")
           || fam == QLatin1String("xkbDefault");
}

static bool onLoss02(const JPtr &onLoss)
{
    JPtr kinds = onLoss ? onLoss->member(QStringLiteral("kinds")) : JPtr();
    if (!kinds || kinds->type != JNode::Object)
        return false;
    for (const auto &m : kinds->members)
        if (family02(m.key))
            return true;
    return false;
}

QString KblDocument::requiredVersion(const JPtr &root)
{
    if (!root || root->type != JNode::Object)
        return QStringLiteral("0.1");
    for (const QString &p : kVersion02Members)
        if (resolve(root, p))
            return QStringLiteral("0.2");
    if (JPtr f = resolve(root, QStringLiteral("/metadata/provenance/format"));
        f && f->type == JNode::String && (f->str == QLatin1String("keylayout") || f->str == QLatin1String("xkb")))
        return QStringLiteral("0.2");
    if (onLoss02(root->member(QStringLiteral("onLoss"))))
        return QStringLiteral("0.2");
    if (JPtr t = root->member(QStringLiteral("targets")); t && t->type == JNode::Object)
        for (const auto &m : t->members)
            if (m.value->type == JNode::Object && onLoss02(m.value->member(QStringLiteral("onLoss"))))
                return QStringLiteral("0.2");
    return QStringLiteral("0.1");
}

const QStringList KblDocument::kVersion02Members{QStringLiteral("/targets/macos"), QStringLiteral("/targets/xkb")};

QString KblDocument::formatVersion() const
{
    JPtr k = node(QStringLiteral("/kbl"));
    return k && k->type == JNode::String ? k->str : QString();
}

void KblDocument::addVersionEdits(std::vector<Edit> &edits) const
{
    const QString have = formatVersion();
    if (have != QLatin1String("0.1") && have != QLatin1String("0.2"))
        return; // a version this editor does not know: never guess
    for (const Edit &e : edits)
        if (e.parent.isEmpty() && (e.key == QLatin1String("kbl") || e.key == QLatin1String("$schema")))
            return; // the user edits the version itself
    const QString before = requiredVersion(m_doc.root);
    auto *self = const_cast<KblDocument *>(this);
    size_t applied = 0;
    for (; applied < edits.size(); ++applied)
        if (!self->apply(edits[applied], true))
            break;
    const QString after = requiredVersion(m_doc.root);
    for (size_t i = applied; i-- > 0;)
        self->apply(edits[i], false);
    QString want;
    if (have == QLatin1String("0.1") && after == QLatin1String("0.2"))
        want = after;
    else if (have == QLatin1String("0.2") && before == QLatin1String("0.2") && after == QLatin1String("0.1"))
        want = after;
    if (want.isEmpty())
        return;
    Edit k;
    k.key = QStringLiteral("kbl");
    k.index = m_doc.root->indexOf(k.key);
    k.before = node(QStringLiteral("/kbl"));
    k.after = JNode::makeString(want);
    edits.push_back(k);
    const QString oldId = QStringLiteral("/schema/%1/kbl.schema.json").arg(have);
    if (JPtr s = node(QStringLiteral("/$schema")); s && s->type == JNode::String && s->str.endsWith(oldId)) {
        Edit e;
        e.key = QStringLiteral("$schema");
        e.index = m_doc.root->indexOf(e.key);
        e.before = s;
        e.after = JNode::makeString(s->str.left(s->str.size() - oldId.size())
                                    + QStringLiteral("/schema/%1/kbl.schema.json").arg(want));
        edits.push_back(e);
    }
}

bool KblDocument::applyEdits(const std::vector<Edit> &edits, const QString &label)
{
    if (edits.empty())
        return false;
    std::vector<Edit> all = edits;
    addVersionEdits(all);
    m_undo.push(new EditCommand(this, all, label)); // push() runs redo()
    return true;
}

bool KblDocument::setNode(const QString &pointer, const JPtr &value, const QString &label)
{
    Edit e;
    if (!planSet(pointer, value, &e))
        return false;
    if (e.before && sameValue(*e.before, *value) && e.before->type == value->type)
        return false; // no change, no undo step
    return applyEdits({e}, label.isEmpty() ? tr("Edit %1").arg(pointer) : label);
}

// ---------------------------------------------------------------------------------------------------------------
// Generic access

QString KblDocument::typeAt(const QString &pointer) const
{
    JPtr n = node(pointer);
    if (!n)
        return {};
    switch (n->type) {
    case JNode::Null: return QStringLiteral("null");
    case JNode::Bool: return QStringLiteral("boolean");
    case JNode::Number: return QStringLiteral("number");
    case JNode::String: return QStringLiteral("string");
    case JNode::Array: return QStringLiteral("array");
    case JNode::Object: return QStringLiteral("object");
    }
    return {};
}

QVariant KblDocument::valueAt(const QString &pointer) const
{
    JPtr n = node(pointer);
    if (!n)
        return {};
    switch (n->type) {
    case JNode::Bool: return n->boolean;
    case JNode::Number: return n->number;
    case JNode::String: return n->str;
    default: return {};
    }
}

QStringList KblDocument::keysAt(const QString &pointer) const
{
    QStringList out;
    JPtr n = node(pointer);
    if (n && n->type == JNode::Object)
        for (const auto &m : n->members)
            out << m.key;
    return out;
}

int KblDocument::countAt(const QString &pointer) const
{
    JPtr n = node(pointer);
    return n ? n->childCount() : 0;
}

QString KblDocument::jsonAt(const QString &pointer) const
{
    JPtr n = node(pointer);
    if (!n)
        return {};
    return QString::fromUtf8(serializeValue(*n->deepCopy()));
}

QString KblDocument::setJsonAt(const QString &pointer, const QString &json, const QString &label)
{
    ParseError err;
    JPtr v = parseValue(json.toUtf8(), &err);
    if (!v)
        return tr("line %1, column %2: %3").arg(err.line).arg(err.column).arg(err.message);
    if (pointer.isEmpty())
        return tr("The whole document cannot be replaced here");
    if (!setNode(pointer, v, label))
        return exists(pointer) ? QString() : tr("Cannot set %1").arg(pointer);
    return {};
}

void KblDocument::setValueAt(const QString &pointer, const QVariant &value, const QString &label)
{
    JPtr v;
    switch (value.typeId()) {
    case QMetaType::Bool: v = JNode::makeBool(value.toBool()); break;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::Double: v = JNode::makeNumber(value.toDouble()); break;
    case QMetaType::QString: v = JNode::makeString(value.toString()); break;
    case QMetaType::QStringList:
    case QMetaType::QVariantList: {
        v = JNode::makeArray();
        for (const QVariant &item : value.toList())
            v->items.push_back(JNode::makeString(item.toString()));
        break;
    }
    default:
        if (!value.isValid()) {
            removeAt(pointer, label);
            return;
        }
        v = JNode::makeString(value.toString());
    }
    setNode(pointer, v, label);
}

void KblDocument::removeAt(const QString &pointer, const QString &label)
{
    Edit e;
    if (planRemove(pointer, &e))
        applyEdits({e}, label.isEmpty() ? tr("Remove %1").arg(pointer) : label);
}

// Adds what `other` has and this document lacks, deepest missing node first: values present in both stay.
int KblDocument::mergeMissing(const JPtr &other, const QString &label)
{
    if (!loaded() || !other)
        return 0;
    std::vector<Edit> edits;
    int added = 0;
    std::function<void(const JPtr &, const QString &)> walk = [&](const JPtr &o, const QString &ptr) {
        if (o->type != JNode::Object)
            return;
        for (const auto &m : o->members) {
            const QString child = ptr + QLatin1Char('/') + escapePointerToken(m.key);
            JPtr mine = node(child);
            if (!mine) {
                Edit e;
                if (planSet(child, m.value->deepCopy(), &e)) {
                    apply(e, true); // applied now so that later siblings see their parent
                    edits.push_back(e);
                    ++added;
                }
            } else if (mine->type == JNode::Object && m.value->type == JNode::Object) {
                walk(m.value, child);
            }
        }
    };
    walk(other, QString());
    if (edits.empty())
        return 0;
    // Roll back, then push as one command (push re-applies).
    for (auto it = edits.rbegin(); it != edits.rend(); ++it)
        apply(*it, false);
    applyEdits(edits, label);
    return added;
}

} // namespace kbl
