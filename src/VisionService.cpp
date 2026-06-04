#include "VisionService.h"

#include "AppController.h"
#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QVariantMap>

VisionService::VisionService(AppController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    connect(&m_visionProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        handleStdoutChunk(m_visionProcess.readAllStandardOutput());
    });
    connect(&m_visionProcess, &QProcess::readyReadStandardError, this, [this]() {
        const QString output = QString::fromUtf8(m_visionProcess.readAllStandardError()).trimmed();
        if (!output.isEmpty()) {
            qWarning() << "vision stderr:" << output;  // ensures python tracebacks/warnings/errors go to autofish.log (always) + stderr
            emit statusMessage(output);
        }
    });
    connect(&m_visionProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VisionService::handleFinished);
}

QProcess &VisionService::process()
{
    return m_visionProcess;
}

QVariantList VisionService::windows() const
{
    return m_windows;
}

int VisionService::selectedWindowIndex() const
{
    return m_selectedWindowIndex;
}

QString VisionService::selectedWindowLabel() const
{
    if (m_selectedWindowIndex < 0 || m_selectedWindowIndex >= m_windows.size()) {
        return QStringLiteral("No window selected");
    }

    const QVariantMap window = m_windows.at(m_selectedWindowIndex).toMap();
    const QString process = window.value(QStringLiteral("process_name")).toString();
    const QString title = window.value(QStringLiteral("title")).toString();
    if (process.isEmpty()) {
        return title;
    }
    return QStringLiteral("%1 - %2").arg(process, title);
}

void VisionService::refreshWindows()
{
    QProcess process;
    process.setProcessEnvironment(m_controller->pythonEnvironment());
    process.setWorkingDirectory(m_controller->repoRoot());
    process.start(m_controller->pythonExecutable(),
                  {QStringLiteral("-m"), QStringLiteral("autofish_vision.service"),
                   QStringLiteral("--list-windows")});

    if (!process.waitForStarted(1500)) {
        emit statusMessage(QStringLiteral("Failed to start window scanner"));
        return;
    }

    if (!process.waitForFinished(4000)) {
        process.kill();
        emit statusMessage(QStringLiteral("Window scanner timed out"));
        return;
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!stderrText.isEmpty()) {
        emit statusMessage(stderrText);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit statusMessage(stdoutText.isEmpty() ? QStringLiteral("Window scanner returned no JSON") : stdoutText);
        return;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("event")).toString() != QStringLiteral("window_list")) {
        emit statusMessage(stdoutText);
        return;
    }

    QVariantList windows;
    const QJsonArray array = root.value(QStringLiteral("details")).toObject().value(QStringLiteral("windows")).toArray();
    for (const QJsonValue &value : array) {
        windows.append(value.toObject().toVariantMap());
    }

    m_windows = windows;
    emit windowsChanged();

    if (m_windows.isEmpty()) {
        m_selectedWindowIndex = -1;
    } else if (m_selectedWindowIndex < 0 || m_selectedWindowIndex >= m_windows.size()) {
        m_selectedWindowIndex = 0;
    }
    emit selectedWindowChanged();
    emit statusMessage(QStringLiteral("Found %1 windows").arg(m_windows.size()));
}

void VisionService::selectWindow(int index)
{
    if (index < -1 || index >= m_windows.size() || m_selectedWindowIndex == index) {
        return;
    }

    m_selectedWindowIndex = index;
    emit selectedWindowChanged();
}

void VisionService::startWatch()
{
    if (m_visionProcess.state() != QProcess::NotRunning) {
        emit statusMessage(QStringLiteral("Vision service is already running"));
        return;
    }

    m_controller->prepareVisionSession();
    m_frameGeometryChecked = false;
    m_stdoutBuffer.clear();

    m_visionProcess.setProcessEnvironment(m_controller->pythonEnvironment());
    m_visionProcess.setWorkingDirectory(m_controller->repoRoot());
    m_visionProcess.start(m_controller->pythonExecutable(), watchArguments());

    if (!m_visionProcess.waitForStarted(1500)) {
        const QString msg = QStringLiteral("Failed to start vision service");
        qWarning() << msg;
        emit statusMessage(msg);
        return;
    }

    emit statusMessage(QStringLiteral("Vision service started"));
}

