#include "AppController.h"

#include <QDateTime>
#include <QThread>

#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

bool AppController::ensureUinput()
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
        destroyUinput();
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
        destroyUinput();
        return false;
    }

    QThread::msleep(50);
    return true;
}

void AppController::destroyUinput()
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
    m_reelPulseDirection = 0;
    m_reelPulseEndMs = 0;
    m_nextReelPulseMs = 0;
    m_reelGuardSettleUntilMs = 0;
    m_lastReelPidMs = 0;
    m_reelPidIntegral = 0.0;
    m_reelPidPreviousMarker = 0.0;
    m_reelPidDFiltered = 0.0;
    m_reelPidFirst = true;
}

void AppController::sendKeyTap(int keyCode)
{
    setKeyDown(keyCode, true);
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
    QThread::msleep(30);
    setKeyDown(keyCode, false);
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
}

void AppController::setKeyDown(int keyCode, bool down)
{
    if (!ensureUinput()) {
        return;
    }
    sendUinputEvent(EV_KEY, static_cast<unsigned short>(keyCode), down ? 1 : 0);
    sendUinputEvent(EV_SYN, SYN_REPORT, 0);
}

void AppController::sendUinputEvent(unsigned short type, unsigned short code, int value)
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

void AppController::setReelDirection(int direction)
{
    direction = qBound(-1, direction, 1);
    if (m_reelDirection == direction) {
        return;
    }

    if (!ensureUinput()) {
        if (direction != 0) {
            setLastEvent(QStringLiteral("Failed to initialize /dev/uinput for A/D"));
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

    const QString key = direction < 0 ? QStringLiteral("A") : direction > 0 ? QStringLiteral("D") : QStringLiteral("none");
    if (!m_debugMode) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastReelControlUiMs < 160) {
        return;
    }
    m_lastReelControlUiMs = now;

    QString mode = QStringLiteral("boundary");
    QString action = QStringLiteral("hold");
    if (m_reelControlMode == ReelControlMode::ChizukuoPid) {
        mode = QStringLiteral("chizukuo pid");
        action = QStringLiteral("pulse");
    } else if (m_reelControlMode == ReelControlMode::LegacyPid) {
        mode = QStringLiteral("stable");
        action = QStringLiteral("pulse");
    } else if (m_reelControlMode == ReelControlMode::KagebaitoGuard) {
        mode = QStringLiteral("experimental");
        action = QStringLiteral("guard");
    }
    setReelControl(QStringLiteral("%1 %2 %3  target %4  marker %5  error %6")
                       .arg(mode)
                       .arg(action)
                       .arg(key)
                       .arg(m_reelTargetCenter, 0, 'f', 1)
                       .arg(m_reelMarkerCenter, 0, 'f', 1)
                       .arg(m_reelTargetCenter - m_reelMarkerCenter, 0, 'f', 1));
}
