// The CLI's report (`-report file.json`) as a list model for the Problems panel.
#pragma once

#include <QAbstractListModel>
#include <QJsonObject>

namespace kbl {

struct ReportEntry {
    QString entryClass; // diagnostic, loss, defaulted
    QString severity;   // info, accept, warning, error
    QString kind;
    QString message;
    QString file;
    int line = 0;
    QString path;       // as reported
    QString pointer;    // a JSON Pointer into the open document, or empty
    bool missing = false; // the document has no value at pointer yet (a default rule fills it)
    QString detail;     // compact JSON
};

class ReportModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY countsChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY countsChanged)
    Q_PROPERTY(int lossCount READ lossCount NOTIFY countsChanged)
    Q_PROPERTY(int defaultCount READ defaultCount NOTIFY countsChanged)
    Q_PROPERTY(int problemCount READ problemCount NOTIFY countsChanged)
    Q_PROPERTY(int total READ total NOTIFY countsChanged)
    Q_PROPERTY(QString source READ source NOTIFY countsChanged)
    Q_PROPERTY(QString toolVersion READ toolVersion NOTIFY countsChanged)
    Q_PROPERTY(bool stale READ stale WRITE setStale NOTIFY staleChanged)

public:
    enum Roles {
        ClassRole = Qt::UserRole + 1,
        SeverityRole,
        KindRole,
        MessageRole,
        LocationRole,
        PointerRole,
        DetailRole,
        NavigableRole,
        MissingRole,
    };
    explicit ReportModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    int errorCount() const { return m_errors; }
    int warningCount() const { return m_warnings; }
    int lossCount() const { return m_losses; }
    int defaultCount() const { return m_defaults; }
    int problemCount() const { return m_problems; }
    int total() const { return int(m_all.size()); }
    QString source() const { return m_source; }
    QString toolVersion() const { return m_toolVersion; }
    bool stale() const { return m_stale; }
    void setStale(bool s);

    // Reads a report. inDocument: the report's paths point into the open document (false for an import, whose
    // locations are in the source file). documentFile: the file name the CLI was given for the document.
    bool load(const QByteArray &json, const QString &source, bool inDocument, const QString &documentFile);
    Q_INVOKABLE void clear();
    const std::vector<ReportEntry> &entries() const { return m_all; }

    // For a defaulted choice, the field that would override it (targets/windows.md §1, "overridden by").
    static QString fieldForDefault(const QString &kind);

signals:
    void filterChanged();
    void countsChanged();
    void staleChanged();

private:
    void refilter();
    bool accepts(const ReportEntry &e) const;

    std::vector<ReportEntry> m_all;
    std::vector<int> m_rows;
    QString m_filter = QStringLiteral("problems");
    QString m_source;
    QString m_toolVersion;
    int m_errors = 0, m_warnings = 0, m_losses = 0, m_defaults = 0, m_problems = 0;
    bool m_stale = false;
};

} // namespace kbl
