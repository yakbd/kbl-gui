// Runs the yakbd command-line tool. The GUI never builds or converts layouts itself: it writes the open
// document to a temporary file and asks the CLI, then reads the CLI's JSON report.
#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

namespace kbl {

class KblDocument;
class ReportModel;
class AppSettings;

class Cli : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString program READ program NOTIFY programChanged)
    Q_PROPERTY(QString version READ version NOTIFY programChanged)
    Q_PROPERTY(bool available READ available NOTIFY programChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY programChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString operation READ operation NOTIFY busyChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)

public:
    Cli(KblDocument *doc, ReportModel *report, AppSettings *settings, QObject *parent = nullptr);

    QString program() const { return m_program; }
    QString version() const { return m_version; }
    bool available() const { return !m_program.isEmpty() && !m_version.isEmpty(); }
    QString problem() const { return m_problem; }
    bool busy() const { return bool(m_proc); }
    QString operation() const { return m_op; }
    QString log() const { return m_log; }

    // Finds yakbd: the configured path, then next to the GUI executable, then on PATH. Checks `yakbd version`.
    Q_INVOKABLE void locate();
    static QString findProgram(const QString &configured);
    static QString queryVersion(const QString &program, QString *problem);

    Q_INVOKABLE bool validate();
    Q_INVOKABLE bool importLayout(const QString &path);
    Q_INVOKABLE bool pinDefaults();
    Q_INVOKABLE bool compile(const QString &outputFolder);
    Q_INVOKABLE bool buildInstaller(const QString &outputFile);
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE void cancel();

    // Formats the CLI can read and what the GUI opens through `convert`.
    Q_INVOKABLE static bool needsImport(const QString &path);

    // For tests: wait until the running command ends.
    bool waitForFinished(int msecs = 60000);

signals:
    void programChanged();
    void busyChanged();
    void logChanged();
    // op: validate, import, pin, compile, installer. ok: exit code 0. message: one line for the status bar.
    void finished(const QString &op, bool ok, const QString &message);
    void imported(const QString &sourcePath);

private:
    bool start(const QString &op, const QStringList &args, bool writeDocument);
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void appendLog(const QString &text);

    KblDocument *m_doc;
    ReportModel *m_report;
    AppSettings *m_settings;
    QString m_program;
    QString m_version;
    QString m_problem;
    QString m_op;
    QString m_log;
    QString m_importSource;
    QString m_pinOut;
    QPointer<QProcess> m_proc;
    std::unique_ptr<QTemporaryDir> m_tmp;
    QString m_docFile;
    QString m_reportFile;
};

} // namespace kbl
