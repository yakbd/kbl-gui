#include "jsontree.h"

#include <QLocale>
#include <QStringDecoder>
#include <cmath>
#include <functional>

namespace kbl {

int JNode::indexOf(const QString &key) const
{
    for (size_t i = 0; i < members.size(); ++i)
        if (members[i].key == key)
            return int(i);
    return -1;
}

JPtr JNode::member(const QString &key) const
{
    const int i = indexOf(key);
    return i < 0 ? JPtr() : members[size_t(i)].value;
}

JPtr JNode::makeNull()
{
    auto n = std::make_shared<JNode>();
    n->type = Null;
    n->raw = "null";
    return n;
}

JPtr JNode::makeBool(bool b)
{
    auto n = std::make_shared<JNode>();
    n->type = Bool;
    n->boolean = b;
    n->raw = b ? "true" : "false";
    return n;
}

JPtr JNode::makeNumber(double d)
{
    QByteArray raw;
    if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 1e15)
        raw = QByteArray::number(qint64(d));
    else
        raw = QByteArray::number(d, 'g', QLocale::FloatingPointShortest);
    return makeNumberRaw(raw);
}

JPtr JNode::makeNumberRaw(const QByteArray &raw)
{
    auto n = std::make_shared<JNode>();
    n->type = Number;
    n->raw = raw;
    n->number = raw.toDouble();
    return n;
}

JPtr JNode::makeString(const QString &s)
{
    auto n = std::make_shared<JNode>();
    n->type = String;
    n->str = s;
    n->raw = quoteString(s);
    return n;
}

JPtr JNode::makeObject()
{
    auto n = std::make_shared<JNode>();
    n->type = Object;
    return n;
}

JPtr JNode::makeArray()
{
    auto n = std::make_shared<JNode>();
    n->type = Array;
    return n;
}

JPtr JNode::deepCopy() const
{
    auto n = std::make_shared<JNode>();
    n->type = type;
    n->raw = raw;
    n->str = str;
    n->boolean = boolean;
    n->number = number;
    for (const auto &m : members)
        n->members.push_back(JMember{m.key, m.keyRaw, -1, m.value->deepCopy()});
    for (const auto &i : items)
        n->items.push_back(i->deepCopy());
    return n;
}

// ---------------------------------------------------------------------------------------------------------------
// Parser

namespace {

struct Parser {
    const QByteArray &s;
    JBuf buf;
    qsizetype p = 0;
    QString err;
    qsizetype errAt = -1;
    int depth = 0;

    bool fail(const QString &m, qsizetype at)
    {
        if (errAt < 0) {
            err = m;
            errAt = at;
        }
        return false;
    }
    void ws()
    {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r'))
            ++p;
    }
    bool literal(const char *word)
    {
        const qsizetype n = qsizetype(strlen(word));
        if (s.mid(p, n) != word)
            return fail(QStringLiteral("invalid value"), p);
        p += n;
        return true;
    }

