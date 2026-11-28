// The editor with the released yakbd CLI: open, edit, save, reopen, validate; import, pin, build.
#include "appsettings.h"
#include "cli.h"
#include "kbldocument.h"
#include "reportmodel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <functional>

using namespace kbl;

static QString data(const char *name)
{
    return QDir(qEnvironmentVariable("YAKBD_TESTDATA")).filePath(QLatin1String(name));
}

class TestCli : public QObject
{
    Q_OBJECT

    QString program;

    // Runs one Cli operation to the end; returns the finished() arguments.
    QList<QVariant> run(Cli &cli, const std::function<bool()> &start)
    {
        QSignalSpy spy(&cli, &Cli::finished);
        if (!start())
            return {};
        if (!spy.wait(60000))
            return {};
        return spy.takeFirst();
    }

private slots:
    void initTestCase()
    {
        AppSettings::setPersistent(false);
        program = qEnvironmentVariable("YAKBD_CLI");
        if (program.isEmpty() || !QFileInfo(program).isExecutable())
        {
            if (qEnvironmentVariableIsSet("YAKBD_REQUIRE_CLI"))
                QFAIL("no yakbd CLI, and YAKBD_REQUIRE_CLI is set");
            QSKIP("no yakbd CLI (set YAKBD_CLI or configure with network access)");
        }
    }

    void locate()
    {
        KblDocument doc;
        ReportModel report;
        Cli cli(&doc, &report, nullptr);
        cli.locate();
        QVERIFY2(cli.available(), qPrintable(cli.problem()));
        QVERIFY(cli.version().startsWith(QLatin1String("0.")));
    }

    // The discriminating test: open a layout, edit a key, save, reopen, and the released CLI validates it.
    void editSaveReopenValidate()
    {
        QTemporaryDir tmp;
        const QString out = tmp.filePath(QStringLiteral("edited.kbl.json"));
        {
            KblDocument doc;
            QVERIFY(doc.open(data("latin-altgr-deadkeys.kbl.json")));
            doc.setBinding(0, QStringLiteral("KeyQ"), QStringLiteral("text"), QStringLiteral("æ"));
            doc.setBinding(1, QStringLiteral("KeyQ"), QStringLiteral("text"), QStringLiteral("Æ"));
            doc.setBinding(4, QStringLiteral("KeyW"), QStringLiteral("deadKey"), QStringLiteral("grave"));
            QVERIFY(doc.dirty());
            QVERIFY(doc.saveAs(out));
            QVERIFY(!doc.dirty());
        }
        KblDocument again;
        QVERIFY(again.open(out));
        QCOMPARE(again.bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("value")).toString(), QStringLiteral("æ"));
        QCOMPARE(again.bindingAt(4, QStringLiteral("KeyW")).value(QStringLiteral("kind")).toString(), QStringLiteral("deadKey"));

        // yakbd validate, run directly
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(program, {QStringLiteral("validate"), QStringLiteral("-windows"), out});
        QVERIFY(p.waitForFinished(60000));
        QVERIFY2(p.exitCode() == 0, p.readAll().constData());

