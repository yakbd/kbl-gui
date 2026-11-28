// The editor's views and edits of a layout, without the CLI.
#include "kbldocument.h"
#include "reportmodel.h"
#include "schemamodel.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTest>

using namespace kbl;

static QString data(const char *name)
{
    return QDir(qEnvironmentVariable("YAKBD_TESTDATA")).filePath(QLatin1String(name));
}

static QStringList ids(const QVariantList &row)
{
    QStringList out;
    for (const QVariant &k : row)
        out << k.toMap().value(QStringLiteral("id")).toString();
    return out;
}

class TestDocument : public QObject
{
    Q_OBJECT
private slots:
    void layersAndKeyboard()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("latin-altgr-deadkeys.kbl.json")));
        const QVariantList layers = doc.layers();
        QCOMPARE(layers.size(), 8);
        QCOMPARE(layers[0].toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Base"));
        QCOMPARE(layers[5].toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Shift+AltR"));

        const QVariantList rows = doc.keyboardRows(0);
        QVERIFY(rows.size() >= 5);
        bool sawQ = false;
        for (const QVariant &r : rows)
            for (const QVariant &k : r.toList()) {
                const QVariantMap m = k.toMap();
                if (m.value(QStringLiteral("id")) == QLatin1String("KeyQ")) {
                    sawQ = true;
                    QCOMPARE(m.value(QStringLiteral("label")).toString(), QStringLiteral("q"));
                    QCOMPARE(m.value(QStringLiteral("kind")).toString(), QStringLiteral("text"));
                }
            }
        QVERIFY(sawQ);
        // The form "us" puts Backslash at the end of the Q row (SPEC Appendix A); Tab is added in front.
        const QStringList qRow = ids(rows[2].toList());
        QCOMPARE(qRow.first(), QStringLiteral("Tab"));
        QVERIFY(qRow.contains(QStringLiteral("Backslash")));
    }

    void bindingsAndFallback()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        // Layer 2 is Caps Lock; digits are not bound there, so they come from the base layer (SPEC §9.3).
        const QVariantMap digit = doc.bindingAt(2, QStringLiteral("Digit1"));
        QCOMPARE(digit.value(QStringLiteral("kind")).toString(), QStringLiteral("unbound"));
        QCOMPARE(digit.value(QStringLiteral("fallback")).toString(), QStringLiteral("1"));
        QCOMPARE(doc.bindingAt(0, QStringLiteral("Backspace")).value(QStringLiteral("kind")).toString(), QStringLiteral("action"));
        QCOMPARE(doc.bindingAt(0, QStringLiteral("Escape")).value(QStringLiteral("display")).toString(), QStringLiteral("Esc"));

        doc.setBinding(0, QStringLiteral("KeyQ"), QStringLiteral("nothing"), {});
        QCOMPARE(doc.bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("kind")).toString(), QStringLiteral("nothing"));
        doc.setBinding(0, QStringLiteral("KeyQ"), QStringLiteral("unbound"), {});
        QVERIFY(!doc.exists(QStringLiteral("/hardware/layers/0/bindings/KeyQ")));
        doc.undo();
        doc.undo();
        QCOMPARE(doc.bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("value")).toString(), QStringLiteral("q"));
        QVERIFY(!doc.dirty());
    }

    void deadKeys()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("latin-altgr-deadkeys.kbl.json")));
        QVERIFY(doc.deadKeyIds().contains(QStringLiteral("acute")));
        const QVariantList entries = doc.deadKeyEntries(QStringLiteral("acute"));
        QVERIFY(!entries.isEmpty());
        const int n = entries.size();
        doc.addDeadKeyEntry(QStringLiteral("acute"), QStringLiteral("w"), QStringLiteral("ẃ"));
        QCOMPARE(doc.deadKeyEntries(QStringLiteral("acute")).size(), n + 1);
        doc.setDeadKeyEntry(QStringLiteral("acute"), n, QStringLiteral("W"), QStringLiteral("Ẃ"));
        QCOMPARE(doc.deadKeyEntries(QStringLiteral("acute")).last().toMap().value(QStringLiteral("result")).toString(),
                 QStringLiteral("Ẃ"));
        doc.removeDeadKeyEntry(QStringLiteral("acute"), n);
        QCOMPARE(doc.deadKeyEntries(QStringLiteral("acute")).size(), n);

        QVERIFY(!doc.addDeadKey(QStringLiteral("acute"), QStringLiteral("x")).isEmpty()); // exists
        QVERIFY(!doc.addDeadKey(QStringLiteral("a b"), QStringLiteral("x")).isEmpty());   // not an id
        QVERIFY(doc.addDeadKey(QStringLiteral("ring"), QStringLiteral("˚")).isEmpty());
        doc.setBinding(5, QStringLiteral("KeyR"), QStringLiteral("deadKey"), QStringLiteral("ring"));
        QCOMPARE(doc.bindingAt(5, QStringLiteral("KeyR")).value(QStringLiteral("kind")).toString(), QStringLiteral("deadKey"));
        QCOMPARE(doc.bindingAt(5, QStringLiteral("KeyR")).value(QStringLiteral("display")).toString(), QStringLiteral("˚"));
    }

    void mergePinnedValues()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        const QByteArray before = doc.bytes();
        QFile f(data("pinned-minimal-qwerty.kbl.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        JPtr pinned = parseValue(f.readAll(), nullptr);
        QVERIFY(pinned);
        const int n = doc.mergeMissing(pinned, QStringLiteral("Pin defaults"));
        QVERIFY(n > 0);
        QVERIFY(doc.exists(QStringLiteral("/targets/windows/dllName")));
        QCOMPARE(doc.valueAt(QStringLiteral("/targets/windows/registration/langId")).toString(), QStringLiteral("0409"));
        // Every line that was there is still there: the only removed lines are ones that gained a comma.
        const QList<QByteArray> oldLines = before.split('\n');
        const QList<QByteArray> newLines = doc.bytes().split('\n');
        int j = 0;
        for (const QByteArray &l : oldLines) {
            while (j < newLines.size() && newLines[j] != l && newLines[j] != l + ',')
                ++j;
            QVERIFY2(j < newLines.size(), l.constData());
            ++j;
        }
        QCOMPARE(doc.undoText(), QStringLiteral("Pin defaults"));
        doc.undo();
        QCOMPARE(doc.bytes(), before);
        QCOMPARE(doc.mergeMissing(parseValue(doc.bytes(), nullptr), QStringLiteral("x")), 0);
    }

    void jsonEditing()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        QVERIFY(!doc.setJsonAt(QStringLiteral("/settings"), QStringLiteral("{\"capsLockFallback\": ")).isEmpty());
        QVERIFY(doc.setJsonAt(QStringLiteral("/settings"), QStringLiteral("{\"capsLockFallback\": false}")).isEmpty());
        QCOMPARE(doc.valueAt(QStringLiteral("/settings/capsLockFallback")).toBool(), false);
        QVERIFY(doc.bindingAt(2, QStringLiteral("Digit1")).value(QStringLiteral("fallback")).toString().isEmpty());
    }

    void newDocument()
    {
        KblDocument doc;
        doc.newDocument();
        QVERIFY(doc.loaded());
        QVERIFY(doc.dirty());
        QVERIFY(doc.filePath().isEmpty());
        QCOMPARE(doc.valueAt(QStringLiteral("/kbl")).toString(), QStringLiteral("0.1"));
    }

    void schemaVersions()
    {
        SchemaModel s;
        QCOMPARE(s.versions(), (QStringList{QStringLiteral("0.1"), QStringLiteral("0.2")}));
        s.setVersion(QStringLiteral("0.1"));
        QVERIFY(s.schemaId().contains(QLatin1String("/schema/0.1/")));
        QCOMPARE(s.versionFor(QStringLiteral("/metadata/name")), QStringLiteral("0.1"));
        QCOMPARE(s.versionFor(QStringLiteral("/targets/macos")), QStringLiteral("0.2"));
        QCOMPARE(s.versionFor(QStringLiteral("/targets/xkb/symbolsName")), QStringLiteral("0.2"));
        // A 0.1 file still gets the forms of the 0.2-only targets (from the 0.2 schema).
        QVERIFY(!s.fields(s.schemaFor(QStringLiteral("/targets/macos"))).isEmpty());
        QVERIFY(!s.fields(s.schemaFor(QStringLiteral("/targets/xkb"))).isEmpty());
        QVERIFY(!s.describe(QStringLiteral("/targets/macos/layoutName")).isEmpty());
        s.setVersion(QStringLiteral("0.2"));
        QVERIFY(s.schemaId().contains(QLatin1String("/schema/0.2/")));
    }

    // The 0.2-only members the document knows (SPEC §1.1) are the targets the 0.2 schema adds.
    void version02MembersMatchTheSchemas()
    {
        auto targets = [](const char *v) {
            QFile f(QStringLiteral(":/qt/qml/Yakbd/schema/%1/kbl.schema.json").arg(QLatin1String(v)));
            if (!f.open(QIODevice::ReadOnly))
                return QStringList();
            ParseError e;
            JPtr root = parseValue(f.readAll(), &e);
            QStringList out;
            if (JPtr p = resolve(root, QStringLiteral("/properties/targets/properties")))
                for (const auto &m : p->members)
                    out << QStringLiteral("/targets/") + m.key;
            return out;
        };
        const QStringList v1 = targets("0.1"), v2 = targets("0.2");
        QVERIFY(!v1.isEmpty());
        QStringList added;
        for (const QString &t : v2)
            if (!v1.contains(t))
                added << t;
        QCOMPARE(added, KblDocument::kVersion02Members);
    }

    void editsKeepTheLowestVersion()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        const QByteArray original = doc.bytes();
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.1"));
        // The first 0.2-only member raises kbl and $schema, in the same undo step.
        doc.setValueAt(QStringLiteral("/targets/macos/layoutName"), QStringLiteral("Demo"), QStringLiteral("Set layoutName"));
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));
        QVERIFY(doc.valueAt(QStringLiteral("/$schema")).toString().endsWith(QLatin1String("/schema/0.2/kbl.schema.json")));
        QCOMPARE(doc.undoText(), QStringLiteral("Set layoutName"));
        doc.undo();
        QCOMPARE(doc.bytes(), original);
        doc.redo();
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));
        // An unrelated edit keeps it; removing the last 0.2-only member lowers it again.
        doc.setValueAt(QStringLiteral("/metadata/name"), QStringLiteral("Renamed"));
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));
        doc.removeAt(QStringLiteral("/targets/macos"));
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.1"));
        QVERIFY(doc.valueAt(QStringLiteral("/$schema")).toString().endsWith(QLatin1String("/schema/0.1/kbl.schema.json")));
        // A loss kind of a 0.2-only family in an onLoss policy needs 0.2 as well.
        doc.setJsonAt(QStringLiteral("/onLoss"), QStringLiteral(R"({"kinds": {"xkb.groupDropped": "error"}})"));
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));
    }

    void versionOnlyChangesByEdits()
    {
        // A 0.2 file without 0.2-only members opens, saves and takes unrelated edits as 0.2 (a user may ask for a
        // higher version; the editor only lowers it when an edit removes the last 0.2-only member).
        QFile f(data("minimal-qwerty.kbl.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray bytes = f.readAll().replace("\"kbl\": \"0.1\"", "\"kbl\": \"0.2\"");
        KblDocument doc;
        QVERIFY(doc.loadBytes(bytes, QStringLiteral("x.kbl.json"), true));
        QCOMPARE(doc.bytes(), bytes);
        doc.setValueAt(QStringLiteral("/metadata/name"), QStringLiteral("Renamed"));
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));

        // A 0.2 example opens and serialises byte for byte; dropping its macOS target makes it 0.1.
        KblDocument mac;
        QVERIFY(mac.open(data("macos-option-deadkeys.kbl.json")));
        QFile m(data("macos-option-deadkeys.kbl.json"));
        QVERIFY(m.open(QIODevice::ReadOnly));
        QCOMPARE(mac.bytes(), m.readAll());
        QCOMPARE(mac.formatVersion(), QStringLiteral("0.2"));
        mac.removeAt(QStringLiteral("/targets/macos"));
        QCOMPARE(mac.formatVersion(), QStringLiteral("0.1"));

        // Pinning (mergeMissing) a 0.2-only target into a 0.1 file raises the version in the same step.
        KblDocument pin;
        QVERIFY(pin.open(data("minimal-qwerty.kbl.json")));
        const QByteArray before = pin.bytes();
        ParseError e;
        JPtr other = parseValue(R"({"targets": {"xkb": {"symbolsName": "demo"}}})", &e);
        QCOMPARE(pin.mergeMissing(other, QStringLiteral("Pin defaults")), 1);
        QCOMPARE(pin.formatVersion(), QStringLiteral("0.2"));
        pin.undo();
        QCOMPARE(pin.bytes(), before);
    }

    void schemaFields()
    {
        SchemaModel s;
        const QVariantList meta = s.fields(QStringLiteral("/properties/metadata"));
        QVERIFY(meta.size() > 10);
        QCOMPARE(meta[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("id")); // schema order
        QVERIFY(meta[0].toMap().value(QStringLiteral("required")).toBool());
        QCOMPARE(meta[0].toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("string"));
        QString nameKind;
        for (const QVariant &f : meta)
            if (f.toMap().value(QStringLiteral("name")) == QLatin1String("name"))
                nameKind = f.toMap().value(QStringLiteral("kind")).toString();
        QCOMPARE(nameKind, QStringLiteral("string")); // a $ref to Text
        const QVariantList win = s.fields(s.schemaFor(QStringLiteral("/targets/windows")));
        QVariantMap builds, localeFlags, dllName;
        for (const QVariant &f : win) {
            const QVariantMap m = f.toMap();
            if (m.value(QStringLiteral("name")) == QLatin1String("builds"))
                builds = m;
            if (m.value(QStringLiteral("name")) == QLatin1String("localeFlags"))
                localeFlags = m;
            if (m.value(QStringLiteral("name")) == QLatin1String("dllName"))
                dllName = m;
        }
        QCOMPARE(builds.value(QStringLiteral("kind")).toString(), QStringLiteral("enumSet"));
        QCOMPARE(localeFlags.value(QStringLiteral("kind")).toString(), QStringLiteral("object"));
        QVERIFY(!dllName.value(QStringLiteral("pattern")).toString().isEmpty());
        QVERIFY(!s.describe(QStringLiteral("/settings/capsLockFallback")).isEmpty());
    }

    void reportPointers()
    {
        const QByteArray json = R"({"entries":[
          {"class":"diagnostic","severity":"error","kind":"kbl.schema","where":{"file":"a.kbl.json","path":"/hardware/layers/0/id"},"message":"m1"},
          {"class":"defaulted","severity":"accept","kind":"windowsDefault.dllName","where":{"path":"/targets/windows"},"message":"m2","detail":{"value":"x"}},
          {"class":"loss","severity":"warning","kind":"general.touchDropped","where":{"path":"DLL"},"message":"m3"},
          {"class":"diagnostic","severity":"error","kind":"kbl.json","where":{"file":"other.klc","line":4},"message":"m4"}]})";
        ReportModel r;
        QVERIFY(r.load(json, QStringLiteral("validate"), true, QStringLiteral("a.kbl.json")));
        QCOMPARE(r.errorCount(), 2);
        QCOMPARE(r.warningCount(), 1);
        QCOMPARE(r.defaultCount(), 1);
        QCOMPARE(r.lossCount(), 1);
        const auto &e = r.entries();
        QCOMPARE(e[0].pointer, QStringLiteral("/hardware/layers/0/id"));
        QCOMPARE(e[1].pointer, QStringLiteral("/targets/windows/dllName"));
        QVERIFY(e[2].pointer.isEmpty()); // a label, not a pointer
        QVERIFY(e[3].pointer.isEmpty()); // another file
        r.setFilter(QStringLiteral("problems"));
        QCOMPARE(r.rowCount(), 3);
        r.setFilter(QStringLiteral("defaults"));
        QCOMPARE(r.rowCount(), 1);
        QCOMPARE(r.data(r.index(0), ReportModel::DetailRole).toString(), QStringLiteral("{\"value\":\"x\"}"));
    }

    // Report format 1 (yakbd 0.1.0-preview.2, docs/report-file.md): pointers only in where.pointer.
    void reportFormat1()
    {
        const QByteArray json = R"({"format":1,"tool":{"name":"yakbd","version":"0.1.0-preview.2"},"command":"validate",
          "input":{"file":"x.kbl.json","format":"kbl.json"},"entries":[
          {"class":"defaulted","severity":"accept","kind":"windowsDefault.dllName","where":{"pointer":"/targets/windows/dllName","missing":true},"message":"m1","detail":{"value":"x"}},
          {"class":"loss","severity":"warning","kind":"windows.x","where":{"path":"/looks/like/a/pointer"},"message":"m2"},
          {"class":"diagnostic","severity":"error","kind":"kbl.schema","where":{"pointer":"/hardware/layers/0/bindings/a~1b"},"message":"m3"}]})";
        ReportModel r;
        QVERIFY(r.load(json, QStringLiteral("validate"), true, QStringLiteral("other-name.kbl.json")));
        QCOMPARE(r.toolVersion(), QStringLiteral("0.1.0-preview.2"));
        const auto &e = r.entries();
        QCOMPARE(e[0].pointer, QStringLiteral("/targets/windows/dllName"));
        QVERIFY(e[0].missing);
        QVERIFY(e[1].pointer.isEmpty()); // where.path never points into the document in format 1
        QCOMPARE(splitPointer(e[2].pointer).last(), QStringLiteral("a/b"));

        // A conversion report (input .klc): nothing points into the open document.
        const QByteArray klc = R"({"format":1,"input":{"file":"a.klc","format":"klc"},"entries":[
          {"class":"loss","severity":"accept","kind":"klc.impliedRows","where":{"path":"/hardware/layers/0","file":"a.klc","line":3},"message":"m"}]})";
        QVERIFY(r.load(klc, QStringLiteral("import"), true, QStringLiteral("a.kbl.json")));
        QVERIFY(r.entries()[0].pointer.isEmpty());
    }
};

QTEST_MAIN(TestDocument)
#include "tst_document.moc"