    static int hexval(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    // Reads a string starting at the quote; returns the decoded value and leaves p after the closing quote.
    bool string(QString *out)
    {
        const qsizetype start = p;
        ++p; // opening quote
        QString result;
        QByteArray pending; // raw UTF-8 bytes not yet decoded
        auto flush = [&]() -> bool {
            if (pending.isEmpty())
                return true;
            QStringDecoder dec(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
            QString part = dec.decode(pending);
            if (dec.hasError())
                return fail(QStringLiteral("invalid UTF-8 in a string"), start);
            result += part;
            pending.clear();
            return true;
        };
        while (true) {
            if (p >= s.size())
                return fail(QStringLiteral("unterminated string"), start);
            const char c = s[p];
            if (c == '"') {
                ++p;
                break;
            }
            if (uchar(c) < 0x20)
                return fail(QStringLiteral("control character in a string must be escaped"), p);
            if (c != '\\') {
                pending += c;
                ++p;
                continue;
            }
            if (!flush())
                return false;
            if (p + 1 >= s.size())
                return fail(QStringLiteral("unterminated string"), start);
            const char e = s[p + 1];
            p += 2;
            switch (e) {
            case '"': result += QLatin1Char('"'); break;
            case '\\': result += QLatin1Char('\\'); break;
            case '/': result += QLatin1Char('/'); break;
            case 'b': result += QChar(0x08); break;
            case 'f': result += QChar(0x0C); break;
            case 'n': result += QChar(0x0A); break;
            case 'r': result += QChar(0x0D); break;
            case 't': result += QChar(0x09); break;
            case 'u': {
                if (p + 4 > s.size())
                    return fail(QStringLiteral("short \\u escape"), p - 2);
                int v = 0;
                for (int i = 0; i < 4; ++i) {
                    const int h = hexval(s[p + i]);
                    if (h < 0)
                        return fail(QStringLiteral("invalid \\u escape"), p - 2);
                    v = v * 16 + h;
                }
                p += 4;
                result += QChar(char16_t(v)); // surrogate pairs combine naturally in UTF-16
                break;
            }
            default:
                return fail(QStringLiteral("invalid escape \\%1").arg(QLatin1Char(e)), p - 2);
            }
        }
        if (!flush())
            return false;
        *out = result;
        return true;
    }

    bool number(JNode *n)
    {
        const qsizetype start = p;
        if (p < s.size() && s[p] == '-')
            ++p;
        if (p >= s.size())
            return fail(QStringLiteral("invalid number"), start);
        if (s[p] == '0')
            ++p;
        else if (s[p] >= '1' && s[p] <= '9')
            while (p < s.size() && s[p] >= '0' && s[p] <= '9')
                ++p;
        else
            return fail(QStringLiteral("invalid number"), start);
        if (p < s.size() && s[p] == '.') {
            ++p;
            const qsizetype d = p;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9')
                ++p;
            if (p == d)
                return fail(QStringLiteral("invalid number"), start);
        }
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            ++p;
            if (p < s.size() && (s[p] == '+' || s[p] == '-'))
                ++p;
            const qsizetype d = p;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9')
                ++p;
            if (p == d)
                return fail(QStringLiteral("invalid number"), start);
        }
        n->type = JNode::Number;
        n->raw = s.mid(start, p - start);
        n->number = n->raw.toDouble();
        return true;
    }

    JPtr value()
    {
        ws();
        if (p >= s.size()) {
            fail(QStringLiteral("unexpected end of file"), p);
            return {};
        }
        if (++depth > 512) {
            fail(QStringLiteral("nesting too deep"), p);
            return {};
        }
        auto n = std::make_shared<JNode>();
        n->buf = buf;
        n->begin = p;
        const char c = s[p];
        bool ok = true;
        if (c == '{') {
            n->type = JNode::Object;
            ++p;
            ws();
            if (p < s.size() && s[p] == '}') {
                ++p;
            } else {
                while (ok) {
                    ws();
                    if (p >= s.size() || s[p] != '"') {
                        ok = fail(QStringLiteral("expected a property name in quotes"), p);
                        break;
                    }
                    JMember m;
                    m.keyBegin = p;
                    ok = string(&m.key);
                    if (!ok)
                        break;
                    m.keyRaw = s.mid(m.keyBegin, p - m.keyBegin);
                    ws();
                    if (p >= s.size() || s[p] != ':') {
                        ok = fail(QStringLiteral("expected ':' after a property name"), p);
                        break;
                    }
                    ++p;
                    m.value = value();
                    if (!m.value) {
                        ok = false;
                        break;
                    }
                    n->members.push_back(m);
                    ws();
                    if (p < s.size() && s[p] == ',') {
                        ++p;
                        continue;
                    }
                    if (p < s.size() && s[p] == '}') {
                        ++p;
                        break;
                    }
                    ok = fail(QStringLiteral("expected ',' or '}'"), p);
                }
            }
            n->origMembers = n->members;
        } else if (c == '[') {
            n->type = JNode::Array;
            ++p;
            ws();
            if (p < s.size() && s[p] == ']') {
                ++p;
            } else {
                while (ok) {
                    JPtr v = value();
                    if (!v) {
                        ok = false;
                        break;
                    }
                    n->items.push_back(v);
                    ws();
                    if (p < s.size() && s[p] == ',') {
                        ++p;
                        continue;
                    }
                    if (p < s.size() && s[p] == ']') {
                        ++p;
                        break;
                    }
                    ok = fail(QStringLiteral("expected ',' or ']'"), p);
                }
            }
            n->origItems = n->items;
        } else if (c == '"') {
            n->type = JNode::String;
            ok = string(&n->str);
            if (ok)
                n->raw = s.mid(n->begin, p - n->begin);
        } else if (c == 't') {
            n->type = JNode::Bool;
            n->boolean = true;
            ok = literal("true");
            n->raw = "true";
        } else if (c == 'f') {
            n->type = JNode::Bool;
            ok = literal("false");
            n->raw = "false";
        } else if (c == 'n') {
            n->type = JNode::Null;
            ok = literal("null");
            n->raw = "null";
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            ok = number(n.get());
        } else {
            ok = fail(QStringLiteral("unexpected character '%1'").arg(QChar(uchar(c))), p);
        }
        --depth;
        if (!ok)
            return {};
        n->end = p;
        return n;
    }
};

void locate(const QByteArray &s, qsizetype at, ParseError *e)
{
    int line = 1;
    qsizetype lineStart = 0;
    for (qsizetype i = 0; i < at && i < s.size(); ++i)
        if (s[i] == '\n') {
            ++line;
            lineStart = i + 1;
        }
    e->line = line;
    e->column = int(QString::fromUtf8(s.mid(lineStart, at - lineStart)).size()) + 1;
}

void stripSpans(JNode &n)
{
    n.buf.reset();
    n.begin = n.end = -1;
    n.origMembers.clear();
    n.origItems.clear();
    for (auto &m : n.members) {
        m.keyBegin = -1;
        stripSpans(*m.value);
    }
    for (auto &i : n.items)
        stripSpans(*i);
}

} // namespace

JDocument parseDocument(const QByteArray &bytes, ParseError *error)
{
    JDocument doc;
    auto buf = std::make_shared<const QByteArray>(bytes);
    Parser ps{*buf, buf};
    qsizetype start = 0;
    if (bytes.startsWith("\xEF\xBB\xBF"))
        start = 3;
    ps.p = start;
    JPtr root = ps.value();
    if (root) {
        ps.ws();
        if (ps.p != bytes.size())
            ps.fail(QStringLiteral("unexpected text after the JSON value"), ps.p);
    }
    if (ps.errAt >= 0) {
        if (error) {
            error->offset = ps.errAt;
            error->message = ps.err;
            locate(bytes, ps.errAt, error);
        }
        return {};
    }
    if (error)
        *error = ParseError{};
    doc.root = root;
    doc.prefix = bytes.left(root->begin);
    doc.suffix = bytes.mid(root->end);
    doc.newline = bytes.contains("\r\n") ? QByteArray("\r\n") : QByteArray("\n");
    return doc;
}

JPtr parseValue(const QByteArray &bytes, ParseError *error)
{
    ParseError e;
    JDocument d = parseDocument(bytes, &e);
    if (error)
        *error = e;
    if (!d.root)
        return {};
    stripSpans(*d.root);
    return d.root;
}

// ---------------------------------------------------------------------------------------------------------------
// Writer

QByteArray quoteString(const QString &s)
{
    static const char hex[] = "0123456789abcdef";
    QByteArray out;
    out.reserve(s.size() + 2);
    out += '"';
    auto escape = [&](char16_t u) {
        out += "\\u";
        out += hex[(u >> 12) & 15];
        out += hex[(u >> 8) & 15];
        out += hex[(u >> 4) & 15];
        out += hex[u & 15];
    };
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar ch = s[i];
        const char16_t u = ch.unicode();
        if (ch.isHighSurrogate() && i + 1 < s.size() && s[i + 1].isLowSurrogate()) {
            out += s.mid(i, 2).toUtf8();
            ++i;
            continue;
        }
        switch (u) {
        case '"': out += "\\\""; continue;
        case '\\': out += "\\\\"; continue;
        case '\n': out += "\\n"; continue;
        case '\r': out += "\\r"; continue;
        case '\t': out += "\\t"; continue;
        default: break;
        }
        // Other control characters, the two separators JavaScript dislikes, and lone surrogates are escaped.
        if (u < 0x20 || u == 0x2028 || u == 0x2029 || ch.isSurrogate()) {
            escape(u);
            continue;
        }
        out += QString(ch).toUtf8();
    }
    out += '"';
    return out;
}

