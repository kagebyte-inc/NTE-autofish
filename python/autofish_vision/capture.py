from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import cv2
import numpy as np


@dataclass(frozen=True)
class WindowGeometry:
    window_id: str
    title: str
    x: int
    y: int
    width: int
    height: int
    pid: int | None = None
    process_name: str = ""
    icon_path: str = ""

    def to_json_dict(self) -> dict:
        return {
            "id": self.window_id,
            "title": self.title,
            "x": self.x,
            "y": self.y,
            "width": self.width,
            "height": self.height,
            "pid": self.pid,
            "process_name": self.process_name,
            "icon_path": self.icon_path,
        }


class WindowLookupError(RuntimeError):
    pass


class CaptureError(RuntimeError):
    pass


class WindowFinder:
    def list_windows(self) -> list[WindowGeometry]:
        errors: list[str] = []
        for backend in (
            self._list_with_hyprctl,
            self._list_with_swaymsg,
            self._list_with_xdotool,
            self._list_with_wmctrl,
        ):
            try:
                windows = backend()
            except WindowLookupError as exc:
                errors.append(str(exc))
                continue
            if windows:
                return windows

        hint = _wayland_hint()
        suffix = f" {hint}" if hint else ""
        details = "; ".join(errors)
        raise WindowLookupError(f"No window listing backend is available.{suffix} Backends: {details}")

    def find(self, title: str) -> WindowGeometry:
        needle = title.casefold()
        for window in self.list_windows():
            if needle in window.title.casefold() or needle in window.process_name.casefold():
                return window

        raise WindowLookupError(f"Window not found: {title}")

    def find_by_id(self, window_id: str) -> WindowGeometry:
        for window in self.list_windows():
            if window.window_id == window_id:
                return window

        raise WindowLookupError(f"Window not found: {window_id}")

    def _find_by_title_legacy(self, title: str) -> WindowGeometry:
        for backend in (self._find_with_xdotool, self._find_with_wmctrl):
            try:
                geometry = backend(title)
            except WindowLookupError:
                continue
            if geometry is not None:
                return geometry

        raise WindowLookupError(f"Window not found: {title}")

    def _list_with_xdotool(self) -> list[WindowGeometry]:
        if shutil.which("xdotool") is None:
            raise WindowLookupError("xdotool is not available")

        search = subprocess.run(
            ["xdotool", "search", "--onlyvisible", "--name", "."],
            check=False,
            capture_output=True,
            text=True,
        )
        if search.returncode != 0:
            raise WindowLookupError(search.stderr.strip() or "xdotool search failed")

        windows: list[WindowGeometry] = []
        for window_id in dict.fromkeys(search.stdout.splitlines()):
            geometry = self._geometry_from_xdotool_id(window_id)
            if geometry is not None:
                windows.append(geometry)

        return windows

    def _list_with_wmctrl(self) -> list[WindowGeometry]:
        if shutil.which("wmctrl") is None:
            raise WindowLookupError("wmctrl is not available")

        listing = subprocess.run(
            ["wmctrl", "-lpG"],
            check=False,
            capture_output=True,
            text=True,
        )
        if listing.returncode != 0:
            raise WindowLookupError(listing.stderr.strip() or "wmctrl failed")

        windows: list[WindowGeometry] = []
        for line in listing.stdout.splitlines():
            parts = line.split(maxsplit=8)
            if len(parts) < 9:
                continue
            window_id, _, pid_text, x, y, width, height, _, window_title = parts
            pid = _parse_pid(pid_text)
            process_name = _process_name(pid)
            windows.append(
                WindowGeometry(
                    window_id=window_id,
                    title=window_title,
                    x=int(x),
                    y=int(y),
                    width=int(width),
                    height=int(height),
                    pid=pid,
                    process_name=process_name,
                    icon_path=_find_icon_path(process_name),
                )
            )

        return windows

    def _list_with_hyprctl(self) -> list[WindowGeometry]:
        if shutil.which("hyprctl") is None:
            raise WindowLookupError("hyprctl is not available")

        result = subprocess.run(
            ["hyprctl", "clients", "-j"],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise WindowLookupError(result.stderr.strip() or "hyprctl clients failed")

        try:
            clients = json.loads(result.stdout)
        except json.JSONDecodeError as exc:
            raise WindowLookupError(f"hyprctl returned invalid JSON: {exc}") from exc

        windows: list[WindowGeometry] = []
        for client in clients:
            if client.get("hidden") or not client.get("mapped", True):
                continue

            at = client.get("at") or [0, 0]
            size = client.get("size") or [0, 0]
            width = int(size[0])
            height = int(size[1])
            if width <= 0 or height <= 0:
                continue

            pid = _parse_pid(str(client.get("pid", "")))
            process_name = _process_name(pid) or str(client.get("class", ""))
            windows.append(
                WindowGeometry(
                    window_id=str(client.get("address", "")),
                    title=str(client.get("title", "")),
                    x=int(at[0]),
                    y=int(at[1]),
                    width=width,
                    height=height,
                    pid=pid,
                    process_name=process_name,
                    icon_path=_find_icon_path(process_name),
                )
            )

        return windows

    def _list_with_swaymsg(self) -> list[WindowGeometry]:
        if shutil.which("swaymsg") is None:
            raise WindowLookupError("swaymsg is not available")

        result = subprocess.run(
            ["swaymsg", "-t", "get_tree"],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise WindowLookupError(result.stderr.strip() or "swaymsg get_tree failed")

        try:
            tree = json.loads(result.stdout)
        except json.JSONDecodeError as exc:
            raise WindowLookupError(f"swaymsg returned invalid JSON: {exc}") from exc

        windows: list[WindowGeometry] = []
        for node in _walk_sway_tree(tree):
            rect = node.get("rect") or {}
            width = int(rect.get("width", 0))
            height = int(rect.get("height", 0))
            if width <= 0 or height <= 0:
                continue

            pid = _parse_pid(str(node.get("pid", "")))
            props = node.get("window_properties") or {}
            process_name = (
                _process_name(pid)
                or str(node.get("app_id") or "")
                or str(props.get("class") or "")
            )
            windows.append(
                WindowGeometry(
                    window_id=str(node.get("id", "")),
                    title=str(node.get("name") or ""),
                    x=int(rect.get("x", 0)),
                    y=int(rect.get("y", 0)),
                    width=width,
                    height=height,
                    pid=pid,
                    process_name=process_name,
                    icon_path=_find_icon_path(process_name),
                )
            )

        return windows

    def _find_with_xdotool(self, title: str) -> WindowGeometry | None:
        if shutil.which("xdotool") is None:
            raise WindowLookupError("xdotool is not available")

        search = subprocess.run(
            ["xdotool", "search", "--name", title],
            check=False,
            capture_output=True,
            text=True,
        )
        if search.returncode != 0:
            raise WindowLookupError(search.stderr.strip() or f"Window not found: {title}")

        for window_id in search.stdout.splitlines():
            geometry = self._geometry_from_xdotool_id(window_id)
            if geometry is not None and title.casefold() in geometry.title.casefold():
                return geometry

        raise WindowLookupError(f"Window geometry not found: {title}")

    def _geometry_from_xdotool_id(self, window_id: str) -> WindowGeometry | None:
        geometry = subprocess.run(
            ["xdotool", "getwindowgeometry", "--shell", window_id],
            check=False,
            capture_output=True,
            text=True,
        )
        if geometry.returncode != 0:
            return None

        values = _parse_shell_values(geometry.stdout)
        width = int(values.get("WIDTH", "0"))
        height = int(values.get("HEIGHT", "0"))
        if width <= 0 or height <= 0:
            return None

        title = subprocess.run(
            ["xdotool", "getwindowname", window_id],
            check=False,
            capture_output=True,
            text=True,
        )
        pid_result = subprocess.run(
            ["xdotool", "getwindowpid", window_id],
            check=False,
            capture_output=True,
            text=True,
        )
        pid = _parse_pid(pid_result.stdout.strip()) if pid_result.returncode == 0 else None
        process_name = _process_name(pid)

        return WindowGeometry(
            window_id=window_id,
            title=title.stdout.strip() if title.returncode == 0 else "",
            x=int(values.get("X", "0")),
            y=int(values.get("Y", "0")),
            width=width,
            height=height,
            pid=pid,
            process_name=process_name,
            icon_path=_find_icon_path(process_name),
        )

    def _find_with_wmctrl(self, title: str) -> WindowGeometry | None:
        if shutil.which("wmctrl") is None:
            raise WindowLookupError("wmctrl is not available")

        listing = subprocess.run(
            ["wmctrl", "-lG"],
            check=False,
            capture_output=True,
            text=True,
        )
        if listing.returncode != 0:
            raise WindowLookupError(listing.stderr.strip() or "wmctrl failed")

        needle = title.casefold()
        for line in listing.stdout.splitlines():
            parts = line.split(maxsplit=7)
            if len(parts) < 8:
                continue
            window_id, _, x, y, width, height, _, window_title = parts
            if needle not in window_title.casefold():
                continue
            return WindowGeometry(
                window_id=window_id,
                title=window_title,
                x=int(x),
                y=int(y),
                width=int(width),
                height=int(height),
                process_name="",
            )

        raise WindowLookupError(f"Window not found: {title}")


class ScreenCapture:
    def capture(self, geometry: WindowGeometry) -> np.ndarray:
        if os.environ.get("WAYLAND_DISPLAY") and shutil.which("grim") is not None:
            return self._capture_with_grim(geometry)

        try:
            import mss
        except ImportError as exc:
            raise CaptureError("mss is not installed") from exc

        monitor = {
            "left": geometry.x,
            "top": geometry.y,
            "width": geometry.width,
            "height": geometry.height,
        }
        try:
            with mss.mss() as screen:
                frame = np.array(screen.grab(monitor))
        except Exception as exc:
            raise CaptureError(str(exc)) from exc

        return cv2.cvtColor(frame, cv2.COLOR_BGRA2BGR)

    def _capture_with_grim(self, geometry: WindowGeometry) -> np.ndarray:
        region = f"{geometry.x},{geometry.y} {geometry.width}x{geometry.height}"
        with tempfile.NamedTemporaryFile(suffix=".png") as image:
            result = subprocess.run(
                ["grim", "-g", region, image.name],
                check=False,
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                raise CaptureError(result.stderr.strip() or "grim capture failed")

            frame = cv2.imread(image.name, cv2.IMREAD_COLOR)
            if frame is None:
                raise CaptureError("grim wrote an unreadable image")

        return frame


def _parse_shell_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", maxsplit=1)
        values[key.strip()] = value.strip()
    return values


def _walk_sway_tree(node: dict) -> list[dict]:
    nodes: list[dict] = []
    stack = [node]
    while stack:
        current = stack.pop()
        if current.get("type") in {"con", "floating_con"} and current.get("name"):
            nodes.append(current)
        stack.extend(current.get("nodes") or [])
        stack.extend(current.get("floating_nodes") or [])
    return nodes


def _parse_pid(value: str) -> int | None:
    try:
        pid = int(value)
    except ValueError:
        return None
    return pid if pid > 0 else None


def _process_name(pid: int | None) -> str:
    if pid is None:
        return ""

    comm = Path(f"/proc/{pid}/comm")
    try:
        return comm.read_text(encoding="utf-8").strip()
    except OSError:
        return ""


def _wayland_hint() -> str:
    if not os.environ.get("WAYLAND_DISPLAY"):
        return ""
    return (
        "Wayland is active; install a compositor-specific backend "
        "(hyprctl for Hyprland, swaymsg+grim for Sway/wlroots) or use an X11 session."
    )


@lru_cache(maxsize=128)
def _find_icon_path(process_name: str) -> str:
    if not process_name:
        return ""

    candidates = _icon_candidates(process_name)
    desktop_icon = _icon_from_desktop_files(candidates)
    if desktop_icon:
        return desktop_icon

    for candidate in candidates:
        icon_path = _resolve_icon_name(candidate)
        if icon_path:
            return icon_path

    return ""


def _icon_candidates(process_name: str) -> list[str]:
    normalized = process_name.strip().casefold()
    names = [normalized]
    if normalized.endswith(".exe"):
        names.append(normalized[:-4])
    if "-" in normalized:
        names.append(normalized.replace("-", ""))
    return [name for name in dict.fromkeys(names) if name]


def _icon_from_desktop_files(candidates: list[str]) -> str:
    search_dirs = [
        Path.home() / ".local/share/applications",
        Path("/usr/local/share/applications"),
        Path("/usr/share/applications"),
    ]

    for directory in search_dirs:
        if not directory.is_dir():
            continue
        for desktop in directory.glob("*.desktop"):
            try:
                text = desktop.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            lowered = text.casefold()
            if not any(candidate in lowered for candidate in candidates):
                continue
            icon_name = _desktop_value(text, "Icon")
            if not icon_name:
                continue
            resolved = _resolve_icon_name(icon_name)
            if resolved:
                return resolved

    return ""


def _desktop_value(text: str, key: str) -> str:
    prefix = f"{key}="
    for line in text.splitlines():
        if line.startswith(prefix):
            return line[len(prefix) :].strip()
    return ""


def _resolve_icon_name(icon_name: str) -> str:
    if not icon_name:
        return ""

    path = Path(os.path.expanduser(icon_name))
    if path.is_absolute() and path.exists():
        return str(path)

    extensions = (".png", ".svg", ".xpm")
    roots = [
        Path.home() / ".local/share/icons",
        Path.home() / ".icons",
        Path("/usr/share/pixmaps"),
        Path("/usr/share/icons/hicolor"),
    ]
    for root in roots:
        if not root.exists():
            continue
        for extension in extensions:
            direct = root / f"{icon_name}{extension}"
            if direct.exists():
                return str(direct)
        for found in root.rglob(f"{icon_name}.*"):
            if found.suffix.casefold() in extensions:
                return str(found)

    return ""
