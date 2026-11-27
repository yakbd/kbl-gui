// QML views with Qt Quick Test, offscreen: the app's own module and C++ objects, no window manager.
#include "appsettings.h"
#include "cli.h"
#include "kbldocument.h"
#include "reportmodel.h"
#include "schemamodel.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuickTest>

Q_IMPORT_QML_PLUGIN(YakbdPlugin)

using namespace kbl;

class Setup : public QObject
{
    Q_OBJECT
public:
    Setup() { AppSettings::setPersistent(false); }

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        settings = new AppSettings(this);
        doc = new KblDocument(this);
        report = new ReportModel(this);
        schema = new SchemaModel(this);
        cli = new Cli(doc, report, settings, this);
        cli->locate();
        QQmlContext *c = engine->rootContext();
        c->setContextProperty(QStringLiteral("doc"), doc);
        c->setContextProperty(QStringLiteral("report"), report);
        c->setContextProperty(QStringLiteral("schema"), schema);
        c->setContextProperty(QStringLiteral("cli"), cli);
        c->setContextProperty(QStringLiteral("appSettings"), settings);
        c->setContextProperty(QStringLiteral("testData"), qEnvironmentVariable("YAKBD_TESTDATA"));
    }

private:
    AppSettings *settings = nullptr;
    KblDocument *doc = nullptr;
    ReportModel *report = nullptr;
    SchemaModel *schema = nullptr;
    Cli *cli = nullptr;
};

QUICK_TEST_MAIN_WITH_SETUP(yakbd_qml, Setup)
#include "tst_qml.moc"