namespace {

QByteArray lineIndent(const QByteArray &s, qsizetype pos)
{
    qsizetype start = pos;
    while (start > 0 && s[start - 1] != '\n')
        --start;
    qsizetype e = start;
    while (e < s.size() && (s[e] == ' ' || s[e] == '\t'))
        ++e;
    return s.mid(start, e - start);
}

bool sameChildren(const JNode &n)
{
    if (n.type == JNode::Object) {
        if (n.members.size() != n.origMembers.size())
            return false;
        for (size_t i = 0; i < n.members.size(); ++i)
            if (n.members[i].value != n.origMembers[i].value || n.members[i].key != n.origMembers[i].key)
                return false;
        return true;
    }
    if (n.items.size() != n.origItems.size())
        return false;
    for (size_t i = 0; i < n.items.size(); ++i)
        if (n.items[i] != n.origItems[i])
            return false;
    return true;
}

bool sameKeys(const JNode &n)
{
    if (n.type == JNode::Object) {
        if (n.members.size() != n.origMembers.size())
            return false;
        for (size_t i = 0; i < n.members.size(); ++i)
            if (n.members[i].key != n.origMembers[i].key)
                return false;
        return true;
    }
    return n.items.size() == n.origItems.size();
}

bool isClean(const JNode &n)
{
    if (!n.buf)
        return false;
    if (!n.isContainer())
        return true;
    if (!sameChildren(n))
        return false;
    for (const auto &m : n.members)
        if (!isClean(*m.value))
            return false;
    for (const auto &i : n.items)
        if (!isClean(*i))
            return false;
    return true;
}

struct Writer {
    QByteArray nl = "\n";

