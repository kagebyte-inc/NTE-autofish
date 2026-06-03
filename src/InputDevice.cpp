#include "InputDevice.h"

#include "AppController.h"

#include <QDateTime>
#include <QThread>

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
    if (m_uinputFd >= 0) {
        return true;
    }

    m_uinputFd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (m_uinputFd < 0) {
        return false;
    }

    if (::ioctl(m_uinputFd, UI_SET_EVBIT, EV_KEY) < 0
        || ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_SYN) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_A) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_D) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_F) < 0
        || ::ioctl(m_uinputFd, UI_SET_KEYBIT, KEY_ESC) < 0) {
        destroy();
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
        destroy();
        return false;
    }

    QThread::msleep(50);
    return true;
}

void InputDevice::destroy()
{
    if (m_uinputFd < 0) {
        return;
    }

    ::ioctl(m_uinputFd, UI_DEV_DESTROY);
    ::close(m_uinputFd);
    m_uinputFd = -1;
    m_keyADown = false;
    m_keyDDown = false;
    m_reelDirection = 0;
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
        return;
    }
    sendUinputEvent(EV_KEY, static_cast<unsigned short>(keyCode), down ? 1 : 0);
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
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

void InputDevice::setReelDirection(int direction)
{
    direction = qBound(-1, direction, 1);
    if (m_reelDirection == direction) {
        return;
    }

    if (!ensureOpen()) {
        if (direction != 0) {
            m_controller->setLastEvent(QStringLiteral("Failed to initialize /dev/uinput for A/D"));
        }
        return;
    }

    const bool wantA = direction < 0;
    const bool wantD = direction > 0;
    if (m_keyADown != wantA) {
        sendUinputEvent(EV_KEY, KEY_A, wantA ? 1 : 0);
        m_keyADown = wantA;
    }
    if (m_keyDDown != wantD) {
        sendUinputEvent(EV_KEY, KEY_D, wantD ? 1 : 0);
        m_keyDDown = wantD;
    }
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
    m_reelDirection = direction;
    m_controller->notifyReelDirectionChanged(direction);
}