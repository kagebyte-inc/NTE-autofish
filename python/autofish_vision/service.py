from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import time

import base64
import cv2
import numpy as np

from .capture import CaptureError, ScreenCapture, WindowFinder, WindowLookupError
from .detector import BiteDetector
from .elements import WindowElementAnalyzer
from .input_sender import InputSendError, send_key
from .portal import PortalCaptureSession


def emit(payload: dict) -> None:
    print(json.dumps(payload, ensure_ascii=True), flush=True)


def load_image(path: str) -> np.ndarray:
    frame = cv2.imread(path, cv2.IMREAD_COLOR)
    if frame is None:
        raise FileNotFoundError(f"Could not read image: {path}")
    return frame


def run_once(image_path: str, debug_dir: str, live_debug: bool = False) -> int:
    detector = BiteDetector()
    analyzer = WindowElementAnalyzer(confirm_fish_hooked_frames=1)
    frame = load_image(image_path)
    motion = detector.analyze(frame).to_json_dict()
    elements = analyzer.analyze(frame)
    debug_path = save_debug_frame(frame, elements, debug_dir, "once") if debug_dir else ""
    event = {
        "event": "frame_analyzed",
        "confidence": 1.0,
        "details": {
            "source": "image",
            "motion": motion,
            "elements": [item.to_json_dict() for item in elements],
            "debug_overlay": debug_path,
        },
    }
    if live_debug:
        b64 = make_live_debug_image_b64(frame, elements)
        if b64:
            event["debug_image"] = b64
    emit(event)
    return 0


def run_list_windows() -> int:
    finder = WindowFinder()
    try:
        windows = [window.to_json_dict() for window in finder.list_windows()]
    except WindowLookupError as exc:
        emit({"event": "window_list_error", "confidence": 0.0, "details": {"error": str(exc)}})
        return 1

    emit({"event": "window_list", "confidence": 1.0, "details": {"windows": windows}})
    return 0


def run_press_key(key: str) -> int:
    try:
        details = send_key(key)
    except InputSendError as exc:
        emit({"event": "input_send_error", "confidence": 0.0, "details": {"error": str(exc), "key": key}})
        return 1

    emit({"event": "input_sent", "confidence": 1.0, "details": details})
    return 0


def run_watch(
    window_title: str,
    window_id: str,
    interval_seconds: float,
    synthetic: bool,
    portal: bool,
    debug_dir: str,
    debug_every: int,
    live_debug: bool = False,
) -> int:
    if portal:
        return run_portal_watch(interval_seconds, debug_dir, debug_every, live_debug)

    detector = BiteDetector()
    analyzer = WindowElementAnalyzer()
    finder = WindowFinder()
    capture = ScreenCapture()
    emit(
        {
            "event": "ready",
            "confidence": 1.0,
            "details": {
                "mode": "watch",
                "window_title": window_title,
                "window_id": window_id,
                "synthetic": synthetic,
            },
        }
    )

    frame_index = 0
    while True:
        frame_index += 1
        if synthetic:
            frame = create_synthetic_frame()
            geometry = None
        else:
            try:
                geometry = finder.find_by_id(window_id) if window_id else finder.find(window_title)
            except WindowLookupError as exc:
                emit({"event": "window_not_found", "confidence": 0.0, "details": {"error": str(exc)}})
                time.sleep(interval_seconds)
                continue

            try:
                frame = capture.capture(geometry)
            except CaptureError as exc:
                emit(
                    {
                        "event": "capture_unavailable",
                        "confidence": 0.0,
                        "details": {"error": str(exc), "window": geometry.to_json_dict()},
                    }
                )
                time.sleep(interval_seconds)
                continue

        motion = detector.analyze(frame).to_json_dict()
        elements = analyzer.analyze(frame)
        debug_path = maybe_save_debug_frame(frame, elements, debug_dir, debug_every, frame_index, "window")
        event = {
            "event": "window_analyzed",
            "confidence": 1.0,
            "details": {
                "window": geometry.to_json_dict() if geometry is not None else None,
                "motion": motion,
                "elements": [item.to_json_dict() for item in elements],
                "debug_overlay": debug_path,
                "frame": {"width": int(frame.shape[1]), "height": int(frame.shape[0])},
            },
        }
        if live_debug:
            b64 = make_live_debug_image_b64(frame, elements)
            if b64:
                event["debug_image"] = b64
        emit(event)
        time.sleep(interval_seconds)


def run_portal_watch(interval_seconds: float, debug_dir: str, debug_every: int, live_debug: bool = False) -> int:
    detector = BiteDetector()
    analyzer = WindowElementAnalyzer()
    emit({"event": "portal_picker_opening", "confidence": 1.0, "details": {}})

    try:
        session = PortalCaptureSession.create()
    except Exception as exc:
        emit({"event": "portal_unavailable", "confidence": 0.0, "details": {"error": str(exc)}})
        return 1

    emit(
        {
            "event": "portal_ready",
            "confidence": 1.0,
            "details": {"stream": session.stream.to_json_dict()},
        }
    )

    try:
        frame_index = 0
        while True:
            frame_index += 1
            try:
                frame = session.capture_frame()
            except Exception as exc:
                emit({"event": "capture_unavailable", "confidence": 0.0, "details": {"error": str(exc)}})
                time.sleep(interval_seconds)
                continue

            motion = detector.analyze(frame).to_json_dict()
            elements = analyzer.analyze(frame)
            debug_path = maybe_save_debug_frame(frame, elements, debug_dir, debug_every, frame_index, "portal")
            event = {
                "event": "portal_frame_analyzed",
                "confidence": 1.0,
                "details": {
                    "stream": session.stream.to_json_dict(),
                    "motion": motion,
                    "elements": [item.to_json_dict() for item in elements],
                    "debug_overlay": debug_path,
                    "frame": {"width": int(frame.shape[1]), "height": int(frame.shape[0])},
                },
            }
            if live_debug:
                b64 = make_live_debug_image_b64(frame, elements)
                if b64:
                    event["debug_image"] = b64
            emit(event)
            time.sleep(interval_seconds)
    finally:
        session.close()


