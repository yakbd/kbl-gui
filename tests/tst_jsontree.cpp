// The JSON tree keeps the file's bytes: untouched files round-trip exactly, and an edit changes only its lines.
#include "jsontree.h"
#include "kbldocument.h"

#include <QDir>
#include <QFile>
#include <QTest>

using namespace kbl;

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot read %s", qPrintable(path));
    return f.readAll();
}

static QString dataDir()
{
    return qEnvironmentVariable("YAKBD_TESTDATA");
}

// Lines that differ between two texts (a naive line diff: enough for "only these lines changed").
static QStringList changedLines(const QByteArray &a, const QByteArray &b)
{
    const QList<QByteArray> la = a.split('\n'), lb = b.split('\n');
    int start = 0;
    while (start < la.size() && start < lb.size() && la[start] == lb[start])
        ++start;
    int ea = la.size() - 1, eb = lb.size() - 1;
    while (ea >= start && eb >= start && la[ea] == lb[eb]) {
        --ea;
        --eb;
    }
    QStringList out;
    for (int i = start; i <= ea; ++i)
        out << QStringLiteral("-") + QString::fromUtf8(la[i]);
    for (int i = start; i <= eb; ++i)
        out << QStringLiteral("+") + QString::fromUtf8(lb[i]);
    return out;
}

class TestJsonTree : public QObject
{
    Q_OBJECT
private slots:
    void roundTripExamples_data()
    {
        QTest::addColumn<QString>("file");
        const QStringList files = QDir(dataDir()).entryList({QStringLiteral("*.kbl.json")}, QDir::Files, QDir::Name);
        QVERIFY(files.size() >= 6);
        for (const QString &f : files)
            QTest::newRow(qPrintable(f)) << QDir(dataDir()).filePath(f);
    }
    void roundTripExamples()
    {
        QFETCH(QString, file);
        const QByteArray bytes = readFile(file);
        ParseError e;
        JDocument d = parseDocument(bytes, &e);
        QVERIFY2(d.root, qPrintable(e.message));
        QCOMPARE(serialize(d), bytes);
    }

    void roundTripOddFormatting()
    {
        const QByteArray in = "\xEF\xBB\xBF  {\"a\":1.50,  \"b\" : [ 1,2 ,3 ],\r\n\"c\":\"\\u00e9\\/x\", \"d\": {}}\r\n\r\n";
        ParseError e;
        JDocument d = parseDocument(in, &e);
        QVERIFY(d.root);
        QCOMPARE(serialize(d), in);
        QCOMPARE(d.newline, QByteArray("\r\n"));
        QCOMPARE(resolve(d.root, QStringLiteral("/c"))->str, QStringLiteral("\u00e9/x"));
    }

    void oneKeyEditIsOneLine()
    {
        KblDocument doc;
        QVERIFY(doc.open(QDir(dataDir()).filePath(QStringLiteral("minimal-qwerty.kbl.json"))));
        const QByteArray before = doc.bytes();
        doc.setBinding(0, QStringLiteral("KeyQ"), QStringLiteral("text"), QStringLiteral("\u00e6"));
        const QStringList diff = changedLines(before, doc.bytes());
        QCOMPARE(diff.size(), 2);
        QVERIFY(diff[0].contains(QStringLiteral("\"KeyQ\": \"q\"")));
        QVERIFY(diff[1].contains(QStringLiteral("\"KeyQ\": \"\u00e6\"")));
        doc.undo();
        QCOMPARE(doc.bytes(), before);
        QVERIFY(!doc.dirty());
        doc.redo();
        QVERIFY(doc.dirty());
        QCOMPARE(changedLines(before, doc.bytes()).size(), 2);
    }

    void insertKeepsStyle()
    {
        KblDocument doc;
        QVERIFY(doc.open(QDir(dataDir()).filePath(QStringLiteral("minimal-qwerty.kbl.json"))));
        const QByteArray before = doc.bytes();
        // A new binding is appended to the layer's bindings in the file's style.
        doc.setBinding(0, QStringLiteral("Backquote"), QStringLiteral("text"), QStringLiteral("`"));
        QStringList diff = changedLines(before, doc.bytes());
        QCOMPARE(diff.size(), 3); // the last binding gains a comma, plus the new line
        QVERIFY(diff.last().startsWith(QStringLiteral("+          \"Backquote\": \"`\"")));
        doc.undo();
        QCOMPARE(doc.bytes(), before);

        // A value under a path that does not exist yet is written as new objects in the 2-space style.
        doc.setValueAt(QStringLiteral("/targets/klc/msklcEditorCompatible"), true);
        diff = changedLines(before, doc.bytes());
        QVERIFY2(diff.join(QLatin1Char('\n')).contains(QStringLiteral("+    \"klc\": {\n+      \"msklcEditorCompatible\": true")),
                 qPrintable(diff.join(QLatin1Char('\n'))));
        doc.undo();
        QCOMPARE(doc.bytes(), before);
    }

