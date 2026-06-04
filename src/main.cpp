#include "AppController.h"
#include "I18nCatalog.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQml>

#include <QFile>
#include <QTextStream>
#include <QDateTime>

#ifdef Q_OS_LINUX
#include <gnu/libc-version.h>
#endif

// Early file logging next to the executable (very useful for released builds that "don't start")
static QFile gLogFile;
static QTextStream gLogStream;

QString executableDirPath(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    const QString procExe = QFile::symLinkTarget(QStringLiteral("/proc/self/exe"));
    if (!procExe.isEmpty()) {
        return QFileInfo(procExe).absolutePath();
    }
#endif
    const QString exePath = (argc > 0) ? QString::fromLocal8Bit(argv[0]) : QString();
    return QFileInfo(exePath).absolutePath();
}

void autofishMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString typeStr;
    switch (type) {
    case QtDebugMsg:   typeStr = "DEBUG"; break;
    case QtInfoMsg:    typeStr = "INFO "; break;
    case QtWarningMsg: typeStr = "WARN "; break;
    case QtCriticalMsg:typeStr = "CRIT "; break;
    case QtFatalMsg:   typeStr = "FATAL"; break;
    default:           typeStr = "???? "; break;
    }

    QString fileInfo = context.file ? QString("%1:%2").arg(context.file).arg(context.line) : QString("?");
    QString logLine = QString("[%1] [%2] %3 (%4)")
                          .arg(timestamp, typeStr, msg, fileInfo);

    if (gLogFile.isOpen()) {
        gLogStream << logLine << Qt::endl;
        gLogStream.flush();
    }
    // Also to stderr so it appears in terminal too
    fprintf(stderr, "%s\n", qPrintable(logLine));

    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[])
{
    // === VERY EARLY LOGGING SETUP (before any Qt init that can fail) ===
    QString exePath = (argc > 0) ? QString::fromLocal8Bit(argv[0]) : QString();
    const QString exeDir = executableDirPath(argc, argv);
    QString logPath = exeDir.isEmpty()
                          ? QStringLiteral("autofish.log")
                          : exeDir + QStringLiteral("/autofish.log");

    gLogFile.setFileName(logPath);
    if (gLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        gLogStream.setDevice(&gLogFile);
        gLogStream << "\n========== Autofish started " << QDateTime::currentDateTime().toString(Qt::ISODate) << " ==========\n";
        gLogStream.flush();
    }

    qInstallMessageHandler(autofishMessageHandler);

    qInfo() << "Exe path:" << exePath;
    qInfo() << "Log file:" << logPath;
    qInfo() << "Qt runtime version:" << qVersion();
#ifdef AUTOFISH_VERSION
    qInfo() << "App version:" << AUTOFISH_VERSION;
#endif

#ifdef Q_OS_LINUX
    qInfo() << "glibc version:" << gnu_get_libc_version();
#endif

    QQuickStyle::setStyle("Fusion");

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NTE Autofish NXXT"));
#ifdef AUTOFISH_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(AUTOFISH_VERSION));
#endif

    qInfo() << "Platform name:" << QGuiApplication::platformName();
    qInfo() << "Library paths:" << QCoreApplication::libraryPaths();
    qInfo() << "Style:" << QQuickStyle::name();

    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (lockDir.isEmpty()) {
        lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    QDir().mkpath(lockDir);

    QString lockFilePath = QDir(lockDir).filePath(QStringLiteral("nte-autofish-nxxt.lock"));
    qInfo() << "Using lock file:" << lockFilePath;

    QLockFile lockFile(lockFilePath);
    if (!lockFile.tryLock(100)) {
        qWarning() << "Another instance is running (lock failed)";
        QMessageBox::information(nullptr,
                                 QStringLiteral("NTE Autofish NXXT"),
                                 QStringLiteral("NTE Autofish NXXT is already running."));
        return 0;
    }
    qInfo() << "Lock acquired successfully";

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

    qInfo() << "Loading QML module Autofish.Main ...";
    engine.loadFromModule("Autofish", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load main QML - no root objects. Check qml/Main.qml and resources.";
    } else {
        qInfo() << "QML loaded successfully, root objects:" << engine.rootObjects().size();
    }

    int ret = app.exec();
    qInfo() << "Application exited with code" << ret;
    return ret;
}