    void write(const JNode &n, const QByteArray &indent, bool compact, QByteArray &out)
    {
        if (isClean(n)) {
            out += QByteArrayView(*n.buf).mid(n.begin, n.end - n.begin);
            return;
        }
        if (!n.isContainer()) {
            out += n.raw;
            return;
        }
        if (n.buf && sameKeys(n) && n.childCount() > 0) {
            // Same members as read: copy the text between the children and write only the changed ones.
            const QByteArray &s = *n.buf;
            qsizetype pos = n.begin;
            const bool inl = !QByteArrayView(s).mid(n.begin, n.end - n.begin).contains('\n');
            for (int i = 0; i < n.childCount(); ++i) {
                const JNode &orig = n.type == JNode::Object ? *n.origMembers[size_t(i)].value : *n.origItems[size_t(i)];
                const JNode &cur = n.type == JNode::Object ? *n.members[size_t(i)].value : *n.items[size_t(i)];
                out += QByteArrayView(s).mid(pos, orig.begin - pos);
                write(cur, lineIndent(s, orig.begin), inl, out);
                pos = orig.end;
            }
            out += QByteArrayView(s).mid(pos, n.end - pos);
            return;
        }
        // Members added or removed: write the container again in its own style.
        bool inl = compact;
        QByteArray close = indent;
        QByteArray inner = indent + "  ";
        if (n.buf) {
            const QByteArray &s = *n.buf;
            inl = !QByteArrayView(s).mid(n.begin, n.end - n.begin).contains('\n') && n.childCount() > 0
                  && (n.origMembers.size() + n.origItems.size()) > 0;
            if (n.origMembers.empty() && n.origItems.empty())
                inl = compact;
            close = lineIndent(s, n.begin);
            inner = close + "  ";
            if (!n.origMembers.empty() && n.origMembers[0].keyBegin >= 0)
                inner = lineIndent(s, n.origMembers[0].keyBegin);
            else if (!n.origItems.empty() && n.origItems[0]->begin >= 0
                     && lineIndent(s, n.origItems[0]->begin).size() > close.size())
                inner = lineIndent(s, n.origItems[0]->begin);
        }
        const bool obj = n.type == JNode::Object;
        const char open = obj ? '{' : '[';
        const char shut = obj ? '}' : ']';
        if (n.childCount() == 0) {
            out += open;
            out += shut;
            return;
        }
        out += open;
        if (inl && obj)
            out += ' ';
        for (int i = 0; i < n.childCount(); ++i) {
            if (i > 0)
                out += inl ? ", " : ",";
            if (!inl) {
                out += nl;
                out += inner;
            }
            if (obj) {
                const JMember &m = n.members[size_t(i)];
                out += m.keyRaw.isEmpty() ? quoteString(m.key) : m.keyRaw;
                out += ": ";
                write(*m.value, inner, inl, out);
            } else {
                write(*n.items[size_t(i)], inner, inl, out);
            }
        }
        if (!inl) {
            out += nl;
            out += close;
        } else if (obj) {
            out += ' ';
        }
        out += shut;
    }
};

} // namespace

