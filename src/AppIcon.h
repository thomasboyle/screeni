#pragma once

#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>

namespace AppIcon {

inline QIcon fallback()
{
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x5a, 0x8f, 0x4a));
    p.drawEllipse(2, 2, 28, 28);
    p.end();
    return QIcon(pm);
}

// The app icon ships as a loose file next to the exe (Assets/Screeni.ico) so the
// binary stays small. Falls back to a generic system icon, then to a painted
// placeholder, if the file is missing.
inline QIcon load()
{
    const QString diskPath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/Assets/Screeni.ico");
    QIcon icon(diskPath);
    if (!icon.isNull())
        return icon;
    icon = QIcon::fromTheme(QStringLiteral("applications-system"));
    return icon.isNull() ? fallback() : icon;
}

}  // namespace AppIcon