    void compactFileStaysCompact()
    {
        KblDocument doc;
        QVERIFY(doc.open(QDir(dataDir()).filePath(QStringLiteral("dll-import-shape.kbl.json"))));
        const QByteArray before = doc.bytes();
        doc.setBinding(0, QStringLiteral("KeyA"), QStringLiteral("text"), QStringLiteral("x"));
        QStringList diff = changedLines(before, doc.bytes());
        QCOMPARE(diff.size(), 2);
        QVERIFY(diff[1].contains(QStringLiteral("\"bindings\": { \"KeyA\": \"x\", \"KeyB\": \"b\" }")));
        doc.setBinding(0, QStringLiteral("KeyC"), QStringLiteral("text"), QStringLiteral("c"));
        diff = changedLines(before, doc.bytes());
        QCOMPARE(diff.size(), 2);
        QVERIFY(diff[1].contains(QStringLiteral("{ \"KeyA\": \"x\", \"KeyB\": \"b\", \"KeyC\": \"c\" }")));
        doc.undo();
        doc.undo();
        QCOMPARE(doc.bytes(), before);
    }

    void removeAndUndoRestoresBytes()
    {
        KblDocument doc;
        QVERIFY(doc.open(QDir(dataDir()).filePath(QStringLiteral("latin-altgr-deadkeys.kbl.json"))));
        const QByteArray before = doc.bytes();
        const QStringList ids = doc.deadKeyIds();
        QVERIFY(!ids.isEmpty());
        doc.removeDeadKey(ids.first());
        QVERIFY(doc.bytes() != before);
        QVERIFY(!doc.exists(QStringLiteral("/deadKeys/") + ids.first()));
        doc.undo();
        QCOMPARE(doc.bytes(), before);
    }

    void savedEditsRoundTrip()
    {
        // After an edit the written file parses again, and an unchanged re-save is byte-identical.
        KblDocument doc;
        QVERIFY(doc.open(QDir(dataDir()).filePath(QStringLiteral("windows-features.kbl.json"))));
        doc.setValueAt(QStringLiteral("/metadata/name"), QStringLiteral("Edited \"name\" \u2028 \x01"));
        const QByteArray out = doc.bytes();
        ParseError e;
        JDocument d = parseDocument(out, &e);
        QVERIFY2(d.root, qPrintable(e.message));
        QCOMPARE(resolve(d.root, QStringLiteral("/metadata/name"))->str, QStringLiteral("Edited \"name\" \u2028 \x01"));
        QVERIFY(out.contains("\"Edited \\\"name\\\" \\u2028 \\u0001\""));
        QCOMPARE(serialize(d), out);
    }

    void pointers()
    {
        QCOMPARE(escapePointerToken(QStringLiteral("a/b~c")), QStringLiteral("a~1b~0c"));
        QCOMPARE(splitPointer(QStringLiteral("/a~1b~0c/0")), (QStringList{QStringLiteral("a/b~c"), QStringLiteral("0")}));
        JPtr v = parseValue("{\"a/b\": [10, {\"~\": true}]}", nullptr);
        QCOMPARE(resolve(v, QStringLiteral("/a~1b/0"))->number, 10.0);
        QVERIFY(resolve(v, QStringLiteral("/a~1b/1/~0"))->boolean);
        QVERIFY(!resolve(v, QStringLiteral("/a~1b/01")));
        QVERIFY(!resolve(v, QStringLiteral("a")));
        QCOMPARE(resolve(v, QString()), v);
    }

    void errors_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<int>("line");
        QTest::addColumn<int>("column");
        QTest::newRow("eof") << QByteArray("{\n  \"a\": ") << 2 << 8;
        QTest::newRow("comma") << QByteArray("{\"a\": 1,}") << 1 << 9;
        QTest::newRow("control") << QByteArray("{\"a\": \"x\ty\"}") << 1 << 9;
        QTest::newRow("bad utf8") << QByteArray("{\"a\": \"\xff\"}") << 1 << 7;
        QTest::newRow("trailing") << QByteArray("{} x") << 1 << 4;
    }
    void errors()
    {
        QFETCH(QByteArray, text);
        QFETCH(int, line);
        QFETCH(int, column);
        ParseError e;
        QVERIFY(!parseDocument(text, &e).root);
        QCOMPARE(e.line, line);
        QCOMPARE(e.column, column);
        QVERIFY(!e.message.isEmpty());
    }

    void stringEscapes()
    {
        QCOMPARE(quoteString(QStringLiteral("a\"\\\n\x1b")), QByteArray("\"a\\\"\\\\\\n\\u001b\""));
        QCOMPARE(quoteString(QString::fromUtf16(u"\U0001F600")), QByteArray("\"\xF0\x9F\x98\x80\""));
        QCOMPARE(quoteString(QString(QChar(0xD800))), QByteArray("\"\\ud800\""));
        JPtr s = parseValue("\"\\ud83d\\ude00\"", nullptr);
        QCOMPARE(s->str, QString::fromUtf16(u"\U0001F600"));
    }
};

QTEST_GUILESS_MAIN(TestJsonTree)
#include "tst_jsontree.moc"
