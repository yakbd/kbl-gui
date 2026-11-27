#include "reportmodel.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace kbl {

ReportModel::ReportModel(QObject *parent) : QAbstractListModel(parent) {}

int ReportModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> ReportModel::roleNames() const
{
    return {
        {ClassRole, "entryClass"}, {SeverityRole, "severity"}, {KindRole, "kind"},
        {MessageRole, "message"},  {LocationRole, "location"}, {PointerRole, "pointer"},
        {DetailRole, "detail"},    {NavigableRole, "navigable"}, {MissingRole, "missing"},
    };
}

QVariant ReportModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return {};
    const ReportEntry &e = m_all[size_t(m_rows[size_t(index.row())])];
    switch (role) {
    case Qt::DisplayRole:
    case MessageRole: return e.message;
    case ClassRole: return e.entryClass;
    case SeverityRole: return e.severity;
    case KindRole: return e.kind;
    case LocationRole: {
        if (!e.pointer.isEmpty())
            return e.missing ? e.pointer + QStringLiteral(" (not in the file)") : e.pointer;
        QString loc = e.file;
        if (e.line > 0)
            loc += QStringLiteral(":%1").arg(e.line);
        if (!e.path.isEmpty())
            loc += (loc.isEmpty() ? QString() : QStringLiteral(" ")) + e.path;
        return loc;
    }
    case PointerRole: return e.pointer;
    case DetailRole: return e.detail;
    case NavigableRole: return !e.pointer.isEmpty();
    case MissingRole: return e.missing;
    }
    return {};
}

void ReportModel::setFilter(const QString &f)
{
    if (f == m_filter)
        return;
    m_filter = f;
    refilter();
    emit filterChanged();
}

void ReportModel::setStale(bool s)
{
    if (s == m_stale)
        return;
    m_stale = s;
    emit staleChanged();
}

bool ReportModel::accepts(const ReportEntry &e) const
{
    if (m_filter == QLatin1String("all"))
        return true;
    if (m_filter == QLatin1String("losses"))
        return e.entryClass == QLatin1String("loss");
    if (m_filter == QLatin1String("defaults"))
        return e.entryClass == QLatin1String("defaulted");
    // problems: errors and warnings of any class, plus every diagnostic
    return e.severity == QLatin1String("error") || e.severity == QLatin1String("warning")
           || e.entryClass == QLatin1String("diagnostic");
}

void ReportModel::refilter()
{
    beginResetModel();
    m_rows.clear();
    for (size_t i = 0; i < m_all.size(); ++i)
        if (accepts(m_all[i]))
            m_rows.push_back(int(i));
    // Errors first, then warnings, then the rest, each in report order.
    auto rank = [&](int i) {
        const QString &s = m_all[size_t(i)].severity;
        return s == QLatin1String("error") ? 0 : s == QLatin1String("warning") ? 1 : 2;
    };
    std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) { return rank(a) < rank(b); });
    endResetModel();
}

QString ReportModel::fieldForDefault(const QString &kind)
{
    static const QHash<QString, QString> f{
        {QStringLiteral("dllName"), QStringLiteral("/targets/windows/dllName")},
        {QStringLiteral("builds"), QStringLiteral("/targets/windows/builds")},
        {QStringLiteral("strings"), QStringLiteral("/targets/windows/strings")},
        {QStringLiteral("versionInfo"), QStringLiteral("/targets/windows/versionInfo")},
        {QStringLiteral("altGr"), QStringLiteral("/targets/windows/localeFlags/altGr")},
        {QStringLiteral("shiftLock"), QStringLiteral("/targets/windows/localeFlags/shiftLock")},
        {QStringLiteral("lrmRlm"), QStringLiteral("/targets/windows/localeFlags/lrmRlm")},
        {QStringLiteral("kbdVersion"), QStringLiteral("/targets/windows/kbdVersion")},
        {QStringLiteral("keyboardType"), QStringLiteral("/targets/windows/keyboardType")},
        {QStringLiteral("modifierBits"), QStringLiteral("/targets/windows/modifiers")},
        {QStringLiteral("modNumber"), QStringLiteral("/targets/windows/modifiers")},
        {QStringLiteral("vk"), QStringLiteral("/targets/windows/keys")},
        {QStringLiteral("scanCodeFlags"), QStringLiteral("/targets/windows/keys")},
        {QStringLiteral("scanCodeTableSize"), QStringLiteral("/targets/windows/scanCodeTableSize")},
        {QStringLiteral("attributes"), QStringLiteral("/targets/windows/keys")},
        {QStringLiteral("tables"), QStringLiteral("/targets/windows/tables")},
        {QStringLiteral("deadCharacter"), QStringLiteral("/targets/windows/deadKeys")},
        {QStringLiteral("registration"), QStringLiteral("/targets/windows/registration")},
        {QStringLiteral("productCode"), QStringLiteral("/targets/windows/registration/productCode")},
        {QStringLiteral("enableForUser"), QStringLiteral("/targets/windows/install/enableForUser")},
        {QStringLiteral("enableForSignInAndNewUsers"),
         QStringLiteral("/targets/windows/install/enableForSignInAndNewUsers")},
        {QStringLiteral("uninstallEntry"), QStringLiteral("/targets/windows/install")},
    };
    if (!kind.startsWith(QLatin1String("windowsDefault.")))
        return {};
    return f.value(kind.mid(int(qstrlen("windowsDefault."))), QStringLiteral("/targets/windows"));
}

