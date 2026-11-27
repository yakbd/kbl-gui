#include "cli.h"

#include "appsettings.h"
#include "kbldocument.h"
#include "reportmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace kbl {

static QString exeName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("yakbd.exe");
#else
    return QStringLiteral("yakbd");
#endif
}

Cli::Cli(KblDocument *doc, ReportModel *report, AppSettings *settings, QObject *parent)
    : QObject(parent), m_doc(doc), m_report(report), m_settings(settings)
{
    if (m_settings)
        connect(m_settings, &AppSettings::cliPathChanged, this, &Cli::locate);
}

QString Cli::findProgram(const QString &configured)
{
    const QString env = qEnvironmentVariable("YAKBD_CLI");
    if (!env.isEmpty() && QFileInfo(env).isExecutable())
        return QFileInfo(env).absoluteFilePath();
    if (!configured.isEmpty() && QFileInfo(configured).isExecutable())
        return QFileInfo(configured).absoluteFilePath();
    const QString beside = QDir(QCoreApplication::applicationDirPath()).filePath(exeName());
    if (QFileInfo(beside).isExecutable())
        return beside;
    return QStandardPaths::findExecutable(QStringLiteral("yakbd"));
}

QString Cli::queryVersion(const QString &program, QString *problem)
{
    QProcess p;
    p.start(program, {QStringLiteral("version")});
    if (!p.waitForFinished(10000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (problem)
            *problem = QCoreApplication::translate("Cli", "%1 did not answer \"version\"").arg(program);
        p.kill();
        return {};
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    static const QRegularExpression re(QStringLiteral("^yakbd (\\d+)\\.(\\d+)\\.(\\d+)(\\S*)"));
    const auto m = re.match(out);
    if (!m.hasMatch()) {
        if (problem)
            *problem = QCoreApplication::translate("Cli", "%1 is not the yakbd tool (it says \"%2\")").arg(program, out);
        return {};
    }
    // This GUI speaks the 0.1 report format; later 0.x releases are expected to keep it (fields only added).
    if (m.captured(1).toInt() != 0 || m.captured(2).toInt() < 1) {
        if (problem)
            *problem = QCoreApplication::translate("Cli", "yakbd %1 is not supported; use 0.1 or a later 0.x release")
                           .arg(out.mid(6));
        return {};
    }
    if (problem)
        problem->clear();
    return out.mid(6);
}

void Cli::locate()
{
    m_program = findProgram(m_settings ? m_settings->cliPath() : QString());
    m_version.clear();
    m_problem.clear();
    if (m_program.isEmpty())
        m_problem = tr("The yakbd command-line tool was not found next to the editor or on the PATH.");
    else
        m_version = queryVersion(m_program, &m_problem);
    emit programChanged();
}

bool Cli::needsImport(const QString &path)
{
    const QString p = path.toLower();
    return p.endsWith(QLatin1String(".klc")) || p.endsWith(QLatin1String(".xml")) || p.endsWith(QLatin1String(".dll"));
}

void Cli::appendLog(const QString &text)
{
    m_log += text;
    if (!m_log.endsWith(QLatin1Char('\n')))
        m_log += QLatin1Char('\n');
    if (m_log.size() > 200000)
        m_log = m_log.right(150000);
    emit logChanged();
}

void Cli::clearLog()
{
    m_log.clear();
    emit logChanged();
}

void Cli::cancel()
{
    if (m_proc)
        m_proc->kill();
}

bool Cli::start(const QString &op, const QStringList &args, bool writeDocument)
{
    if (busy())
        return false;
    if (!available()) {
        emit finished(op, false, m_problem.isEmpty() ? tr("The yakbd tool is not available") : m_problem);
        return false;
    }
    if (writeDocument) {
        QFile f(m_docFile);
        if (!f.open(QIODevice::WriteOnly) || f.write(m_doc->bytes()) < 0) {
            emit finished(op, false, tr("Cannot write a temporary copy of the layout"));
            return false;
        }
    }
    m_op = op;
    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(m_tmp->path());
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::finished, this, &Cli::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            onFinished(-1, QProcess::CrashExit);
    });
    QStringList shown;
    for (const QString &a : args)
        shown << (a.contains(QLatin1Char(' ')) ? QLatin1Char('"') + a + QLatin1Char('"') : a);
    appendLog(QStringLiteral("$ yakbd ") + shown.join(QLatin1Char(' ')));
    m_proc->start(m_program, args);
    emit busyChanged();
    return true;
}

bool Cli::waitForFinished(int msecs)
{
    if (!m_proc)
        return true;
    QPointer<QProcess> p = m_proc;
    p->waitForFinished(msecs);
    QCoreApplication::processEvents();
    return !m_proc;
}

static std::unique_ptr<QTemporaryDir> newTemp()
{
    auto t = std::make_unique<QTemporaryDir>(QDir::temp().filePath(QStringLiteral("yakbd-gui-XXXXXX")));
    return t;
}

bool Cli::validate()
{
    if (!m_doc->loaded() || busy())
        return false;
    m_tmp = newTemp();
    m_docFile = m_tmp->filePath(m_doc->fileNameForCli());
    m_reportFile = m_tmp->filePath(QStringLiteral("report.json"));
    return start(QStringLiteral("validate"),
                 {QStringLiteral("validate"), QStringLiteral("-windows"), QStringLiteral("-report"), m_reportFile,
                  m_docFile},
                 true);
}

