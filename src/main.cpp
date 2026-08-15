#include "core/tracker.h"
#include "services/UpdateService.h"
#include "theme.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QLockFile>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Screeni"));
    QApplication::setOrganizationName(QStringLiteral("Screeni"));
    QApplication::setQuitOnLastWindowClosed(false);

    const QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QLatin1String("/screeni.lock");
    QLockFile lock(lockPath);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100)) {
        QMessageBox::information(nullptr, QStringLiteral("Screeni"),
                                 QStringLiteral("Screeni is already running."));
        return 0;
    }

    int fontId = QFontDatabase::addApplicationFont(
        QCoreApplication::applicationDirPath() + QStringLiteral("/Assets/Fonts/Monocraft.ttf"));
    if (fontId < 0)
        fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/assets/Fonts/Monocraft.ttf"));
    if (fontId >= 0) {
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont f(families.at(0));
            f.setPixelSize(14);
            app.setFont(f);
        }
    }

    const QString storedTheme = QSettings().value(QStringLiteral("theme"), QStringLiteral("Matcha")).toString();
    Theme::setTheme(storedTheme == QLatin1String("Lilac") ? Theme::Id::Lilac : Theme::Id::Matcha);

    app.setStyleSheet(Theme::globalStyleSheet());

    Tracker tracker;
    if (!tracker.start()) {
        QMessageBox::critical(nullptr, QStringLiteral("Screeni"),
                              QStringLiteral("Failed to start usage tracker."));
        return 1;
    }

    MainWindow window(tracker);
    window.show();

    const int code = app.exec();
    tracker.stop();
    return code;
}