QByteArray serialize(const JDocument &doc)
{
    if (!doc.root)
        return {};
    Writer w;
    w.nl = doc.newline;
    QByteArray out = doc.prefix;
    w.write(*doc.root, {}, false, out);
    out += doc.suffix;
    return out;
}

QByteArray serializeValue(const JNode &node, const QByteArray &indent, bool compact)
{
    Writer w;
    QByteArray out;
    w.write(node, indent, compact, out);
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Pointers

QString escapePointerToken(const QString &token)
{
    QString t = token;
    t.replace(QLatin1Char('~'), QLatin1String("~0"));
    t.replace(QLatin1Char('/'), QLatin1String("~1"));
    return t;
}

QStringList splitPointer(const QString &pointer, bool *ok)
{
    if (ok)
        *ok = true;
    if (pointer.isEmpty())
        return {};
    if (!pointer.startsWith(QLatin1Char('/'))) {
        if (ok)
            *ok = false;
        return {};
    }
    QStringList parts = pointer.mid(1).split(QLatin1Char('/'));
    for (QString &p : parts) {
        p.replace(QLatin1String("~1"), QLatin1String("/"));
        p.replace(QLatin1String("~0"), QLatin1String("~"));
    }
    return parts;
}

QString joinPointer(const QStringList &tokens)
{
    QString out;
    for (const QString &t : tokens)
        out += QLatin1Char('/') + escapePointerToken(t);
    return out;
}

static bool arrayIndex(const QString &t, int *out)
{
    if (t.isEmpty() || (t.size() > 1 && t[0] == QLatin1Char('0')))
        return false;
    bool ok = false;
    const int v = t.toInt(&ok);
    if (!ok || v < 0)
        return false;
    *out = v;
    return true;
}

JPtr resolve(const JPtr &root, const QString &pointer)
{
    bool ok = false;
    const QStringList parts = splitPointer(pointer, &ok);
    if (!ok)
        return {};
    JPtr n = root;
    for (const QString &t : parts) {
        if (!n)
            return {};
        if (n->type == JNode::Object) {
            n = n->member(t);
        } else if (n->type == JNode::Array) {
            int i = 0;
            if (!arrayIndex(t, &i) || i >= int(n->items.size()))
                return {};
            n = n->items[size_t(i)];
        } else {
            return {};
        }
    }
    return n;
}

bool sameValue(const JNode &a, const JNode &b)
{
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
    case JNode::Null: return true;
    case JNode::Bool: return a.boolean == b.boolean;
    case JNode::Number: return a.number == b.number;
    case JNode::String: return a.str == b.str;
    case JNode::Array:
        if (a.items.size() != b.items.size())
            return false;
        for (size_t i = 0; i < a.items.size(); ++i)
            if (!sameValue(*a.items[i], *b.items[i]))
                return false;
        return true;
    case JNode::Object:
        if (a.members.size() != b.members.size())
            return false;
        for (const auto &m : a.members) {
            JPtr o = b.member(m.key);
            if (!o || !sameValue(*m.value, *o))
                return false;
        }
        return true;
    }
    return false;
}

} // namespace kbl
