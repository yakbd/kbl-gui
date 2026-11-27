// User preferences: recent files, appearance, the CLI's location.
#pragma once

#include <QObject>
#include <QPalette>
#include <QStringList>
#include <QUrl>

namespace kbl {

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(QString appearance READ appearance WRITE setAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY appearanceChanged)
    Q_PROPERTY(QString cliPath READ cliPath WRITE setCliPath NOTIFY cliPathChanged)
    Q_PROPERTY(QString lastFolder READ lastFolder WRITE setLastFolder NOTIFY lastFolderChanged)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(QString style READ style CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QStringList recentFiles() const { return m_recent; }
    Q_INVOKABLE void addRecent(const QString &path);
    Q_INVOKABLE void removeRecent(const QString &path);
    Q_INVOKABLE void clearRecent();

    QString appearance() const { return m_appearance; }
    void setAppearance(const QString &a); // "system", "light", "dark"
    bool dark() const;
    // Applies the appearance to the application (colour scheme, or a palette where the platform has none).
    void apply();

    QString cliPath() const { return m_cliPath; }
    void setCliPath(const QString &p);
    QString lastFolder() const { return m_lastFolder; }
    void setLastFolder(const QString &f);

    QString platform() const;
    QString style() const;
    QString version() const;
    QString qtVersion() const { return QString::fromLatin1(qVersion()); }

    Q_INVOKABLE QString toLocalFile(const QUrl &url) const;
    Q_INVOKABLE QUrl fromLocalFile(const QString &path) const;
    Q_INVOKABLE QString folderOf(const QString &path) const;
    Q_INVOKABLE QString fileName(const QString &path) const;
    Q_INVOKABLE QString nativePath(const QString &path) const;

    static void setPersistent(bool p); // tests turn persistence off

signals:
    void recentFilesChanged();
    void appearanceChanged();
    void cliPathChanged();
    void lastFolderChanged();

private:
    void store();
    QStringList m_recent;
    QString m_appearance = QStringLiteral("system");
    QString m_cliPath;
    QString m_lastFolder;
    QPalette m_platformPalette;
    bool m_ownPalette = false;
};

} // namespace kbl
