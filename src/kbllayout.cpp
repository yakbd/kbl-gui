// The editor's views of a layout: layers, the keyboard picture, bindings and dead keys. Everything here follows
// SPEC.md (the .kbl.json format); the picture's geometry is presentation only.
#include "kbldocument.h"

#include <QCoreApplication>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QTextLayout>
#include <QSet>

#include <algorithm>

namespace kbl {

namespace {

const QStringList kLetterRows[] = {
    {QStringLiteral("KeyQ"), QStringLiteral("KeyW"), QStringLiteral("KeyE"), QStringLiteral("KeyR"),
     QStringLiteral("KeyT"), QStringLiteral("KeyY"), QStringLiteral("KeyU"), QStringLiteral("KeyI"),
     QStringLiteral("KeyO"), QStringLiteral("KeyP")},
    {QStringLiteral("KeyA"), QStringLiteral("KeyS"), QStringLiteral("KeyD"), QStringLiteral("KeyF"),
     QStringLiteral("KeyG"), QStringLiteral("KeyH"), QStringLiteral("KeyJ"), QStringLiteral("KeyK"),
     QStringLiteral("KeyL")},
    {QStringLiteral("KeyZ"), QStringLiteral("KeyX"), QStringLiteral("KeyC"), QStringLiteral("KeyV"),
     QStringLiteral("KeyB"), QStringLiteral("KeyN"), QStringLiteral("KeyM")},
};

QStringList digitRow()
{
    QStringList r{QStringLiteral("Backquote")};
    for (int i = 1; i <= 9; ++i)
        r << QStringLiteral("Digit%1").arg(i);
    r << QStringLiteral("Digit0") << QStringLiteral("Minus") << QStringLiteral("Equal");
    return r;
}

// CLDR's implied forms as SPEC Appendix A translates them: used to draw a file that names no form.
QList<QStringList> impliedForm(const QString &id)
{
    const QStringList p{QStringLiteral("BracketLeft"), QStringLiteral("BracketRight")};
    const QStringList sq{QStringLiteral("Semicolon"), QStringLiteral("Quote")};
    const QStringList cps{QStringLiteral("Comma"), QStringLiteral("Period"), QStringLiteral("Slash")};
    const QStringList space{QStringLiteral("Space")};
    if (id == QLatin1String("iso"))
        return {digitRow(), kLetterRows[0] + p, kLetterRows[1] + sq + QStringList{QStringLiteral("Backslash")},
                QStringList{QStringLiteral("IntlBackslash")} + kLetterRows[2] + cps, space};
    return {digitRow(), kLetterRows[0] + p + QStringList{QStringLiteral("Backslash")}, kLetterRows[1] + sq,
            kLetterRows[2] + cps, space};
}

struct Frame {
    QString id;
    double width;
};

double widthOf(const QString &id)
{
    static const QHash<QString, double> w{
        {QStringLiteral("Backspace"), 2.0},   {QStringLiteral("Tab"), 1.5},
        {QStringLiteral("CapsLock"), 1.75},   {QStringLiteral("Enter"), 2.25},
        {QStringLiteral("ShiftLeft"), 2.25},  {QStringLiteral("ShiftRight"), 2.75},
        {QStringLiteral("ControlLeft"), 1.25}, {QStringLiteral("MetaLeft"), 1.25},
        {QStringLiteral("AltLeft"), 1.25},    {QStringLiteral("AltRight"), 1.25},
        {QStringLiteral("MetaRight"), 1.25},  {QStringLiteral("ContextMenu"), 1.25},
        {QStringLiteral("ControlRight"), 1.25}, {QStringLiteral("Space"), 6.25},
    };
    return w.value(id, 1.0);
}

const QSet<QString> &modifierPositions()
{
    static const QSet<QString> s{QStringLiteral("ShiftLeft"),  QStringLiteral("ShiftRight"),
                                 QStringLiteral("ControlLeft"), QStringLiteral("ControlRight"),
                                 QStringLiteral("AltLeft"),    QStringLiteral("AltRight"),
                                 QStringLiteral("MetaLeft"),   QStringLiteral("MetaRight"),
                                 QStringLiteral("CapsLock")};
    return s;
}

QString capitalised(const QString &s)
{
    return s.isEmpty() ? s : s.left(1).toUpper() + s.mid(1);
}

QString setLabel(const JPtr &set)
{
    if (!set || set->type != JNode::Array)
        return QStringLiteral("?");
    if (set->items.empty())
        return QObject::tr("Base");
    QStringList parts;
    for (const auto &c : set->items)
        parts << capitalised(c->str);
    return parts.join(QLatin1Char('+'));
}

QStringList setComponents(const JPtr &set)
{
    QStringList out;
    if (set && set->type == JNode::Array)
        for (const auto &c : set->items)
            out << c->str;
    out.sort();
    return out;
}

} // namespace

// Whether the system's fonts (with fallback) can draw every character of a key label. A label with a character
// no installed font has would show an empty box, so the key shows its code points instead.
static bool renderable(const QString &text)
{
    if (text.isEmpty() || !qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return true;
    static QHash<QString, bool> cache;
    const auto it = cache.constFind(text);
    if (it != cache.constEnd())
        return *it;
    QTextLayout layout(text, QGuiApplication::font());
    layout.beginLayout();
    layout.createLine();
    layout.endLayout();
    bool ok = true;
    for (const QGlyphRun &run : layout.glyphRuns())
        for (quint32 g : run.glyphIndexes())
            if (g == 0)
                ok = false;
    cache.insert(text, ok);
    return ok;
}

static QString codePoints(const QString &text)
{
    QStringList out;
    for (char32_t c : text.toUcs4())
        out << QStringLiteral("U+") + QString::number(uint(c), 16).toUpper().rightJustified(4, QLatin1Char('0'));
    return out.join(QLatin1Char(' '));
}

QString KblDocument::displayText(const QString &text) const
{
    if (text.isEmpty())
        return {};
    static const QHash<QString, QString> names{
        {QStringLiteral("\x1b"), QStringLiteral("Esc")}, {QStringLiteral("\b"), QStringLiteral("⌫")},
        {QStringLiteral("\r"), QStringLiteral("↵")},     {QStringLiteral("\n"), QStringLiteral("LF")},
        {QStringLiteral("\t"), QStringLiteral("⇥")},     {QStringLiteral(" "), QStringLiteral("␣")},
        {QString(QChar(0x00A0)), QStringLiteral("NBSP")}, {QString(QChar(0x200D)), QStringLiteral("ZWJ")},
        {QString(QChar(0x200C)), QStringLiteral("ZWNJ")}, {QString(QChar(0x200B)), QStringLiteral("ZWSP")},
    };
    if (names.contains(text))
        return names.value(text);
    QString out;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const char32_t c = text[i].isHighSurrogate() && i + 1 < text.size()
                               ? QChar::surrogateToUcs4(text[i], text[i + 1])
                               : char32_t(text[i].unicode());
        const int len = c > 0xFFFF ? 2 : 1;
        if (c < 0x20 || c == 0x7F)
            out += QStringLiteral("^%1").arg(QChar(char16_t(c ^ 0x40)));
        else if (QChar::category(c) == QChar::Mark_NonSpacing && out.isEmpty())
            out += QChar(0x25CC) + text.mid(i, len); // a lone combining mark stands on a dotted circle
        else
            out += text.mid(i, len);
        i += len - 1;
    }
    return out;
}

QVariantList KblDocument::layers() const
{
    QVariantList out;
    JPtr ls = node(QStringLiteral("/hardware/layers"));
    if (!ls || ls->type != JNode::Array)
        return out;
    for (size_t i = 0; i < ls->items.size(); ++i) {
        const JPtr &l = ls->items[i];
        QStringList sets;
        if (JPtr mods = l->member(QStringLiteral("modifiers")); mods && mods->type == JNode::Array)
            for (const auto &s : mods->items)
                sets << setLabel(s);
        JPtr id = l->member(QStringLiteral("id"));
        QVariantMap m;
        m[QStringLiteral("index")] = int(i);
        m[QStringLiteral("id")] = id && id->type == JNode::String ? id->str : QString();
        m[QStringLiteral("modifiers")] = sets.join(QStringLiteral(", "));
        m[QStringLiteral("label")] = sets.isEmpty() ? QStringLiteral("?") : sets.join(QStringLiteral(" · "));
        JPtr b = l->member(QStringLiteral("bindings"));
        m[QStringLiteral("count")] = b ? b->childCount() : 0;
        out << m;
    }
    return out;
}

QString KblDocument::bindingDisplay(const JPtr &b, QString *kind) const
{
    if (!b) {
        *kind = QStringLiteral("unbound");
        return {};
    }
    if (b->type == JNode::String) {
        *kind = b->str == QStringLiteral("\b") ? QStringLiteral("action") : QStringLiteral("text");
        return displayText(b->str);
    }
    if (b->type != JNode::Object) {
        *kind = QStringLiteral("invalid");
        return QStringLiteral("?");
    }
    if (JPtr o = b->member(QStringLiteral("output"))) {
        if (o->type == JNode::String) {
            *kind = o->str.isEmpty() ? QStringLiteral("nothing") : QStringLiteral("text");
            return displayText(o->str);
        }
        *kind = QStringLiteral("marked");
        QString t;
        for (const auto &part : o->items)
            if (part->type == JNode::String)
                t += part->str;
        return displayText(t) + QStringLiteral("•");
    }
    if (JPtr d = b->member(QStringLiteral("deadKey"))) {
        *kind = QStringLiteral("deadKey");
        JPtr dk = node(QStringLiteral("/deadKeys/") + escapePointerToken(d->str));
        if (dk) {
            if (JPtr disp = dk->member(QStringLiteral("display")); disp && disp->type == JNode::String)
                return displayText(disp->str);
            if (JPtr sp = dk->member(QStringLiteral("spacing")); sp && sp->type == JNode::String)
                return displayText(sp->str);
        }
        return d->str;
    }
    if (JPtr k = b->member(QStringLiteral("key"))) {
        *kind = QStringLiteral("key");
        JPtr key = node(QStringLiteral("/keys/") + escapePointerToken(k->str));
        if (key)
            if (JPtr o = key->member(QStringLiteral("output")); o && o->type == JNode::String)
                return displayText(o->str);
        return k->str.size() == 1 ? k->str : QStringLiteral("⟨%1⟩").arg(k->str);
    }
    if (JPtr u = b->member(QStringLiteral("utf16")); u && u->type == JNode::Array) {
        *kind = QStringLiteral("codeUnits");
        QStringList units;
        for (const auto &x : u->items)
            units << x->str.toUpper();
        return QStringLiteral("U+") + units.join(QStringLiteral(" "));
    }
    *kind = QStringLiteral("invalid");
    return QStringLiteral("?");
}

// What a key gives through Caps Lock fallback (SPEC §9.3) when the layer itself has no binding.
JPtr KblDocument::fallbackBinding(int layer, const QString &position) const
{
    JPtr fb = node(QStringLiteral("/settings/capsLockFallback"));
    if (fb && fb->type == JNode::Bool && !fb->boolean)
        return {};
    JPtr ls = node(QStringLiteral("/hardware/layers"));
    if (!ls || layer < 0 || layer >= int(ls->items.size()))
        return {};
    JPtr mods = ls->items[size_t(layer)]->member(QStringLiteral("modifiers"));
    if (!mods || mods->type != JNode::Array)
        return {};
    for (const auto &set : mods->items) {
        QStringList comps = setComponents(set);
        if (!comps.removeOne(QStringLiteral("caps")))
            continue;
        for (const auto &other : ls->items) {
            JPtr om = other->member(QStringLiteral("modifiers"));
            if (!om || om->type != JNode::Array)
                continue;
            for (const auto &os : om->items)
                if (setComponents(os) == comps) {
                    JPtr b = other->member(QStringLiteral("bindings"));
                    return b ? b->member(position) : JPtr();
                }
        }
    }
    return {};
}

QVariantMap KblDocument::bindingAt(int layer, const QString &position) const
{
    QVariantMap m;
    const QString ptr = QStringLiteral("/hardware/layers/%1/bindings/%2").arg(layer).arg(escapePointerToken(position));
    JPtr b = node(ptr);
    QString kind;
    m[QStringLiteral("display")] = bindingDisplay(b, &kind);
    m[QStringLiteral("kind")] = kind;
    m[QStringLiteral("pointer")] = ptr;
    QString value;
    if (b && b->type == JNode::String)
        value = b->str;
    else if (b && b->type == JNode::Object) {
        if (JPtr o = b->member(QStringLiteral("output")); o && o->type == JNode::String)
            value = o->str;
        else if (JPtr d = b->member(QStringLiteral("deadKey")))
            value = d->str;
        else if (JPtr k = b->member(QStringLiteral("key")))
            value = k->str;
    }
    m[QStringLiteral("value")] = value;
    m[QStringLiteral("json")] = b ? QString::fromUtf8(serializeValue(*b->deepCopy(), {}, true)) : QString();
    if (!b) {
        if (JPtr f = fallbackBinding(layer, position)) {
            QString fk;
            m[QStringLiteral("fallback")] = bindingDisplay(f, &fk);
        }
    }
    return m;
}

QVariantList KblDocument::bindingsForPosition(const QString &position) const
{
    QVariantList out;
    const QVariantList ls = layers();
    for (const QVariant &l : ls) {
        const QVariantMap lm = l.toMap();
        QVariantMap m = bindingAt(lm.value(QStringLiteral("index")).toInt(), position);
        m[QStringLiteral("layer")] = lm.value(QStringLiteral("index"));
        m[QStringLiteral("layerLabel")] = lm.value(QStringLiteral("label"));
        m[QStringLiteral("layerId")] = lm.value(QStringLiteral("id"));
        out << m;
    }
    return out;
}

void KblDocument::setBinding(int layer, const QString &position, const QString &kind, const QString &value)
{
    const QString ptr = QStringLiteral("/hardware/layers/%1/bindings/%2").arg(layer).arg(escapePointerToken(position));
    if (!exists(QStringLiteral("/hardware/layers/%1").arg(layer)))
        return;
    const QString label = tr("Set %1").arg(position);
    if (kind == QLatin1String("unbound")) {
        removeAt(ptr, tr("Unbind %1").arg(position));
        return;
    }
    JPtr v;
    if (kind == QLatin1String("text")) {
        if (value.isEmpty()) {
            removeAt(ptr, tr("Unbind %1").arg(position));
            return;
        }
        v = JNode::makeString(value);
    } else if (kind == QLatin1String("nothing")) {
        v = JNode::makeObject();
        v->members.push_back(JMember{QStringLiteral("output"), "\"output\"", -1, JNode::makeString(QString())});
    } else if (kind == QLatin1String("deadKey") || kind == QLatin1String("key")) {
        v = JNode::makeObject();
        v->members.push_back(JMember{kind, quoteString(kind), -1, JNode::makeString(value)});
    } else {
        return;
    }
    setNode(ptr, v, label);
}

QVariantMap KblDocument::positionInfo(const QString &position) const
{
    QVariantMap m;
    const QString ptr = QStringLiteral("/positions/") + escapePointerToken(position);
    JPtr p = node(ptr);
    m[QStringLiteral("id")] = position;
    m[QStringLiteral("pointer")] = ptr;
    m[QStringLiteral("exists")] = bool(p);
    auto str = [&](const char *k) {
        JPtr v = p ? p->member(QLatin1String(k)) : JPtr();
        return v && v->type == JNode::String ? v->str : QString();
    };
    m[QStringLiteral("scanCode")] = str("scanCode");
    m[QStringLiteral("name")] = str("name");
    m[QStringLiteral("description")] = str("description");
    JPtr nl = p ? p->member(QStringLiteral("numLock")) : JPtr();
    m[QStringLiteral("numLock")] = nl && nl->type == JNode::Bool && nl->boolean;
    bool modifier = modifierPositions().contains(position);
    if (JPtr cm = node(QStringLiteral("/customModifiers")); cm && cm->type == JNode::Object)
        for (const auto &c : cm->members)
            if (JPtr ps = c.value->member(QStringLiteral("positions")); ps && ps->type == JNode::Array)
                for (const auto &x : ps->items)
                    if (x->str == position)
                        modifier = true;
    m[QStringLiteral("modifier")] = modifier;
    return m;
}

QVariantList KblDocument::keyboardRows(int layer) const
{
    QVariantList out;
    if (!loaded())
        return out;
    // Positions the file knows: declared ones and those bound in any layer.
    QStringList known;
    QSet<QString> knownSet;
    auto add = [&](const QString &p) {
        if (!knownSet.contains(p)) {
            knownSet.insert(p);
            known << p;
        }
    };
    if (JPtr ps = node(QStringLiteral("/positions")); ps && ps->type == JNode::Object)
        for (const auto &m : ps->members)
            add(m.key);
    if (JPtr ls = node(QStringLiteral("/hardware/layers")); ls && ls->type == JNode::Array)
        for (const auto &l : ls->items)
            if (JPtr b = l->member(QStringLiteral("bindings")); b && b->type == JNode::Object)
                for (const auto &m : b->members)
                    add(m.key);

    // Main block: the file's own form if it names one, otherwise the implied form that fits.
    QList<QStringList> rows;
    JPtr formId = node(QStringLiteral("/hardware/formId"));
    JPtr form = formId && formId->type == JNode::String
                    ? node(QStringLiteral("/forms/") + escapePointerToken(formId->str) + QStringLiteral("/rows"))
                    : JPtr();
    if (form && form->type == JNode::Array) {
        for (const auto &r : form->items) {
            QStringList row;
            for (const auto &x : r->items)
                row << x->str;
            rows << row;
        }
    } else {
        rows = impliedForm(knownSet.contains(QStringLiteral("IntlBackslash")) ? QStringLiteral("iso")
                                                                             : QStringLiteral("us"));
    }
    // Frame keys around it, where the file has them.
    QStringList fn{QStringLiteral("Escape")};
    for (int i = 1; i <= 12; ++i)
        fn << QStringLiteral("F%1").arg(i);
    QList<QStringList> board;
    board << fn;
    for (int i = 0; i < rows.size(); ++i) {
        QStringList r = rows[i];
        const bool last = i == rows.size() - 1;
        if (i == 0)
            r << QStringLiteral("Backspace");
        else if (i == 1)
            r.prepend(QStringLiteral("Tab"));
        else if (i == 2) {
            r.prepend(QStringLiteral("CapsLock"));
            r << QStringLiteral("Enter");
        } else if (i == 3) {
            r.prepend(QStringLiteral("ShiftLeft"));
            r << QStringLiteral("ShiftRight");
        }
        if (last && r.contains(QStringLiteral("Space"))) {
            r = QStringList{QStringLiteral("ControlLeft"), QStringLiteral("MetaLeft"), QStringLiteral("AltLeft")} + r
                + QStringList{QStringLiteral("AltRight"), QStringLiteral("MetaRight"), QStringLiteral("ContextMenu"),
                              QStringLiteral("ControlRight")};
        }
        board << r;
    }
    QSet<QString> placed;
    const bool iso = knownSet.contains(QStringLiteral("IntlBackslash"));
    auto keyItem = [&](const QString &id, double width) {
        QVariantMap k;
        k[QStringLiteral("id")] = id;
        k[QStringLiteral("width")] = width;
        const QVariantMap b = bindingAt(layer, id);
        const QString label = b.value(QStringLiteral("display")).toString();
        k[QStringLiteral("label")] = renderable(label) ? label : codePoints(label);
        k[QStringLiteral("kind")] = b.value(QStringLiteral("kind"));
        const QString fb = b.value(QStringLiteral("fallback")).toString();
        k[QStringLiteral("fallback")] = renderable(fb) ? fb : codePoints(fb);
        k[QStringLiteral("present")] = knownSet.contains(id);
        k[QStringLiteral("modifier")] = positionInfo(id).value(QStringLiteral("modifier"));
        return k;
    };
    for (const QStringList &r : board) {
        QVariantList row;
        for (const QString &id : r) {
            if (placed.contains(id))
                continue;
            const bool frame = !rows.isEmpty() && !std::any_of(rows.begin(), rows.end(), [&](const QStringList &x) {
                return x.contains(id);
            });
            if (frame && !knownSet.contains(id) && !modifierPositions().contains(id))
                continue; // a frame key the file does not mention is left out
            double w = widthOf(id);
            if (iso && id == QLatin1String("ShiftLeft"))
                w = 1.25;
            if (iso && id == QLatin1String("Enter"))
                w = 1.25;
            placed.insert(id);
            row << keyItem(id, w);
        }
        if (!row.isEmpty())
            out << QVariant(row);
    }
    // Everything else the file has, in file order.
    QVariantList rest;
    for (const QString &id : known)
        if (!placed.contains(id))
            rest << keyItem(id, 1.0);
    for (int i = 0; i < rest.size(); i += 15)
        out << QVariant(rest.mid(i, 15));
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Dead keys

QStringList KblDocument::deadKeyIds() const
{
    return keysAt(QStringLiteral("/deadKeys"));
}

QStringList KblDocument::keyIds() const
{
    return keysAt(QStringLiteral("/keys"));
}

static QString textOf(const JPtr &v, QString *kind)
{
    if (!v) {
        *kind = QString();
        return {};
    }
    if (v->type == JNode::String) {
        *kind = QStringLiteral("text");
        return v->str;
    }
    if (v->type == JNode::Object) {
        if (JPtr d = v->member(QStringLiteral("deadKey"))) {
            *kind = QStringLiteral("deadKey");
            return d->str;
        }
        if (JPtr u = v->member(QStringLiteral("utf16")); u && u->type == JNode::Array) {
            *kind = QStringLiteral("codeUnits");
            QStringList units;
            for (const auto &x : u->items)
                units << x->str;
            return units.join(QLatin1Char(' '));
        }
    }
    *kind = QStringLiteral("other");
    return {};
}

QVariantMap KblDocument::deadKeyInfo(const QString &id) const
{
    QVariantMap m;
    const QString ptr = QStringLiteral("/deadKeys/") + escapePointerToken(id);
    JPtr d = node(ptr);
    m[QStringLiteral("id")] = id;
    m[QStringLiteral("pointer")] = ptr;
    if (!d)
        return m;
    QString kind;
    m[QStringLiteral("spacing")] = textOf(d->member(QStringLiteral("spacing")), &kind);
    m[QStringLiteral("spacingKind")] = kind;
    JPtr name = d->member(QStringLiteral("name"));
    m[QStringLiteral("name")] = name && name->type == JNode::String ? name->str : QString();
    JPtr nm = d->member(QStringLiteral("noMatch"));
    m[QStringLiteral("noMatch")] = nm && nm->type == JNode::String ? nm->str : QStringLiteral("emitBoth");
    JPtr e = d->member(QStringLiteral("entries"));
    m[QStringLiteral("count")] = e ? e->childCount() : 0;
    m[QStringLiteral("display")] = displayText(m.value(QStringLiteral("spacing")).toString());
    return m;
}

QVariantList KblDocument::deadKeyEntries(const QString &id) const
{
    QVariantList out;
    JPtr e = node(QStringLiteral("/deadKeys/") + escapePointerToken(id) + QStringLiteral("/entries"));
    if (!e || e->type != JNode::Array)
        return out;
    for (size_t i = 0; i < e->items.size(); ++i) {
        const JPtr &entry = e->items[i];
        QVariantMap m;
        QString bk, rk;
        m[QStringLiteral("index")] = int(i);
        m[QStringLiteral("base")] = textOf(entry->member(QStringLiteral("base")), &bk);
        m[QStringLiteral("baseKind")] = bk;
        m[QStringLiteral("result")] = textOf(entry->member(QStringLiteral("result")), &rk);
        m[QStringLiteral("resultKind")] = rk;
        m[QStringLiteral("baseDisplay")] = bk == QLatin1String("text") ? displayText(m[QStringLiteral("base")].toString())
                                                                       : m[QStringLiteral("base")].toString();
        m[QStringLiteral("resultDisplay")] = rk == QLatin1String("text")
                                                 ? displayText(m[QStringLiteral("result")].toString())
                                                 : m[QStringLiteral("result")].toString();
        out << m;
    }
    return out;
}

static JPtr textValue(const QString &kindOrText, const QString &text)
{
    Q_UNUSED(kindOrText)
    return JNode::makeString(text);
}

void KblDocument::setDeadKeyEntry(const QString &id, int index, const QString &base, const QString &result)
{
    const QString ptr = QStringLiteral("/deadKeys/%1/entries/%2").arg(escapePointerToken(id)).arg(index);
    JPtr cur = node(ptr);
    if (!cur || cur->type != JNode::Object)
        return;
    std::vector<Edit> edits;
    QString bk, rk;
    const QString oldBase = textOf(cur->member(QStringLiteral("base")), &bk);
    const QString oldResult = textOf(cur->member(QStringLiteral("result")), &rk);
    Edit e;
    if (bk == QLatin1String("text") && base != oldBase && planSet(ptr + QStringLiteral("/base"), textValue({}, base), &e))
        edits.push_back(e);
    if (rk == QLatin1String("text") && result != oldResult
        && planSet(ptr + QStringLiteral("/result"), textValue({}, result), &e))
        edits.push_back(e);
    applyEdits(edits, tr("Edit dead key %1").arg(id));
}

void KblDocument::addDeadKeyEntry(const QString &id, const QString &base, const QString &result)
{
    const QString ptr = QStringLiteral("/deadKeys/%1/entries/-").arg(escapePointerToken(id));
    JPtr v = JNode::makeObject();
    v->members.push_back(JMember{QStringLiteral("base"), "\"base\"", -1, JNode::makeString(base)});
    v->members.push_back(JMember{QStringLiteral("result"), "\"result\"", -1, JNode::makeString(result)});
    setNode(ptr, v, tr("Add entry to %1").arg(id));
}

void KblDocument::removeDeadKeyEntry(const QString &id, int index)
{
    removeAt(QStringLiteral("/deadKeys/%1/entries/%2").arg(escapePointerToken(id)).arg(index),
             tr("Remove entry from %1").arg(id));
}

QString KblDocument::addDeadKey(const QString &id, const QString &spacing)
{
    if (id.isEmpty() || id.contains(QLatin1Char(',')) || id.contains(QRegularExpression(QStringLiteral("\\s"))))
        return tr("A dead key id is a single word without commas");
    if (exists(QStringLiteral("/deadKeys/") + escapePointerToken(id)))
        return tr("A dead key named %1 exists already").arg(id);
    if (spacing.isEmpty())
        return tr("Give the character the dead key stands for");
    JPtr v = JNode::makeObject();
    v->members.push_back(JMember{QStringLiteral("spacing"), "\"spacing\"", -1, JNode::makeString(spacing)});
    v->members.push_back(JMember{QStringLiteral("entries"), "\"entries\"", -1, JNode::makeArray()});
    setNode(QStringLiteral("/deadKeys/") + escapePointerToken(id), v, tr("Add dead key %1").arg(id));
    return {};
}

void KblDocument::removeDeadKey(const QString &id)
{
    removeAt(QStringLiteral("/deadKeys/") + escapePointerToken(id), tr("Remove dead key %1").arg(id));
}

} // namespace kbl
