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

private:
    void sendUinputEvent(unsigned short type, unsigned short code, int value);

    AppController *m_controller = nullptr;
    int m_uinputFd = -1;
    bool m_keyADown = false;
    bool m_keyDDown = false;
    int m_reelDirection = 0;
};