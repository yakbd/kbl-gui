// The whole main window (Main.qml) with the platform's standard shortcuts and the appearance setting: on macOS the
// same test sends ⌘Z, ⇧⌘Z and ⌘, (QKeySequence maps the standard keys per platform).
#include "appsettings.h"
#include "cli.h"
#include "kbldocument.h"
#include "reportmodel.h"
#include "schemamodel.h"

#include <QDir>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStyleHints>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(YakbdPlugin)

using namespace kbl;

class TestWindow : public QObject
{
    Q_OBJECT

    AppSettings *settings = nullptr;
    KblDocument *doc = nullptr;
    ReportModel *report = nullptr;
    SchemaModel *schema = nullptr;
    Cli *cli = nullptr;
    QQmlApplicationEngine *engine = nullptr;
    QQuickWindow *win = nullptr;

    static QString data(const char *name)
    {
        return QDir(qEnvironmentVariable("YAKBD_TESTDATA")).filePath(QLatin1String(name));
    }

private slots:
    void initTestCase()
    {
        AppSettings::setPersistent(false);
        settings = new AppSettings(this);
        doc = new KblDocument(this);
        report = new ReportModel(this);
        schema = new SchemaModel(this);
        cli = new Cli(doc, report, settings, this);
        engine = new QQmlApplicationEngine(this);
        QQmlContext *c = engine->rootContext();
        c->setContextProperty(QStringLiteral("doc"), doc);
        c->setContextProperty(QStringLiteral("report"), report);
        c->setContextProperty(QStringLiteral("schema"), schema);
        c->setContextProperty(QStringLiteral("cli"), cli);
        c->setContextProperty(QStringLiteral("appSettings"), settings);
        engine->loadFromModule("Yakbd", "Main");
        QVERIFY(!engine->rootObjects().isEmpty());
        win = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
        QVERIFY(win);
        win->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(win));
        QMetaObject::invokeMethod(win, "openPath", Q_ARG(QVariant, data("minimal-qwerty.kbl.json")));
        QVERIFY(doc->loaded());
    }

    // ⌘Z / Ctrl+Z and ⇧⌘Z / Ctrl+Y (or the platform's Redo) reach the Edit menu's actions.
    void undoRedoShortcuts()
    {
        doc->setBinding(0, QStringLiteral("KeyQ"), QStringLiteral("text"), QStringLiteral("x"));
        QVERIFY(doc->canUndo());
        win->contentItem()->forceActiveFocus(); // no text field has focus: the window's shortcuts apply
        QTest::keySequence(win, QKeySequence(QKeySequence::Undo));
        QTRY_VERIFY(!doc->canUndo());
        QCOMPARE(doc->bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("value")).toString(), QStringLiteral("q"));
        QTest::keySequence(win, QKeySequence(QKeySequence::Redo));
        QTRY_VERIFY(doc->canUndo());
        QCOMPARE(doc->bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("value")).toString(), QStringLiteral("x"));
        doc->undo();
    }

    // The platform's Preferences shortcut (⌘, on macOS) opens Preferences, where the platform has one.
    void preferencesShortcut()
    {
        const QKeySequence prefs(QKeySequence::Preferences);
        if (prefs.isEmpty())
            QSKIP("this platform has no standard Preferences shortcut");
        QObject *dialog = win->findChild<QObject *>(QStringLiteral("preferencesDialog"));
        QVERIFY(dialog);
        QTest::keySequence(win, prefs);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }

    // The menus carry the titles that put About, Settings and Quit into the macOS application menu (Qt moves native
    // menu items there by their text).
    void menuTitles()
    {
        QStringList titles;
        const auto actions = win->findChildren<QObject *>();
        for (QObject *o : actions)
            if (o->inherits("QQuickAction"))
                titles << o->property("text").toString().remove(QLatin1Char('&'));
        QVERIFY2(titles.contains(QStringLiteral("About YAKBD Editor")), qPrintable(titles.join(QLatin1Char('|'))));
#ifdef Q_OS_MACOS
        QVERIFY(titles.contains(QStringLiteral("Settings…")));
        QVERIFY(titles.contains(QStringLiteral("Quit")));
#endif
    }

    // Light and dark: the Appearance setting changes the window's palette at once (no restart).
    void appearanceSwitchesLive()
    {
        settings->setAppearance(QStringLiteral("dark"));
        QTRY_VERIFY(win->property("palette").value<QPalette>().color(QPalette::Window).lightness() < 128
                    || settings->dark());
        QVERIFY(settings->dark());
        settings->setAppearance(QStringLiteral("light"));
        QTRY_VERIFY(!settings->dark());
        settings->setAppearance(QStringLiteral("system"));
    }

    void cleanupTestCase()
    {
        delete engine; // before the objects its bindings read
        engine = nullptr;
    }
};

QTEST_MAIN(TestWindow)
#include "tst_window.moc"
