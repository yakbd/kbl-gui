#include "appsettings.h"
#include "cli.h"
#include "kbldocument.h"
#include "reportmodel.h"
#include "schemamodel.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QProcess>
#include <QtQml/qqmlextensionplugin.h>
#include <cstdio>

Q_IMPORT_QML_PLUGIN(YakbdPlugin)

using namespace kbl;

static const char *defaultStyle()
{
#if defined(Q_OS_WIN)
    return "FluentWinUI3";
#elif defined(Q_OS_MACOS)
    return "macOS";
#else
    return "Fusion";
#endif
}

// --self-test FILE: open, edit a key, save, reopen and validate with the CLI, without a window. Used to check
// a package on a machine where nobody watches the screen (CI runners, the Windows guest).
static int selfTest(const QString &file)
{
    auto say = [](const QString &s) { std::printf("%s\n", qPrintable(s)); };
    say(QStringLiteral("YAKBD Editor ") + QStringLiteral(YAKBD_GUI_VERSION));
    KblDocument doc;
    if (!doc.open(file)) {
        say(QStringLiteral("FAIL open: ") + doc.lastError());
        return 1;
    }
    const QByteArray original = doc.bytes();
    QFile in(file);
    in.open(QIODevice::ReadOnly);
    if (in.readAll() != original) {
        say(QStringLiteral("FAIL round trip: the unedited document does not serialise to the file's bytes"));
        return 1;
    }
    say(QStringLiteral("ok   open and byte-exact round trip"));
    const QVariantList layers = doc.layers();
    if (layers.isEmpty()) {
        say(QStringLiteral("FAIL the layout has no hardware layers"));
        return 1;
    }
    doc.setBinding(0, QStringLiteral("KeyQ"), QStringLiteral("text"), QStringLiteral("æ"));
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QFileInfo(file).fileName());
    if (!doc.saveAs(out)) {
        say(QStringLiteral("FAIL save: ") + doc.lastError());
        return 1;
    }
    KblDocument again;
    if (!again.open(out) || again.bindingAt(0, QStringLiteral("KeyQ")).value(QStringLiteral("value")).toString()
                                != QStringLiteral("æ")) {
        say(QStringLiteral("FAIL reopen: the edit is not in the saved file"));
        return 1;
    }
    say(QStringLiteral("ok   edit, save, reopen"));
    const QString program = Cli::findProgram({});
    QString problem;
    const QString version = program.isEmpty() ? QString() : Cli::queryVersion(program, &problem);
    if (version.isEmpty()) {
        say(QStringLiteral("FAIL cli: ") + (program.isEmpty() ? QStringLiteral("yakbd not found") : problem));
        return 1;
    }
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, {QStringLiteral("validate"), QStringLiteral("-windows"), QStringLiteral("-q"), out});
    p.waitForFinished(60000);
    if (p.exitCode() != 0) {
        say(QStringLiteral("FAIL yakbd validate: ") + QString::fromUtf8(p.readAll()));
        return 1;
    }
    say(QStringLiteral("ok   yakbd %1 (%2) validates the saved file").arg(version, QDir::toNativeSeparators(program)));
    say(QStringLiteral("PASS"));
    return 0;
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QString::fromLatin1(defaultStyle()));
    QGuiApplication::setOrganizationName(QStringLiteral("YAKBD"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("yakbd.github.io"));
    QGuiApplication::setApplicationName(QStringLiteral("YAKBD Editor"));
    QGuiApplication::setApplicationVersion(QStringLiteral(YAKBD_GUI_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.yakbd.gui"));
    // The self-test needs no display: it runs without a GUI application object (no platform plugin).
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], "--self-test") == 0) {
            QCoreApplication core(argc, argv);
            const QStringList args = core.arguments();
            QString file;
            for (int j = 1; j < args.size(); ++j)
                if (!args[j].startsWith(QLatin1String("--")))
                    file = args[j];
            if (file.isEmpty()) {
                std::fprintf(stderr, "--self-test needs a .kbl.json file\n");
                return 2;
            }
            return selfTest(file);
        }
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Yakbd/icons/yakbd.svg")));

    QCommandLineParser cl;
    cl.setApplicationDescription(QStringLiteral("Editor for .kbl.json keyboard layouts"));
    cl.addHelpOption();
    cl.addVersionOption();
    cl.addPositionalArgument(QStringLiteral("file"), QStringLiteral("A layout to open (.kbl.json, .klc, CLDR .xml, .dll)"));
    QCommandLineOption grab(QStringLiteral("grab"), QStringLiteral("Save a screenshot of the window to FILE and quit."), QStringLiteral("file"));
    QCommandLineOption grabDelay(QStringLiteral("grab-delay"), QStringLiteral("Milliseconds before --grab (default 1500)."), QStringLiteral("ms"), QStringLiteral("1500"));
    QCommandLineOption appearance(QStringLiteral("appearance"), QStringLiteral("system, light or dark for this run."), QStringLiteral("mode"));
    QCommandLineOption page(QStringLiteral("page"), QStringLiteral("Page to show: keyboard, deadkeys, metadata, settings, windows, targets, json."), QStringLiteral("page"));
    QCommandLineOption select(QStringLiteral("select"), QStringLiteral("JSON Pointer to show, as the Problems panel does."), QStringLiteral("pointer"));
    QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Window size WxH."), QStringLiteral("size"));
    QCommandLineOption validate(QStringLiteral("validate"), QStringLiteral("Run yakbd validate after opening."));
    QCommandLineOption selfTestOpt(QStringLiteral("self-test"), QStringLiteral("Open FILE, edit, save, reopen and validate it without a window."));
    QCommandLineOption noSettings(QStringLiteral("no-settings"), QStringLiteral("Do not read or write saved preferences."));
    QCommandLineOption openDialog(QStringLiteral("show-open-dialog"), QStringLiteral("Show the Open dialog after start (for checks of the native dialog)."));
    cl.addOptions({grab, grabDelay, appearance, page, select, size, validate, selfTestOpt, noSettings, openDialog});
    cl.process(app);
    const QStringList files = cl.positionalArguments();

    if (cl.isSet(noSettings) || cl.isSet(grab))
        AppSettings::setPersistent(false);
    AppSettings settings;
    if (cl.isSet(appearance))
        settings.setAppearance(cl.value(appearance));
    settings.apply();
    KblDocument doc;
    ReportModel report;
    SchemaModel schema;
    Cli cli(&doc, &report, &settings);
    cli.locate();

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("doc"), &doc);
    ctx->setContextProperty(QStringLiteral("report"), &report);
    ctx->setContextProperty(QStringLiteral("schema"), &schema);
    ctx->setContextProperty(QStringLiteral("cli"), &cli);
    ctx->setContextProperty(QStringLiteral("appSettings"), &settings);
    engine.loadFromModule("Yakbd", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (cl.isSet(size)) {
        const QStringList wh = cl.value(size).split(QLatin1Char('x'));
        if (wh.size() == 2)
            win->resize(wh[0].toInt(), wh[1].toInt());
    }
    if (!files.isEmpty())
        QMetaObject::invokeMethod(win, "openPath", Q_ARG(QVariant, QFileInfo(files.first()).absoluteFilePath()));
    if (cl.isSet(page))
        QMetaObject::invokeMethod(win, "showPage", Q_ARG(QVariant, cl.value(page)));
    if (cl.isSet(select))
        doc.navigate(cl.value(select));
    if (cl.isSet(validate))
        cli.validate();
    if (cl.isSet(openDialog))
        QTimer::singleShot(800, win, [win] { QMetaObject::invokeMethod(win, "showOpenDialog"); });

    if (cl.isSet(grab)) {
        const QString out = cl.value(grab);
        QTimer::singleShot(cl.value(grabDelay).toInt(), &app, [&, out] {
            const QImage img = win->grabWindow();
            const bool ok = !img.isNull() && img.save(out);
            std::printf("grab %s %dx%d style=%s platform=%s dark=%d %s\n", qPrintable(out), img.width(), img.height(),
                        qPrintable(QQuickStyle::name()), qPrintable(QGuiApplication::platformName()),
                        int(settings.dark()), ok ? "saved" : "FAILED");
            QCoreApplication::exit(ok ? 0 : 3);
        });
    }
    return app.exec();
}
