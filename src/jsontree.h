// An order-preserving JSON tree that remembers where every value came from in the file's bytes, so that saving
// writes the untouched parts byte for byte and an edit changes only the text of the edited value.
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace kbl {

struct JNode;
using JPtr = std::shared_ptr<JNode>;
using JBuf = std::shared_ptr<const QByteArray>;

struct JMember {
    QString key;
    QByteArray keyRaw;      // the key as written, quotes and escapes included
    qsizetype keyBegin = -1; // offset of keyRaw in the parent's buffer, -1 for a member added by an edit
    JPtr value;
};

struct JNode {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;

    QByteArray raw;     // scalars: the exact text (source or serialised)
    QString str;        // String: the decoded value
    bool boolean = false;
    double number = 0;

    std::vector<JMember> members; // Object, in file order
    std::vector<JPtr> items;      // Array

    // Where the node was read from. buf is null for a node made by an edit.
    JBuf buf;
    qsizetype begin = -1, end = -1;
    std::vector<JMember> origMembers; // the children as read, to tell an edited container from an untouched one
    std::vector<JPtr> origItems;

    bool isContainer() const { return type == Array || type == Object; }
    int childCount() const { return int(type == Object ? members.size() : items.size()); }
    int indexOf(const QString &key) const;
    JPtr member(const QString &key) const;

    static JPtr makeNull();
    static JPtr makeBool(bool b);
    static JPtr makeNumber(double d);
    static JPtr makeNumberRaw(const QByteArray &raw);
    static JPtr makeString(const QString &s);
    static JPtr makeObject();
    static JPtr makeArray();
    JPtr deepCopy() const; // an unattached copy without source spans
};

struct ParseError {
    qsizetype offset = -1;
    int line = 0;   // 1-based
    int column = 0; // 1-based, in characters
    QString message;
    bool ok() const { return offset < 0; }
};

// The whole file: the value plus the text around it.
struct JDocument {
    JPtr root;
    QByteArray prefix; // before the value (a byte-order mark or white space)
    QByteArray suffix; // after the value (normally "\n")
    QByteArray newline = "\n";
};

JDocument parseDocument(const QByteArray &bytes, ParseError *error);
JPtr parseValue(const QByteArray &bytes, ParseError *error); // a standalone value, no spans kept
QByteArray serialize(const JDocument &doc);
// Serialises a value in the canonical style (2-space indent) with the given indent for continuation lines.
QByteArray serializeValue(const JNode &node, const QByteArray &indent = {}, bool compact = false);
QByteArray quoteString(const QString &s);

// JSON Pointers (RFC 6901).
QStringList splitPointer(const QString &pointer, bool *ok = nullptr);
QString joinPointer(const QStringList &tokens);
QString escapePointerToken(const QString &token);
JPtr resolve(const JPtr &root, const QString &pointer);

// Order-insensitive structural comparison of two values.
bool sameValue(const JNode &a, const JNode &b);

} // namespace kbl
