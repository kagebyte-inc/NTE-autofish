#include "InputDevice.h"

#include "AppController.h"

#include <QDateTime>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

InputDevice::InputDevice(AppController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
}

bool InputDevice::ensureOpen()
{
    if (m_backend == Backend::Uinput && m_uinputFd >= 0) {
        return true;
    }
    if (m_backend == Backend::Ydotool || m_backend == Backend::Xdotool) {
        return true;
    }

    if (openUinput()) {
        return true;
    }

    return selectCommandBackend();
}

bool InputDevice::openUinput()
{
    m_uinputFd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (m_uinputFd < 0) {
        m_lastError = QStringLiteral("uinput unavailable: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }

    if (::ioctl(m_uinputFd, UI_SET_EVBIT, EV_KEY) < 0
        || ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_SYN) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_A) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_D) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_F) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_ESC) < 0) {
        m_lastError = QStringLiteral("uinput capability setup failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        closeUinput();
        return false;
    }

    uinput_user_dev device;
    std::memset(&device, 0, sizeof(device));
    std::strncpy(device.name, "autofish-uinput", UINPUT_MAX_NAME_SIZE - 1);
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x1209;
    device.id.product = 0x0001;
    device.id.version = 1;

    if (::write(m_uinputFd, &device, sizeof(device)) != sizeof(device)
        || ::ioctl(m_uinputFd, UI_DEV_CREATE) < 0) {
        m_lastError = QStringLiteral("uinput device creation failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        closeUinput();
        return false;
    }

    QThread::msleep(150);
    m_backend = Backend::Uinput;
    m_lastError.clear();
    return true;
}

bool InputDevice::selectCommandBackend()
{
    const QString ydotool = QStandardPaths::findExecutable(QStringLiteral("ydotool"));
    if (!ydotool.isEmpty()) {
        m_backend = Backend::Ydotool;
        m_backendExecutable = ydotool;
        return true;
    }

    const QString xdotool = QStandardPaths::findExecutable(QStringLiteral("xdotool"));
    if (!xdotool.isEmpty()) {
        m_backend = Backend::Xdotool;
        m_backendExecutable = xdotool;
        return true;
    }

    if (m_lastError.isEmpty()) {
        m_lastError = QStringLiteral("No usable input backend found (/dev/uinput, ydotool, xdotool)");
    } else {
        m_lastError += QStringLiteral("; no ydotool/xdotool fallback found");
    }
    return false;
}

void InputDevice::destroy()
{
    if (m_keyADown) {
        if (m_backend == Backend::Uinput) {
            sendUinputEvent(EV_KEY, KEY_A, 0);
        } else {
            runKeyCommand(KEY_A, false);
        }
    }
    if (m_keyDDown) {
        if (m_backend == Backend::Uinput) {
            sendUinputEvent(EV_KEY, KEY_D, 0);
        } else {
            runKeyCommand(KEY_D, false);
        }
    }
    if (m_backend == Backend::Uinput && m_uinputFd >= 0) {
        sendUinputEvent(EV_SYN, SYN_REPORT, 0);
    }

    closeUinput();
    m_backend = Backend::None;
    m_backendExecutable.clear();
    m_keyADown = false;
    m_keyDDown = false;
    m_reelDirection = 0;
}

void InputDevice::closeUinput()
{
    if (m_uinputFd < 0) {
        return;
    }
    ::ioctl(m_uinputFd, UI_DEV_DESTROY);
    ::close(m_uinputFd);
    m_uinputFd = -1;
}

void InputDevice::sendKeyTap(int keyCode)
{
    setKeyDown(keyCode, true);
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
    QThread::msleep(30);
    setKeyDown(keyCode, false);
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
}

void InputDevice::setKeyDown(int keyCode, bool down)
{
    if (!ensureOpen()) {
        if (m_controller) {
            m_controller->setLastEvent(QStringLiteral("Failed to initialize input backend: %1").arg(m_lastError));
        }
        return;
    }

    if (m_backend == Backend::Uinput) {
        sendUinputEvent(EV_KEY, static_cast<unsigned short>(keyCode), down ? 1 : 0);
        sendUinputEvent(EV_SYN, SYN_REPORT, 0);
        return;
    }

    if (!runKeyCommand(keyCode, down) && m_controller) {
        m_controller->setLastEvent(QStringLiteral("Input backend %1 failed: %2").arg(backendName(), m_lastError));
    }
}

void InputDevice::sendUinputEvent(unsigned short type, unsigned short code, int value)
{
    if (m_uinputFd < 0) {
        return;
    }

    input_event event;
    std::memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    ::write(m_uinputFd, &event, sizeof(event));
}

int InputDevice::reelDirection() const
{
    return m_reelDirection;
}

QString InputDevice::backendName() const
{
    switch (m_backend) {
    case Backend::Uinput:
        return QStringLiteral("uinput");
    case Backend::Ydotool:
        return QStringLiteral("ydotool");
    case Backend::Xdotool:
        return QStringLiteral("xdotool");
    case Backend::None:
        break;
    }
    return QStringLiteral("none");
}

QString InputDevice::lastError() const
{
    return m_lastError;
}

void InputDevice::setReelDirection(int direction)
{
    direction = qBound(-1, direction, 1);
    if (m_reelDirection == direction) {
        return;
    }

    if (!ensureOpen()) {
        if (direction != 0) {
            m_controller->setLastEvent(QStringLiteral("Failed to initialize input backend for A/D: %1").arg(m_lastError));
        }
        return;
    }

    const bool wantA = direction < 0;
    const bool wantD = direction > 0;
    if (m_keyADown != wantA) {
        if (m_backend == Backend::Uinput) {
            sendUinputEvent(EV_KEY, KEY_A, wantA ? 1 : 0);
        } else if (!runKeyCommand(KEY_A, wantA)) {
            m_controller->setLastEvent(QStringLiteral("Input backend %1 failed for A: %2").arg(backendName(), m_lastError));
            return;
        }
        m_keyADown = wantA;
    }
    if (m_keyDDown != wantD) {
        if (m_backend == Backend::Uinput) {
            sendUinputEvent(EV_KEY, KEY_D, wantD ? 1 : 0);
        } else if (!runKeyCommand(KEY_D, wantD)) {
            m_controller->setLastEvent(QStringLiteral("Input backend %1 failed for D: %2").arg(backendName(), m_lastError));
            return;
        }
        m_keyDDown = wantD;
    }
    if (m_backend == Backend::Uinput) {
        sendUinputEvent(EV_SYN, SYN_REPORT, 0);
    }
    m_reelDirection = direction;
    m_controller->notifyReelDirectionChanged(direction);
}

bool InputDevice::runKeyCommand(int keyCode, bool down)
{
    if (m_backend != Backend::Ydotool && m_backend != Backend::Xdotool) {
        return false;
    }

    QStringList arguments;
    if (m_backend == Backend::Ydotool) {
        const int code = linuxKeyCode(keyCode);
        if (code < 0) {
            m_lastError = QStringLiteral("ydotool has no key code for %1").arg(keyCode);
            return false;
        }
        arguments = {QStringLiteral("key"), QStringLiteral("%1:%2").arg(code).arg(down ? 1 : 0)};
    } else if (m_backend == Backend::Xdotool) {
        const QString key = keyName(keyCode);
        if (key.isEmpty()) {
            m_lastError = QStringLiteral("xdotool has no key name for %1").arg(keyCode);
            return false;
        }
        arguments = {down ? QStringLiteral("keydown") : QStringLiteral("keyup"), key};
    }

    QProcess process;
    process.start(m_backendExecutable, arguments);
    if (!process.waitForStarted(500)) {
        m_lastError = QStringLiteral("failed to start %1").arg(m_backendExecutable);
        return false;
    }
    if (!process.waitForFinished(1000)) {
        process.kill();
        process.waitForFinished(200);
        m_lastError = QStringLiteral("%1 timed out").arg(backendName());
        return false;
    }
    if (process.exitCode() != 0) {
        const QString output = QString::fromUtf8(process.readAllStandardError() + process.readAllStandardOutput()).trimmed();
        m_lastError = output.isEmpty()
            ? QStringLiteral("%1 exited with code %2").arg(backendName()).arg(process.exitCode())
            : output;
        return false;
    }
    return true;
}

QString InputDevice::keyName(int keyCode) const
{
    switch (keyCode) {
    case KEY_A:
        return QStringLiteral("a");
    case KEY_D:
        return QStringLiteral("d");
    case KEY_F:
        return QStringLiteral("f");
    case KEY_ESC:
        return QStringLiteral("Escape");
    default:
        break;
    }
    return {};
}

int InputDevice::linuxKeyCode(int keyCode) const
{
    switch (keyCode) {
    case KEY_A:
        return 30;
    case KEY_D:
        return 32;
    case KEY_F:
        return 33;
    case KEY_ESC:
        return 1;
    default:
        break;
    }
    return -1;
}