def create_synthetic_frame() -> np.ndarray:
    frame = np.zeros((160, 240, 3), dtype=np.uint8)
    cv2.circle(frame, (120, 80), 8, (255, 255, 255), -1)
    cv2.rectangle(frame, (30, 30), (52, 52), (0, 0, 255), -1)
    return frame


def maybe_save_debug_frame(
    frame: np.ndarray,
    elements: list,
    debug_dir: str,
    debug_every: int,
    frame_index: int,
    prefix: str,
) -> str:
    if not debug_dir or debug_every <= 0 or frame_index % debug_every != 0:
        return ""
    return save_debug_frame(frame, elements, debug_dir, f"{prefix}_{frame_index:06d}")


def save_debug_frame(frame: np.ndarray, elements: list, debug_dir: str, name: str) -> str:
    output_dir = Path(debug_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_path = output_dir / f"{name}_raw.png"
    cv2.imwrite(str(raw_path), frame)

    overlay = build_debug_overlay(frame, elements)

    output_path = output_dir / f"{name}.png"
    cv2.imwrite(str(output_path), overlay)
    return str(output_path)


def _element_color(name: str) -> tuple[int, int, int]:
    colors = {
        "fishing_panel": (255, 180, 0),
        "start_fishing_button": (0, 255, 0),
        "close_button": (0, 255, 255),
        "fish_card": (255, 0, 255),
        "rod_slot": (255, 80, 80),
        "bait_slot": (80, 180, 255),
        "water_roi": (255, 255, 0),
        "fish_hooked_prompt": (0, 0, 255),
        "hook_action_button": (0, 160, 255),
        "hook_blue_trigger": (255, 128, 0),
        "active_fishing_close_button": (0, 255, 255),
        "fishing_result_screen": (0, 220, 255),
        "reel_green_target": (0, 255, 120),
        "reel_yellow_marker": (0, 255, 255),
        "red_region": (0, 0, 255),
        "bright_region": (255, 255, 255),
    }
    return colors.get(name, (180, 180, 180))


def build_debug_overlay(frame: np.ndarray, elements: list) -> np.ndarray:
    overlay = frame.copy()
    for element in elements:
        x, y, width, height = element.bbox
        color = _element_color(element.name)
        cv2.rectangle(overlay, (x, y), (x + width, y + height), color, 2)
        cv2.putText(
            overlay,
            element.name,
            (x, max(16, y - 6)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            color,
            1,
            cv2.LINE_AA,
        )
    return overlay


def make_live_debug_image_b64(frame: np.ndarray, elements: list, max_width: int = 320) -> str:
    overlay = build_debug_overlay(frame, elements)
    h, w = overlay.shape[:2]
    if w > max_width:
        scale = max_width / float(w)
        new_h = int(h * scale)
        overlay = cv2.resize(overlay, (max_width, new_h), interpolation=cv2.INTER_AREA)
    success, buf = cv2.imencode(".png", overlay)
    if success:
        return base64.b64encode(buf.tobytes()).decode("ascii")
    return ""


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--list-windows", action="store_true", help="List capturable windows and exit.")
    mode.add_argument("--once", action="store_true", help="Analyze one image and exit.")
    mode.add_argument("--watch", action="store_true", help="Run continuously.")
    mode.add_argument("--press-key", action="store_true", help="Send a key press through an available desktop input backend.")
    parser.add_argument("--image", help="Image path for --once.")
    parser.add_argument("--key", default="F", help="Key for --press-key.")
    parser.add_argument("--window-id", default="", help="Exact window id from --list-windows.")
    parser.add_argument("--window-title", default="NTE", help="Game window title substring.")
    parser.add_argument("--interval", type=float, default=0.02)
    parser.add_argument("--synthetic", action="store_true", help="Use generated frames instead of screen capture.")
    parser.add_argument("--portal", action="store_true", help="Use xdg-desktop-portal ScreenCast picker.")
    parser.add_argument("--debug-dir", default="", help="Write frames with detector overlays to this directory.")
    parser.add_argument("--debug-every", type=int, default=5, help="Save every Nth analyzed frame when --debug-dir is set.")
    parser.add_argument("--live-debug", action="store_true", help="Include small base64 debug image in JSON events for live UI preview.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    if args.list_windows:
        return run_list_windows()

    if args.once:
        if not args.image:
            raise SystemExit("--image is required with --once")
        return run_once(args.image, args.debug_dir, getattr(args, "live_debug", False))

    if args.press_key:
        return run_press_key(args.key)

    return run_watch(
        args.window_title,
        args.window_id,
        args.interval,
        args.synthetic,
        args.portal,
        args.debug_dir,
        args.debug_every,
        getattr(args, "live_debug", False),
    )


if __name__ == "__main__":
    raise SystemExit(main())
