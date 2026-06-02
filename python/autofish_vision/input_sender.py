from __future__ import annotations

import fcntl
import os
import shutil
import subprocess
import struct
import time


class InputSendError(RuntimeError):
    pass


_LINUX_KEY_CODES = {
    "A": 30,
    "D": 32,
    "ESC": 1,
    "ESCAPE": 1,
    "F": 33,
}


def send_key(key: str) -> dict:
    normalized = key.strip().upper()
    if not normalized:
        raise InputSendError("Key is empty")

    attempts: list[str] = []
    for backend in (_send_with_uinput, _send_with_ydotool, _send_with_dotool, _send_with_wtype, _send_with_xdotool):
        try:
            return backend(normalized)
        except InputSendError as exc:
            attempts.append(str(exc))

    raise InputSendError("No input backend worked: " + " | ".join(attempts))


def _send_with_uinput(key: str) -> dict:
    code = _LINUX_KEY_CODES.get(key)
    if code is None:
        raise InputSendError(f"uinput has no key code mapping for {key}")

    try:
        fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    except OSError as exc:
        raise InputSendError(f"uinput unavailable: {exc}") from exc

    try:
        _uinput_ioctl(fd, 0x40045564, 0x01)  # UI_SET_EVBIT, EV_KEY
        _uinput_ioctl(fd, 0x40045564, 0x00)  # UI_SET_EVBIT, EV_SYN
        _uinput_ioctl(fd, 0x40045565, code)  # UI_SET_KEYBIT

        device = struct.pack(
            "80sHHHHI" + "i" * 256,
            b"autofish-uinput",
            0x03,  # BUS_USB
            0x1209,
            0x0001,
            1,
            0,
            *([0] * 256),
        )
        os.write(fd, device)
        fcntl.ioctl(fd, 0x5501)  # UI_DEV_CREATE
        time.sleep(0.05)
        _uinput_event(fd, 0x01, code, 1)
        _uinput_event(fd, 0x00, 0, 0)
        time.sleep(0.03)
        _uinput_event(fd, 0x01, code, 0)
        _uinput_event(fd, 0x00, 0, 0)
        time.sleep(0.05)
        fcntl.ioctl(fd, 0x5502)  # UI_DEV_DESTROY
    except OSError as exc:
        raise InputSendError(f"uinput failed: {exc}") from exc
    finally:
        os.close(fd)

    return {"backend": "uinput", "key": key}


def _uinput_ioctl(fd: int, request: int, value: int) -> None:
    fcntl.ioctl(fd, request, value)


def _uinput_event(fd: int, event_type: int, code: int, value: int) -> None:
    os.write(fd, struct.pack("qqHHi", 0, 0, event_type, code, value))


def _send_with_ydotool(key: str) -> dict:
    executable = shutil.which("ydotool")
    if executable is None:
        raise InputSendError("ydotool not found")

    code = _LINUX_KEY_CODES.get(key)
    if code is None:
        raise InputSendError(f"ydotool has no key code mapping for {key}")

    _run([executable, "key", f"{code}:1", f"{code}:0"], "ydotool")
    return {"backend": "ydotool", "key": key}


def _send_with_dotool(key: str) -> dict:
    executable = shutil.which("dotool")
    if executable is None:
        raise InputSendError("dotool not found")

    command = f"key {key.lower()}\n"
    _run([executable], "dotool", input_text=command)
    return {"backend": "dotool", "key": key}


def _send_with_wtype(key: str) -> dict:
    executable = shutil.which("wtype")
    if executable is None:
        raise InputSendError("wtype not found")

    _run([executable, "-k", key.lower()], "wtype")
    return {"backend": "wtype", "key": key}


def _send_with_xdotool(key: str) -> dict:
    executable = shutil.which("xdotool")
    if executable is None:
        raise InputSendError("xdotool not found")

    _run([executable, "key", "--clearmodifiers", key.lower()], "xdotool")
    return {"backend": "xdotool", "key": key}


def _run(command: list[str], backend: str, input_text: str | None = None) -> None:
    result = subprocess.run(
        command,
        input=input_text,
        text=True,
        check=False,
        capture_output=True,
        timeout=2.0,
    )
    if result.returncode != 0:
        error = (result.stderr or result.stdout).strip()
        raise InputSendError(f"{backend} failed: {error or result.returncode}")
