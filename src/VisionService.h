#pragma once

#include <QObject>
#include <QProcess>
#include <QVariantList>

class AppController;

class VisionService final : public QObject
{
    Q_OBJECT

public:
    explicit VisionService(AppController *controller, QObject *parent = nullptr);

    QProcess &process();
    QVariantList windows() const;
    int selectedWindowIndex() const;
    QString selectedWindowLabel() const;

    void refreshWindows();
    void selectWindow(int index);
    void startWatch();
    void startPortal();
    void stop();
    void handleStdoutChunk(const QByteArray &chunk);
    void handleFinished(int exitCode);

signals:
    void statusMessage(const QString &message);
    void windowPickerRequired();
    void windowsChanged();
    void selectedWindowChanged();

private:
    void resetSessionState();
    QStringList watchArguments() const;

    AppController *m_controller = nullptr;
    QProcess m_visionProcess;
    QVariantList m_windows;
    int m_selectedWindowIndex = -1;
    QString m_stdoutBuffer;
    bool m_frameGeometryChecked = false;
};