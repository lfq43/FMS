#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QMessageBox>
#include <QStandardPaths>

#include "AppShell.h"
#include "AuthService.h"
#include "core/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("FMS");
    QCoreApplication::setApplicationName("FMS");

    const int fontId = QFontDatabase::addApplicationFont(":/fonts/NotoSansSC-Regular.ttf");
    if (fontId != -1) {
        const QString family = QFontDatabase::applicationFontFamilies(fontId).value(0);
        QFont font(family);
        font.setHintingPreference(QFont::PreferNoHinting);
        QApplication::setFont(font);
    }
    //加载自定义字体

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString databasePath = dataDir + "/fms.db";
    if (!DatabaseManager::instance().openDatabase(databasePath)) {
        QMessageBox::critical(nullptr, "Database Error", DatabaseManager::instance().lastError());
        return 1;
    }
    AuthService::restoreRememberedLogin();

    AppShell window;
    window.setWindowTitle("文枢");
    window.show();

    return app.exec();
}