        // and through the editor's Cli, which fills the report
        ReportModel report;
        Cli cli(&again, &report, nullptr);
        cli.locate();
        const QList<QVariant> r = run(cli, [&] { return cli.validate(); });
        QCOMPARE(r.value(0).toString(), QStringLiteral("validate"));
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QCOMPARE(report.errorCount(), 0);
        QVERIFY(report.defaultCount() > 0);
    }

    void problemsPointIntoTheDocument()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        QVERIFY(doc.setJsonAt(QStringLiteral("/hardware/layers/0/id"), QStringLiteral("5")).isEmpty());
        ReportModel report;
        Cli cli(&doc, &report, nullptr);
        cli.locate();
        const QList<QVariant> r = run(cli, [&] { return cli.validate(); });
        QVERIFY(!r.value(1).toBool());
        QVERIFY(report.errorCount() >= 1);
        bool found = false;
        for (const ReportEntry &e : report.entries())
            if (e.pointer == QLatin1String("/hardware/layers/0/id"))
                found = true;
        QVERIFY(found);
    }

    void importKlc()
    {
        QTemporaryDir tmp;
        const QString klc = tmp.filePath(QStringLiteral("sample.klc"));
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(program, {QStringLiteral("convert"), QStringLiteral("-q"), data("minimal-qwerty.kbl.json"), klc});
        QVERIFY(p.waitForFinished(60000));
        QVERIFY2(p.exitCode() == 0, p.readAll().constData());

        KblDocument doc;
        ReportModel report;
        Cli cli(&doc, &report, nullptr);
        cli.locate();
        QVERIFY(Cli::needsImport(klc));
        const QList<QVariant> r = run(cli, [&] { return cli.importLayout(klc); });
        QCOMPARE(r.value(0).toString(), QStringLiteral("import"));
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QVERIFY(doc.loaded());
        QVERIFY(doc.dirty());
        QVERIFY(doc.filePath().isEmpty());
        QCOMPARE(doc.bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("value")).toString(), QStringLiteral("q"));
        QCOMPARE(report.source(), QStringLiteral("import"));
        for (const ReportEntry &e : report.entries())
            QVERIFY(e.pointer.isEmpty()); // import locations are in the .klc file
    }

    // CLDR keyboard XML and a layout DLL open through convert as well (both made here by the CLI).
    void importCldrAndDll()
    {
        QTemporaryDir tmp;
        const QString xml = tmp.filePath(QStringLiteral("pt-abnt2.xml"));
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(program, {QStringLiteral("convert"), QStringLiteral("-q"), data("cldr-pt-abnt2.kbl.json"), xml});
        QVERIFY(p.waitForFinished(60000));
        QVERIFY2(p.exitCode() == 0, p.readAll().constData());
        p.start(program, {QStringLiteral("compile"), QStringLiteral("-q"), QStringLiteral("-target"), QStringLiteral("x64"),
                          QStringLiteral("-o"), tmp.filePath(QStringLiteral("dll")), data("minimal-qwerty.kbl.json")});
        QVERIFY(p.waitForFinished(60000));
        QVERIFY2(p.exitCode() == 0, p.readAll().constData());
        QDirIterator it(tmp.filePath(QStringLiteral("dll")), {QStringLiteral("*.dll")}, QDir::Files, QDirIterator::Subdirectories);
        QVERIFY(it.hasNext());
        const QString dll = it.next();

        for (const QString &source : {xml, dll}) {
            KblDocument doc;
            ReportModel report;
            Cli cli(&doc, &report, nullptr);
            cli.locate();
            QVERIFY(Cli::needsImport(source));
            const QList<QVariant> r = run(cli, [&] { return cli.importLayout(source); });
            QVERIFY2(r.value(1).toBool(), qPrintable(source + QStringLiteral(": ") + r.value(2).toString()));
            QVERIFY(doc.loaded());
            QVERIFY(doc.dirty());
            QVERIFY(!doc.layers().isEmpty());
            QVERIFY(!doc.bindingAt(0, QStringLiteral("KeyA")).value(QStringLiteral("value")).toString().isEmpty());
        }
    }

    void pinBuildInstaller()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        ReportModel report;
        Cli cli(&doc, &report, nullptr);
        cli.locate();
        QList<QVariant> r = run(cli, [&] { return cli.pinDefaults(); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QVERIFY(doc.exists(QStringLiteral("/targets/windows/dllName")));
        QCOMPARE(doc.undoText(), QStringLiteral("Pin defaults"));
        // Every default choice is in the file now: validate with -strict-defaults (defaults are errors) passes.
        {
            QTemporaryDir t;
            const QString pinned = t.filePath(QStringLiteral("pinned.kbl.json"));
            QVERIFY(doc.saveAs(pinned));
            QProcess p;
            p.setProcessChannelMode(QProcess::MergedChannels);
            p.start(program, {QStringLiteral("validate"), QStringLiteral("-windows"), QStringLiteral("-strict-defaults"), pinned});
            QVERIFY(p.waitForFinished(60000));
            QVERIFY2(p.exitCode() == 0, p.readAll().constData());
        }

        QTemporaryDir tmp;
        r = run(cli, [&] { return cli.compile(tmp.filePath(QStringLiteral("dlls"))); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QStringList dlls;
        QDirIterator it(tmp.filePath(QStringLiteral("dlls")), {QStringLiteral("*.dll")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            dlls << it.next();
        QVERIFY2(dlls.size() >= 3, qPrintable(dlls.join(QLatin1Char(' '))));

        const QString setup = tmp.filePath(QStringLiteral("layout-setup.exe"));
        r = run(cli, [&] { return cli.buildInstaller(setup); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QVERIFY(QFileInfo(setup).size() > 10000);
    }

    // The target picker: a CLI with targets lists them; validate, pin, compile and installer run for the picked one.
    // A CLI from before targets (the pinned 0.1.0-preview.3) offers Windows only and gets the old command lines.
    void targets()
    {
        KblDocument doc;
        QVERIFY(doc.open(data("minimal-qwerty.kbl.json")));
        const QByteArray original = doc.bytes();
        ReportModel report;
        Cli cli(&doc, &report, nullptr);
        cli.locate();
        QStringList names;
        for (const QVariant &t : cli.targets())
            names << t.toMap().value(QStringLiteral("name")).toString();
        QCOMPARE(names.value(0), QStringLiteral("windows"));
        cli.setTarget(QStringLiteral("macos"));
        if (!names.contains(QLatin1String("macos"))) {
            QCOMPARE(cli.target(), QStringLiteral("windows")); // an unknown pick falls back to Windows
            QSKIP("this yakbd has no macos target");
        }
        QCOMPARE(cli.target(), QStringLiteral("macos"));
        QCOMPARE(cli.targetInfo().value(QStringLiteral("installerSuffix")).toString(), QStringLiteral(".zip"));
        QCOMPARE(cli.targetLabel(QStringLiteral("macos")), QStringLiteral("macOS"));
        for (const QVariant &t : cli.targets()) // the XKB target hands out a .tar.gz archive
            if (t.toMap().value(QStringLiteral("name")) == QLatin1String("xkb"))
                QCOMPARE(t.toMap().value(QStringLiteral("installerSuffix")).toString(), QStringLiteral(".tar.gz"));

        QList<QVariant> r = run(cli, [&] { return cli.validate(); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QCOMPARE(cli.reportTarget(), QStringLiteral("macos"));
        QVERIFY(cli.log().contains(QLatin1String("validate -target macos")));
        bool macosDefault = false;
        for (const ReportEntry &e : report.entries())
            macosDefault = macosDefault || e.kind.startsWith(QLatin1String("macosDefault."));
        QVERIFY(macosDefault);

        QTemporaryDir tmp;
        r = run(cli, [&] { return cli.compile(tmp.filePath(QStringLiteral("out"))); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QStringList bundles = QDir(tmp.filePath(QStringLiteral("out"))).entryList({QStringLiteral("*.bundle")}, QDir::Dirs);
        QCOMPARE(bundles.size(), 1);
        const QString zip = tmp.filePath(QStringLiteral("layout.zip"));
        r = run(cli, [&] { return cli.buildInstaller(zip); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QVERIFY(QFileInfo(zip).size() > 100);

        // Pin writes targets.macos into the 0.1 file, which becomes 0.2 in the same undo step.
        r = run(cli, [&] { return cli.pinDefaults(); });
        QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
        QVERIFY(doc.exists(QStringLiteral("/targets/macos")));
        QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));
        QCOMPARE(doc.undoText(), QStringLiteral("Pin macOS defaults"));
        doc.undo();
        QCOMPARE(doc.bytes(), original);

        // The same for Linux (XKB), when this yakbd has it.
        if (names.contains(QLatin1String("xkb"))) {
            cli.setTarget(QStringLiteral("xkb"));
            r = run(cli, [&] { return cli.pinDefaults(); });
            QVERIFY2(r.value(1).toBool(), qPrintable(r.value(2).toString()));
            QVERIFY(doc.exists(QStringLiteral("/targets/xkb")));
            QCOMPARE(doc.formatVersion(), QStringLiteral("0.2"));
            QCOMPARE(doc.undoText(), QStringLiteral("Pin Linux (XKB) defaults"));
            doc.undo();
            QCOMPARE(doc.bytes(), original);
        }
    }
};

QTEST_GUILESS_MAIN(TestCli)
#include "tst_cli.moc"