void VisionService::startPortal()
{
    if (m_visionProcess.state() != QProcess::NotRunning) {
        emit statusMessage(QStringLiteral("Vision service is already running"));
        return;
    }

    m_controller->prepareVisionSession();
    m_frameGeometryChecked = false;
    m_stdoutBuffer.clear();

    QStringList arguments = {QStringLiteral("-m"), QStringLiteral("autofish_vision.service"),
                             QStringLiteral("--watch"), QStringLiteral("--portal")};
    if (m_controller->settings()->saveDebugFrames()) {
        arguments << QStringLiteral("--debug-dir") << QStringLiteral("debug")
                  << QStringLiteral("--debug-every") << QStringLiteral("30");
    }
    if (m_controller->settings()->debugMode()) {
        arguments << QStringLiteral("--live-debug")
                  << QStringLiteral("--live-debug-every") << QStringLiteral("20");
    }
    arguments << QStringLiteral("--interval") << QStringLiteral("0.005");

    m_visionProcess.setProcessEnvironment(m_controller->pythonEnvironment());
    m_visionProcess.setWorkingDirectory(m_controller->repoRoot());
    m_visionProcess.start(m_controller->pythonExecutable(), arguments);

    if (!m_visionProcess.waitForStarted(1500)) {
        const QString msg = QStringLiteral("Failed to start portal vision service");
        qWarning() << msg;
        emit statusMessage(msg);
        return;
    }

    emit statusMessage(QStringLiteral("Opening portal picker"));
}

void VisionService::stop()
{
    if (m_visionProcess.state() == QProcess::NotRunning) {
        return;
    }

    m_visionProcess.terminate();
    if (!m_visionProcess.waitForFinished(1500)) {
        m_visionProcess.kill();
    }
}

void VisionService::handleStdoutChunk(const QByteArray &chunk)
{
    m_stdoutBuffer += QString::fromUtf8(chunk);

    qsizetype newlineIndex = -1;
    while ((newlineIndex = m_stdoutBuffer.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_stdoutBuffer.left(newlineIndex).trimmed();
        m_stdoutBuffer.remove(0, newlineIndex + 1);
        if (!line.isEmpty()) {
            m_controller->handleVisionLine(line, m_frameGeometryChecked);
        }
    }

    const QString pending = m_stdoutBuffer.trimmed();
    if (!pending.isEmpty()) {
        emit statusMessage(pending);
    }
}

void VisionService::handleFinished(int exitCode)
{
    m_controller->onVisionStopped(exitCode);
}

QStringList VisionService::watchArguments() const
{
    QString windowId;
    QString windowTitle = QStringLiteral("NTE");
    if (m_selectedWindowIndex >= 0 && m_selectedWindowIndex < m_windows.size()) {
        const QVariantMap window = m_windows.at(m_selectedWindowIndex).toMap();
        windowId = window.value(QStringLiteral("id")).toString();
        windowTitle = window.value(QStringLiteral("title")).toString();
    }

    QStringList arguments = {QStringLiteral("-m"), QStringLiteral("autofish_vision.service"),
                             QStringLiteral("--watch"), QStringLiteral("--window-title"), windowTitle};
    if (!windowId.isEmpty()) {
        arguments << QStringLiteral("--window-id") << windowId;
    }
    if (m_controller->settings()->saveDebugFrames()) {
        arguments << QStringLiteral("--debug-dir") << QStringLiteral("debug")
                  << QStringLiteral("--debug-every") << QStringLiteral("30");
    }
    if (m_controller->settings()->debugMode()) {
        arguments << QStringLiteral("--live-debug")
                  << QStringLiteral("--live-debug-every") << QStringLiteral("20");
    }
    arguments << QStringLiteral("--interval") << QStringLiteral("0.005");
    return arguments;
}
