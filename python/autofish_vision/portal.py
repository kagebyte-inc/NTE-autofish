from __future__ import annotations

import asyncio
from dataclasses import dataclass
import os
import secrets
import select
import shutil
import subprocess
import struct
import time
from typing import Any

import cv2
import numpy as np

from dbus_next import Message, MessageType, Variant
from dbus_next.aio import MessageBus


PORTAL_BUS_NAME = "org.freedesktop.portal.Desktop"
PORTAL_OBJECT_PATH = "/org/freedesktop/portal/desktop"
SCREENCAST_IFACE = "org.freedesktop.portal.ScreenCast"
REQUEST_IFACE = "org.freedesktop.portal.Request"


class PortalError(RuntimeError):
    pass


@dataclass(frozen=True)
class PortalStream:
    node_id: int
    properties: dict[str, Any]

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "node_id": self.node_id,
            "properties": self.properties,
        }


class PortalCaptureSession:
    def __init__(
        self,
        bus: MessageBus,
        session_handle: str,
        pipewire_fd: int,
        stream: PortalStream,
    ) -> None:
        self.bus = bus
        self.session_handle = session_handle
        self.pipewire_fd = pipewire_fd
        self.stream = stream
        self._target_property: str | None = None
        self._gst_process: subprocess.Popen[bytes] | None = None

    @classmethod
    def create(cls) -> "PortalCaptureSession":
        if shutil.which("gst-launch-1.0") is None:
            raise PortalError("gst-launch-1.0 is required for PipeWire capture")

        return asyncio.run(_create_session())

    def capture_frame(self) -> np.ndarray:
        if self._gst_process is not None:
            try:
                return self._read_png_frame()
            except PortalError:
                self._stop_gst_pipeline()

        errors: list[str] = []
        for target_property in ("path", "target-object"):
            try:
                self._start_gst_pipeline(target_property)
                frame = self._read_png_frame()
                self._target_property = target_property
                return frame
            except PortalError as exc:
                errors.append(str(exc))
                self._stop_gst_pipeline()

        raise PortalError("GStreamer PipeWire capture failed: " + " | ".join(errors))

    def _start_gst_pipeline(self, target_property: str) -> None:
        child_fd = os.dup(self.pipewire_fd)
        command = [
            "gst-launch-1.0",
            "-q",
            "pipewiresrc",
            f"fd={child_fd}",
            f"{target_property}={self.stream.node_id}",
            "!",
            "videoconvert",
            "!",
            "video/x-raw,format=RGB",
            "!",
            "pngenc",
            "compression-level=1",
            "!",
            "fdsink",
            "fd=1",
            "sync=false",
        ]
        try:
            self._gst_process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                pass_fds=(child_fd,),
            )
        finally:
            try:
                os.close(child_fd)
            except OSError:
                pass

    def _read_png_frame(self) -> np.ndarray:
        if self._gst_process is None or self._gst_process.stdout is None:
            raise PortalError("GStreamer pipeline is not running")

        try:
            png = _read_png(self._gst_process.stdout, timeout_seconds=8)
        except PortalError as exc:
            error = _process_stderr(self._gst_process)
            if error:
                raise PortalError(f"{exc}: {error}") from exc
            raise

        frame = cv2.imdecode(np.frombuffer(png, dtype=np.uint8), cv2.IMREAD_COLOR)
        if frame is None:
            raise PortalError("GStreamer produced an unreadable PNG frame")
        return frame

    def close(self) -> None:
        self._stop_gst_pipeline()
        try:
            os.close(self.pipewire_fd)
        except OSError:
            pass
        self.bus.disconnect()

    def _stop_gst_pipeline(self) -> None:
        if self._gst_process is None:
            return

        process = self._gst_process
        self._gst_process = None
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)


async def _create_session() -> PortalCaptureSession:
    bus = await MessageBus(negotiate_unix_fd=True).connect()
    try:
        session_handle = await _create_portal_session(bus)
        await _select_sources(bus, session_handle)
        streams = await _start_session(bus, session_handle)
        if not streams:
            raise PortalError("Portal returned no PipeWire streams")
        pipewire_fd = await _open_pipewire_remote(bus, session_handle)
    except Exception:
        bus.disconnect()
        raise

    return PortalCaptureSession(bus, session_handle, os.dup(pipewire_fd), streams[0])


async def _create_portal_session(bus: MessageBus) -> str:
    response = await _call_request(
        bus,
        "CreateSession",
        "a{sv}",
        [
            {
                "handle_token": Variant("s", _token("create")),
                "session_handle_token": Variant("s", _token("session")),
            }
        ],
    )
    session_handle = _variant_value(response.get("session_handle"))
    if not session_handle:
        raise PortalError("Portal did not return a session handle")
    return str(session_handle)


async def _select_sources(bus: MessageBus, session_handle: str) -> None:
    await _call_request(
        bus,
        "SelectSources",
        "oa{sv}",
        [
            session_handle,
            {
                "handle_token": Variant("s", _token("select")),
                "types": Variant("u", 3),
                "multiple": Variant("b", False),
                "cursor_mode": Variant("u", 2),
            },
        ],
    )


