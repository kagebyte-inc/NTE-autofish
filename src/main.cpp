#include "AppController.h"
#include "I18nCatalog.h"

#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQml>

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle("Fusion");

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NSE Autofish NXXT"));
#ifdef AUTOFISH_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(AUTOFISH_VERSION));
#endif

    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (lockDir.isEmpty()) {
        lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    QDir().mkpath(lockDir);

    QLockFile lockFile(QDir(lockDir).filePath(QStringLiteral("nse-autofish-nxxt.lock")));
    if (!lockFile.tryLock(100)) {
        QMessageBox::information(nullptr,
                                 QStringLiteral("NSE Autofish NXXT"),
                                 QStringLiteral("NSE Autofish NXXT is already running."));
        return 0;
    }

    AppController controller;
    I18nCatalog i18n;
    i18n.setLanguage(controller.uiLanguage());
    QObject::connect(&controller, &AppController::uiLanguageChanged, &i18n, [&]() {
        i18n.setLanguage(controller.uiLanguage());
    });

    qmlRegisterSingletonInstance("Autofish", 1, 0, "AppController", &controller);
    qmlRegisterSingletonInstance("Autofish", 1, 0, "I18n", &i18n);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Autofish", "Main");

    return app.exec();
}