bool ReportModel::load(const QByteArray &json, const QString &source, bool inDocument, const QString &documentFile)
{
    QJsonParseError err;
    const QJsonDocument d = QJsonDocument::fromJson(json, &err);
    if (!d.isObject())
        return false;
    const QJsonObject root = d.object();
    m_all.clear();
    m_errors = m_warnings = m_losses = m_defaults = m_problems = 0;
    m_source = source;
    // Report format 1 (yakbd 0.1.0-preview.2 and later, docs/report-file.md of yakbd-cli): where.pointer is a
    // JSON Pointer into a .kbl.json input, where.path never is. Without "format" (0.1.0-preview.1) every location
    // is in where.path, pointers mixed with labels, and every default rule points at /targets/windows.
    const bool format1 = root.value(QStringLiteral("format")).toInt() >= 1;
    m_toolVersion = root.value(QStringLiteral("tool")).toObject().value(QStringLiteral("version")).toString();
    if (format1)
        inDocument = root.value(QStringLiteral("input")).toObject().value(QStringLiteral("format")).toString()
                     == QLatin1String("kbl.json");
    for (const QJsonValue &v : root.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject o = v.toObject();
        ReportEntry e;
        e.entryClass = o.value(QStringLiteral("class")).toString();
        e.severity = o.value(QStringLiteral("severity")).toString();
        e.kind = o.value(QStringLiteral("kind")).toString();
        e.message = o.value(QStringLiteral("message")).toString();
        const QJsonObject where = o.value(QStringLiteral("where")).toObject();
        e.file = where.value(QStringLiteral("file")).toString();
        e.line = where.value(QStringLiteral("line")).toInt();
        e.path = where.value(QStringLiteral("path")).toString();
        e.missing = where.value(QStringLiteral("missing")).toBool();
        const QJsonValue det = o.value(QStringLiteral("detail"));
        if (det.isObject())
            e.detail = QString::fromUtf8(QJsonDocument(det.toObject()).toJson(QJsonDocument::Compact));
        else if (det.isArray())
            e.detail = QString::fromUtf8(QJsonDocument(det.toArray()).toJson(QJsonDocument::Compact));
        else if (!det.isUndefined())
            e.detail = det.toVariant().toString();
        if (format1) {
            if (inDocument && where.contains(QStringLiteral("pointer")))
                e.pointer = where.value(QStringLiteral("pointer")).toString();
        } else {
            // preview.1: only paths that look like pointers, in the open document, are links; the CLI also
            // wrote labels ("DLL", "x64", "keys[1]") in this field, and an import's locations are in its source.
            const bool sameFile = e.file.isEmpty() || e.file == documentFile
                                  || e.file.endsWith(QLatin1Char('/') + documentFile)
                                  || e.file.endsWith(QLatin1Char('\\') + documentFile);
            if (inDocument && sameFile && e.path.startsWith(QLatin1Char('/'))) {
                e.pointer = e.path;
                e.path.clear();
            }
            if (inDocument && e.entryClass == QLatin1String("defaulted")
                && (e.pointer.isEmpty() || e.pointer == QLatin1String("/targets/windows")))
                e.pointer = fieldForDefault(e.kind);
        }
        if (e.severity == QLatin1String("error"))
            ++m_errors;
        else if (e.severity == QLatin1String("warning"))
            ++m_warnings;
        if (e.entryClass == QLatin1String("loss"))
            ++m_losses;
        if (e.entryClass == QLatin1String("defaulted"))
            ++m_defaults;
        m_all.push_back(e);
    }
    for (const auto &e : m_all)
        if (e.severity == QLatin1String("error") || e.severity == QLatin1String("warning")
            || e.entryClass == QLatin1String("diagnostic"))
            ++m_problems;
    m_stale = false;
    refilter();
    emit countsChanged();
    emit staleChanged();
    return true;
}

void ReportModel::clear()
{
    beginResetModel();
    m_all.clear();
    m_rows.clear();
    m_errors = m_warnings = m_losses = m_defaults = m_problems = 0;
    m_source.clear();
    m_toolVersion.clear();
    endResetModel();
    emit countsChanged();
}

} // namespace kbl
