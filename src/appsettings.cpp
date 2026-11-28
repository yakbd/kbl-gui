#include "appsettings.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QSettings>
#include <QStyleHints>
#include <QUrl>

namespace kbl {

static bool g_persistent = true;

void AppSettings::setPersistent(bool p)
{
    g_persistent = p;
}

AppSettings::AppSettings(QObject *parent) : QObject(parent)
{
    m_platformPalette = QGuiApplication::palette();
    if (g_persistent) {
        QSettings s;
        m_recent = s.value(QStringLiteral("recentFiles")).toStringList();
        m_appearance = s.value(QStringLiteral("appearance"), QStringLiteral("system")).toString();
        m_cliPath = s.value(QStringLiteral("cliPath")).toString();
        m_lastFolder = s.value(QStringLiteral("lastFolder")).toString();
        m_target = s.value(QStringLiteral("target"), m_target).toString();
    }
}

void AppSettings::store()
{
    if (!g_persistent)
        return;
    QSettings s;
    s.setValue(QStringLiteral("recentFiles"), m_recent);
    s.setValue(QStringLiteral("appearance"), m_appearance);
    s.setValue(QStringLiteral("cliPath"), m_cliPath);
    s.setValue(QStringLiteral("lastFolder"), m_lastFolder);
    s.setValue(QStringLiteral("target"), m_target);
}

void AppSettings::addRecent(const QString &path)
{
    const QString p = QFileInfo(path).absoluteFilePath();
    m_recent.removeAll(p);
    m_recent.prepend(p);
    while (m_recent.size() > 10)
        m_recent.removeLast();
    store();
    emit recentFilesChanged();
}

void AppSettings::removeRecent(const QString &path)
{
    if (m_recent.removeAll(path)) {
        store();
        emit recentFilesChanged();
    }
}

void AppSettings::clearRecent()
{
    m_recent.clear();
    store();
    emit recentFilesChanged();
}

void AppSettings::setAppearance(const QString &a)
{
    const QString v = (a == QLatin1String("light") || a == QLatin1String("dark")) ? a : QStringLiteral("system");
    if (v == m_appearance)
        return;
    m_appearance = v;
    store();
    apply();
}

static QPalette fusionPalette(bool dark)
{
    QPalette p;
    if (dark) {
        const QColor window(0x2b, 0x2b, 0x2e), base(0x1e, 0x1e, 0x20), text(0xe8, 0xe8, 0xea);
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, QColor(0x26, 0x26, 0x29));
        p.setColor(QPalette::ToolTipBase, base);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::PlaceholderText, QColor(0x8c, 0x8c, 0x92));
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, QColor(0x35, 0x35, 0x39));
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::BrightText, Qt::white);
        p.setColor(QPalette::Light, QColor(0x45, 0x45, 0x4a));
        p.setColor(QPalette::Midlight, QColor(0x3a, 0x3a, 0x3e));
        p.setColor(QPalette::Mid, QColor(0x25, 0x25, 0x28));
        p.setColor(QPalette::Dark, QColor(0x18, 0x18, 0x1a));
        p.setColor(QPalette::Shadow, Qt::black);
        p.setColor(QPalette::Highlight, QColor(0x3d, 0x7e, 0xd6));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Link, QColor(0x6c, 0xa8, 0xf0));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x70, 0x70, 0x76));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x70, 0x70, 0x76));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x70, 0x70, 0x76));
    } else {
        const QColor window(0xef, 0xef, 0xf1), text(0x1c, 0x1c, 0x1e);
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(0xf6, 0xf6, 0xf8));
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, QColor(0xf4, 0xf4, 0xf6));
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::PlaceholderText, QColor(0x7a, 0x7a, 0x80));
        p.setColor(QPalette::Highlight, QColor(0x2f, 0x6f, 0xd0));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Light, Qt::white);
        p.setColor(QPalette::Midlight, QColor(0xe3, 0xe3, 0xe6));
        p.setColor(QPalette::Mid, QColor(0xb8, 0xb8, 0xbe));
        p.setColor(QPalette::Dark, QColor(0x9c, 0x9c, 0xa2));
        p.setColor(QPalette::Link, QColor(0x1f, 0x5f, 0xc0));
    }
    return p;
}

void AppSettings::apply()
{
    QStyleHints *hints = QGuiApplication::styleHints();
    Qt::ColorScheme want = Qt::ColorScheme::Unknown;
    if (m_appearance == QLatin1String("light"))
        want = Qt::ColorScheme::Light;
    else if (m_appearance == QLatin1String("dark"))
        want = Qt::ColorScheme::Dark;
    if (want == Qt::ColorScheme::Unknown)
        hints->unsetColorScheme();
    else
        hints->setColorScheme(want);
    // Where the platform cannot switch (no platform theme, as in a bare X session), Fusion gets our palette.
    // With "system" and an unknown scheme the app stays light (Fusion's default).
    const bool followed = want == Qt::ColorScheme::Unknown || hints->colorScheme() == want;
    if (!followed && style() == QLatin1String("Fusion")) {
        QGuiApplication::setPalette(fusionPalette(want == Qt::ColorScheme::Dark));
        m_ownPalette = true;
    } else if (m_ownPalette) {
        QGuiApplication::setPalette(m_platformPalette);
        m_ownPalette = false;
    }
    emit appearanceChanged();
}

bool AppSettings::dark() const
{
    const Qt::ColorScheme s = QGuiApplication::styleHints()->colorScheme();
    if (m_ownPalette)
        return m_appearance == QLatin1String("dark");
    if (s != Qt::ColorScheme::Unknown)
        return s == Qt::ColorScheme::Dark;
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

void AppSettings::setCliPath(const QString &p)
{
    if (p == m_cliPath)
        return;
    m_cliPath = p;
    store();
    emit cliPathChanged();
}

void AppSettings::setLastFolder(const QString &f)
{
    if (f == m_lastFolder)
        return;
    m_lastFolder = f;
    store();
    emit lastFolderChanged();
}

QString AppSettings::platform() const
{
#if defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#else
    return QStringLiteral("linux");
#endif
}

QString AppSettings::style() const
{
    return QQuickStyle::name();
}

QString AppSettings::version() const
{
    return QStringLiteral(YAKBD_GUI_VERSION);
}

QString AppSettings::toLocalFile(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QUrl AppSettings::fromLocalFile(const QString &path) const
{
    return QUrl::fromLocalFile(path);
}

QString AppSettings::folderOf(const QString &path) const
{
    return QFileInfo(path).absolutePath();
}

QString AppSettings::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString AppSettings::nativePath(const QString &path) const
{
    return QDir::toNativeSeparators(path);
}

} // namespace kbl

namespace kbl {

void AppSettings::setTarget(const QString &t)
{
    if (t.isEmpty() || t == m_target)
        return;
    m_target = t;
    store();
    emit targetChanged();
}

} // namespace kbl
