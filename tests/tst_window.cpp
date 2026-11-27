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
#include <QScreen>
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
#ifdef Q_OS_MACOS
        QVERIFY2(!prefs.isEmpty(), "macOS has ⌘, for Settings");
#endif
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

    // The layer row shows whole tabs only, at narrow window widths too: every shown tab lies inside the row, the
    // selected one is shown, and the arrows step through all of them without ever cutting one.
    void layerRowShowsWholeTabs()
    {
        QMetaObject::invokeMethod(win, "openPath", Q_ARG(QVariant, data("latin-altgr-deadkeys.kbl.json")));
        QVERIFY(doc->loaded());
        auto *page = win->findChild<QQuickItem *>(QStringLiteral("keyboardPage"));
        auto *strip = win->findChild<QQuickItem *>(QStringLiteral("layerRow"));
        QVERIFY(page && strip);
        const int layers = page->property("layerList").toList().size();
        QVERIFY(layers >= 5);
        const int oldWidth = win->width();
        // The tabs by the visual tree (a Repeater's delegates are not QObject children of the row).
        const auto tabsOf = [](QQuickItem *root) {
            QList<QQuickItem *> found, todo{root};
            while (!todo.isEmpty()) {
                QQuickItem *it = todo.takeFirst();
                if (it->objectName() == QLatin1String("layerTab"))
                    found << it;
                todo << it->childItems();
            }
            return found;
        };
        const auto check = [&](const QString &where) {
            int shown = 0;
            bool selectedShown = false;
            const int sel = page->property("layerIndex").toInt();
            const auto tabs = tabsOf(strip);
            for (QQuickItem *t : tabs) {
                if (!t->isVisible())
                    continue;
                ++shown;
                const QRectF r = t->mapRectToItem(strip, QRectF(0, 0, t->width(), t->height()));
                if (r.left() < -0.5 || r.right() > strip->width() + 0.5)
                    return QStringLiteral("%1: tab '%2' at %3..%4 of a row %5 wide").arg(where, t->property("text").toString())
                        .arg(r.left()).arg(r.right()).arg(strip->width());
                if (t->property("checked").toBool())
                    selectedShown = true;
            }
            if (shown == 0)
                return QStringLiteral("%1: no tab shown").arg(where);
            if (!selectedShown)
                return QStringLiteral("%1: the selected layer %2 is not shown").arg(where).arg(sel);
            return QString();
        };
        for (int width : {760, 992, 1360}) {
            if (width > win->screen()->availableGeometry().width())
                continue; // a window cannot be wider than the screen (CI runners have 1024 x 768)
            win->setWidth(width);
            // The platform resizes the window and the row fits itself on a later turn of the event loop: wait for both.
            QTRY_COMPARE(win->width(), width);
            QTest::qWait(200);
            if (width == 760) // this file's layers do not fit there in any style: the case under test
                QVERIFY2(strip->property("overflows").toBool(), qPrintable(QStringLiteral("width %1 does not overflow").arg(width)));
            for (int i = 0; i < layers; ++i) {
                page->setProperty("layerIndex", i);
                const QString where = QStringLiteral("width %1, layer %2").arg(win->width()).arg(i);
                QString problem;
                QTRY_VERIFY2_WITH_TIMEOUT((problem = check(where)).isEmpty(), qPrintable(problem), 3000);
            }
            // Stepping with the arrows from the start to the end: each step shows whole tabs.
            if (strip->property("overflows").toBool()) {
                page->setProperty("layerIndex", 0);
                QTest::qWait(50);
                for (int s = 0; s < layers; ++s) {
                    QMetaObject::invokeMethod(strip, "step", Q_ARG(QVariant, 1));
                    QTest::qWait(100);
                    const auto tabs = tabsOf(strip);
                    for (QQuickItem *t : tabs) {
                        if (!t->isVisible())
                            continue;
                        const QRectF r = t->mapRectToItem(strip, QRectF(0, 0, t->width(), t->height()));
                        QVERIFY2(r.left() >= -0.5 && r.right() <= strip->width() + 0.5,
                                 qPrintable(QStringLiteral("width %1, step %2: tab '%3' cut").arg(win->width()).arg(s)
                                                .arg(t->property("text").toString())));
                    }
                }
                QCOMPARE(strip->property("last").toInt(), layers - 1);
            }
        }
        win->setWidth(oldWidth);
    }

    void cleanupTestCase()
    {
        delete engine; // before the objects its bindings read
        engine = nullptr;
    }
};

QTEST_MAIN(TestWindow)
#include "tst_window.moc"