bool Cli::importLayout(const QString &path)
{
    if (busy())
        return false;
    m_tmp = newTemp();
    m_importSource = QFileInfo(path).absoluteFilePath();
    m_docFile = m_tmp->filePath(QFileInfo(path).completeBaseName() + QStringLiteral(".kbl.json"));
    m_reportFile = m_tmp->filePath(QStringLiteral("report.json"));
    return start(QStringLiteral("import"),
                 {QStringLiteral("convert"), QStringLiteral("-report"), m_reportFile, m_importSource, m_docFile},
                 false);
}

bool Cli::pinDefaults()
{
    if (!m_doc->loaded() || busy())
        return false;
    m_tmp = newTemp();
    m_docFile = m_tmp->filePath(m_doc->fileNameForCli());
    m_pinOut = m_tmp->filePath(QStringLiteral("pinned.kbl.json"));
    m_reportFile = m_tmp->filePath(QStringLiteral("report.json"));
    return start(QStringLiteral("pin"),
                 {QStringLiteral("pin"), QStringLiteral("-report"), m_reportFile, m_docFile, m_pinOut}, true);
}

bool Cli::compile(const QString &outputFolder)
{
    if (!m_doc->loaded() || busy())
        return false;
    m_tmp = newTemp();
    m_docFile = m_tmp->filePath(m_doc->fileNameForCli());
    m_reportFile = m_tmp->filePath(QStringLiteral("report.json"));
    return start(QStringLiteral("compile"),
                 {QStringLiteral("compile"), QStringLiteral("-o"), outputFolder, QStringLiteral("-report"),
                  m_reportFile, m_docFile},
                 true);
}

bool Cli::buildInstaller(const QString &outputFile)
{
    if (!m_doc->loaded() || busy())
        return false;
    m_tmp = newTemp();
    m_docFile = m_tmp->filePath(m_doc->fileNameForCli());
    m_reportFile = m_tmp->filePath(QStringLiteral("report.json"));
    return start(QStringLiteral("installer"),
                 {QStringLiteral("installer"), QStringLiteral("-o"), outputFile, QStringLiteral("-report"),
                  m_reportFile, m_docFile},
                 true);
}

void Cli::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_proc)
        return;
    const QString output = QString::fromUtf8(m_proc->readAll());
    const QString op = m_op;
    m_proc->deleteLater();
    m_proc = nullptr;
    m_op.clear();
    if (!output.isEmpty())
        appendLog(output);
    emit busyChanged();

    if (exitCode < 0 || status != QProcess::NormalExit) {
        appendLog(tr("yakbd did not run to the end"));
        emit finished(op, false, tr("The yakbd tool stopped unexpectedly"));
        return;
    }
    if (exitCode == 2) {
        // Usage error: the GUI built a command line this CLI does not take.
        emit finished(op, false, tr("yakbd did not accept the command (exit 2); see Output"));
        return;
    }
    const bool ok = exitCode == 0;
    QString message;
    if (op == QLatin1String("import") && ok) {
        QFile f(m_docFile);
        if (f.open(QIODevice::ReadOnly)
            && m_doc->loadBytes(f.readAll(), QFileInfo(m_importSource).dir().filePath(QFileInfo(m_docFile).fileName()), true)) {
            emit imported(m_importSource);
            message = tr("Imported %1; save it as a .kbl.json file").arg(QFileInfo(m_importSource).fileName());
        } else {
            message = tr("The converted file could not be read: %1").arg(m_doc->lastError());
        }
    }
    QFile rf(m_reportFile);
    if (rf.open(QIODevice::ReadOnly)) {
        const bool inDoc = op != QLatin1String("import");
        m_report->load(rf.readAll(), op, inDoc, QFileInfo(m_docFile).fileName());
    }
    if (op == QLatin1String("pin") && ok) {
        QFile f(m_pinOut);
        ParseError e;
        JPtr pinned = f.open(QIODevice::ReadOnly) ? parseValue(f.readAll(), &e) : JPtr();
        const int n = pinned ? m_doc->mergeMissing(pinned, tr("Pin defaults")) : 0;
        message = n ? tr("Pinned %n default choice(s) into the file", nullptr, n) : tr("Nothing to pin: every choice is in the file");
    }
    if (message.isEmpty()) {
        const int errors = m_report->errorCount(), warnings = m_report->warningCount();
        if (op == QLatin1String("validate"))
            message = ok && warnings == 0 ? tr("No problems found")
                      : ok              ? tr("Valid, with %n warning(s)", nullptr, warnings)
                                        : tr("%n error(s), %1 warning(s)", nullptr, errors).arg(warnings);
        else if (op == QLatin1String("compile"))
            message = ok ? tr("Built the layout DLLs") : tr("The build failed: %n error(s)", nullptr, errors);
        else if (op == QLatin1String("installer"))
            message = ok ? tr("Built the installer") : tr("The installer was not built: %n error(s)", nullptr, errors);
        else
            message = ok ? tr("Done") : tr("Failed: %n error(s)", nullptr, errors);
    }
    emit finished(op, ok, message);
}

} // namespace kbl