async def _start_session(bus: MessageBus, session_handle: str) -> list[PortalStream]:
    response = await _call_request(
        bus,
        "Start",
        "osa{sv}",
        [
            session_handle,
            "",
            {"handle_token": Variant("s", _token("start"))},
        ],
        timeout_seconds=120,
    )

    raw_streams = _variant_value(response.get("streams")) or []
    streams: list[PortalStream] = []
    for stream in raw_streams:
        if len(stream) < 2:
            continue
        node_id = int(stream[0])
        properties = _plain_value(stream[1])
        streams.append(PortalStream(node_id=node_id, properties=properties))
    return streams


async def _open_pipewire_remote(bus: MessageBus, session_handle: str) -> int:
    reply = await bus.call(
        Message(
            destination=PORTAL_BUS_NAME,
            path=PORTAL_OBJECT_PATH,
            interface=SCREENCAST_IFACE,
            member="OpenPipeWireRemote",
            signature="oa{sv}",
            body=[session_handle, {}],
        )
    )
    if reply.message_type == MessageType.ERROR:
        raise PortalError(reply.body[0] if reply.body else reply.error_name)
    if not reply.body:
        raise PortalError("Portal did not return a PipeWire fd")

    fd_index = int(reply.body[0])
    try:
        return int(reply.unix_fds[fd_index])
    except (AttributeError, IndexError) as exc:
        raise PortalError("Portal returned an invalid PipeWire fd") from exc


async def _call_request(
    bus: MessageBus,
    member: str,
    signature: str,
    body: list[Any],
    timeout_seconds: float = 30,
) -> dict[str, Any]:
    reply = await bus.call(
        Message(
            destination=PORTAL_BUS_NAME,
            path=PORTAL_OBJECT_PATH,
            interface=SCREENCAST_IFACE,
            member=member,
            signature=signature,
            body=body,
        )
    )
    if reply.message_type == MessageType.ERROR:
        raise PortalError(reply.body[0] if reply.body else reply.error_name)
    if not reply.body:
        raise PortalError(f"{member} returned no request handle")

    return await _wait_response(bus, str(reply.body[0]), timeout_seconds)


async def _wait_response(bus: MessageBus, request_path: str, timeout_seconds: float) -> dict[str, Any]:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[dict[str, Any]] = loop.create_future()

    def handler(message: Message) -> None:
        if (
            message.message_type != MessageType.SIGNAL
            or message.path != request_path
            or message.interface != REQUEST_IFACE
            or message.member != "Response"
        ):
            return
        if future.done():
            return

        response_code = int(message.body[0])
        results = _plain_value(message.body[1]) if len(message.body) > 1 else {}
        if response_code != 0:
            future.set_exception(PortalError(f"Portal request was cancelled or denied: {response_code}"))
            return
        future.set_result(results)

    bus.add_message_handler(handler)
    try:
        return await asyncio.wait_for(future, timeout=timeout_seconds)
    finally:
        bus.remove_message_handler(handler)


def _token(prefix: str) -> str:
    return f"autofish_{prefix}_{secrets.token_hex(8)}"


def _variant_value(value: Any) -> Any:
    return value.value if isinstance(value, Variant) else value


def _plain_value(value: Any) -> Any:
    value = _variant_value(value)
    if isinstance(value, dict):
        return {key: _plain_value(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_plain_value(item) for item in value]
    if isinstance(value, tuple):
        return tuple(_plain_value(item) for item in value)
    return value


def _read_png(stream: Any, timeout_seconds: float) -> bytes:
    signature = _read_exact(stream, 8, timeout_seconds)
    if signature != b"\x89PNG\r\n\x1a\n":
        raise PortalError("GStreamer output did not start with a PNG frame")

    chunks = bytearray(signature)
    while True:
        length_bytes = _read_exact(stream, 4, timeout_seconds)
        chunk_length = struct.unpack(">I", length_bytes)[0]
        chunk_type = _read_exact(stream, 4, timeout_seconds)
        chunk_data = _read_exact(stream, chunk_length, timeout_seconds)
        chunk_crc = _read_exact(stream, 4, timeout_seconds)
        chunks.extend(length_bytes)
        chunks.extend(chunk_type)
        chunks.extend(chunk_data)
        chunks.extend(chunk_crc)
        if chunk_type == b"IEND":
            return bytes(chunks)


def _read_exact(stream: Any, length: int, timeout_seconds: float) -> bytes:
    fd = stream.fileno()
    deadline = time.monotonic() + timeout_seconds
    data = bytearray()
    while len(data) < length:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise PortalError("Timed out waiting for a PipeWire frame")

        ready, _, _ = select.select([fd], [], [], remaining)
        if not ready:
            raise PortalError("Timed out waiting for a PipeWire frame")

        chunk = os.read(fd, length - len(data))
        if not chunk:
            raise PortalError("GStreamer pipeline ended before a complete frame was read")
        data.extend(chunk)

    return bytes(data)


def _process_stderr(process: subprocess.Popen[bytes]) -> str:
    if process.stderr is None or process.poll() is None:
        return ""
    try:
        return process.stderr.read().decode("utf-8", errors="replace").strip()
    except OSError:
        return ""
