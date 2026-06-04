#pragma once

#include <QObject>
#include <QString>

class AppController;

class InputDevice final : public QObject
{
    Q_OBJECT

public:
    explicit InputDevice(AppController *controller, QObject *parent = nullptr);

    bool ensureOpen();
    void destroy();
    void sendKeyTap(int keyCode);
    void setKeyDown(int keyCode, bool down);
    void setReelDirection(int direction);
    int reelDirection() const;
    QString backendName() const;
    QString lastError() const;

private:
    enum class Backend {
        None,
        Uinput,
        Ydotool,
        Xdotool,
    };

    bool openUinput();
    bool selectCommandBackend();
    void closeUinput();
    bool runKeyCommand(int keyCode, bool down);
    QString keyName(int keyCode) const;
    int linuxKeyCode(int keyCode) const;
    void sendUinputEvent(unsigned short type, unsigned short code, int value);

    AppController *m_controller = nullptr;
    Backend m_backend = Backend::None;
    int m_uinputFd = -1;
    bool m_keyADown = false;
    bool m_keyDDown = false;
    int m_reelDirection = 0;
    QString m_lastError;
    QString m_backendExecutable;
};
