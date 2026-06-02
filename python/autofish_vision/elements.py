from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import cv2
import numpy as np

# ROI ratios and HSV ranges are adapted from Chizukuo/NTE-auto-fish's MIT-licensed detector.
_NTE_BAR_ROI = {"left": 0.314844, "top": 0.05463, "width": 0.37526, "height": 0.02963}
_NTE_BUTTON_ROI = {
    "left": 3400 / 3840,
    "top": 1760 / 2160,
    "width": 440 / 3840,
    "height": 360 / 2160,
}
_NTE_SAFE_ZONE_LOWER = np.array([75, 190, 190], dtype=np.uint8)
_NTE_SAFE_ZONE_UPPER = np.array([100, 255, 255], dtype=np.uint8)
_NTE_CURSOR_LOWER = np.array([18, 115, 240], dtype=np.uint8)
_NTE_CURSOR_UPPER = np.array([40, 150, 255], dtype=np.uint8)
_NTE_BLUE_LOWER = np.array([100, 160, 140], dtype=np.uint8)
_NTE_BLUE_UPPER = np.array([130, 255, 255], dtype=np.uint8)
_NTE_KERNEL_3X3 = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))


@dataclass(frozen=True)
class ElementObservation:
    name: str
    confidence: float
    bbox: tuple[int, int, int, int]
    details: dict[str, Any]

    def to_json_dict(self) -> dict[str, Any]:
        x, y, width, height = self.bbox
        return {
            "name": self.name,
            "confidence": round(self.confidence, 4),
            "bbox": [x, y, width, height],
            "details": self.details,
        }


class WindowElementAnalyzer:
    def __init__(
        self,
        min_area: int = 24,
        max_results: int = 12,
        confirm_fish_hooked_frames: int = 1,
    ) -> None:
        self.min_area = min_area
        self.max_results = max_results
        self.confirm_fish_hooked_frames = confirm_fish_hooked_frames
        self._fish_hooked_streak = 0
        self._last_fish_hooked_bbox: tuple[int, int, int, int] | None = None
        self._last_reel_target_x: float | None = None
        self._last_reel_marker_x: float | None = None

    def analyze(self, frame: np.ndarray) -> list[ElementObservation]:
        observations: list[ElementObservation] = []
        reeling = self._find_nte_reeling_control_elements(frame) or self._find_reeling_control_elements(frame)
        if reeling:
            return reeling

        result_screen = self._find_fishing_result_screen(frame)

        prep_visible = self._find_start_fishing_button(frame) is not None
        raw_active = [] if prep_visible else self._find_active_fishing_elements(frame)
        active = self._confirm_active_fishing_elements(raw_active, frame.shape)
        observations.extend(active)
        if result_screen is not None:
            observations.append(result_screen)
        if not active and result_screen is None:
            observations.extend(self._find_fishing_prep_elements(frame))
        observations.extend(self._find_bright_regions(frame))
        observations.extend(self._find_red_regions(frame))

        semantic_names = {
            "fishing_panel",
            "start_fishing_button",
            "close_button",
            "fish_card",
            "rod_slot",
            "bait_slot",
            "water_roi",
            "fish_hooked_prompt",
            "hook_action_button",
            "active_fishing_close_button",
            "reel_green_target",
            "reel_yellow_marker",
            "hook_blue_trigger",
            "fishing_result_screen",
        }
        semantic_observations = [item for item in observations if item.name in semantic_names]
        raw_observations = [item for item in observations if item.name not in semantic_names]
        raw_observations.sort(key=lambda item: item.confidence, reverse=True)
        return semantic_observations + raw_observations[: self.max_results]

    def _confirm_active_fishing_elements(
        self,
        active: list[ElementObservation],
        frame_shape: tuple[int, ...],
    ) -> list[ElementObservation]:
        prompt = next((item for item in active if item.name == "fish_hooked_prompt"), None)
        if prompt is None:
            self._fish_hooked_streak = 0
            self._last_fish_hooked_bbox = None
            return [item for item in active if item.name == "hook_blue_trigger"]

        if self.confirm_fish_hooked_frames <= 1:
            return active

        if self._last_fish_hooked_bbox is not None and _bbox_close(
            prompt.bbox,
            self._last_fish_hooked_bbox,
            frame_shape,
        ):
            self._fish_hooked_streak += 1
        else:
            self._fish_hooked_streak = 1
        self._last_fish_hooked_bbox = prompt.bbox

        if self._fish_hooked_streak < self.confirm_fish_hooked_frames:
            return []

        return active

    def _find_active_fishing_elements(self, frame: np.ndarray) -> list[ElementObservation]:
        prompt = self._find_fish_hooked_prompt(frame)
        if prompt is None:
            blue_trigger = self._find_nte_hook_blue_trigger(frame)
            return [blue_trigger] if blue_trigger is not None else []

        frame_height, frame_width = frame.shape[:2]
        elements = [
            ElementObservation(
                "fish_hooked_prompt",
                prompt[4],
                prompt[:4],
                {
                    "state": "fish_on_hook",
                    "mode": "day_or_night",
                    "action": "press_hook_button",
                },
            ),
            ElementObservation(
                "hook_action_button",
                0.72,
                (
                    int(frame_width * 0.905),
                    int(frame_height * 0.860),
                    int(frame_width * 0.050),
                    int(frame_height * 0.090),
                ),
                {"key_hint": "F"},
            ),
            ElementObservation(
                "active_fishing_close_button",
                0.70,
                (
                    int(frame_width * 0.947),
                    int(frame_height * 0.055),
                    int(frame_width * 0.045),
                    int(frame_height * 0.075),
                ),
                {},
            ),
        ]
        blue_trigger = self._find_nte_hook_blue_trigger(frame)
        if blue_trigger is not None:
            elements.append(blue_trigger)
        return elements

    def _find_nte_hook_blue_trigger(self, frame: np.ndarray) -> ElementObservation | None:
        frame_height, frame_width = frame.shape[:2]
        x, y, width, height = _ratio_box(frame_width, frame_height, _NTE_BUTTON_ROI)
        roi = frame[y : y + height, x : x + width]
        if roi.size == 0:
            return None

        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, _NTE_BLUE_LOWER, _NTE_BLUE_UPPER)
        blue_pixels = int(cv2.countNonZero(mask))
        patch_width = max(12, int(round(frame_width * 0.012)))
        patch_height = max(12, int(round(frame_height * 0.018)))
        best_ratio, best_box = _best_mask_density_box(mask, patch_width, patch_height)
        min_ratio = 0.30
        if best_ratio < min_ratio:
            return None

        patch_x, patch_y, patch_w, patch_h = best_box
        confidence = min(1.0, best_ratio / min_ratio)
        return ElementObservation(
            "hook_blue_trigger",
            confidence,
            (x + patch_x, y + patch_y, patch_w, patch_h),
            {
                "state": "fish_on_hook",
                "action": "press_hook_button",
                "blue_pixels": blue_pixels,
                "best_ratio": best_ratio,
                "min_ratio": min_ratio,
                "scan_roi": [x, y, width, height],
                "source": "nte_hsv_button_roi",
            },
        )

    def _find_fishing_result_screen(self, frame: np.ndarray) -> ElementObservation | None:
        frame_height, frame_width = frame.shape[:2]
        x, y, width, height = _ratio_box(
            frame_width,
            frame_height,
            {"left": 0.40, "top": 0.30, "width": 0.22, "height": 0.34},
        )
        roi = frame[y : y + height, x : x + width]
        if roi.size == 0:
            return None

        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        saturation = hsv[:, :, 1]
        value = hsv[:, :, 2]
        vivid_ratio = float(np.mean((saturation > 80) & (value > 120)))
        bright_ratio = float(np.mean(value > 190))
        white_ratio = float(np.mean((saturation < 90) & (value > 165)))

        if vivid_ratio < 0.38 or bright_ratio < 0.15 or white_ratio < 0.10:
            return None

        bbox = _ratio_box(
            frame_width,
            frame_height,
            {"left": 0.30, "top": 0.08, "width": 0.40, "height": 0.86},
        )
        confidence = min(1.0, 0.55 + vivid_ratio * 0.30 + bright_ratio * 0.25 + white_ratio * 0.40)
        return ElementObservation(
            "fishing_result_screen",
            confidence,
            bbox,
            {
                "state": "result",
                "action": "press_escape_then_recast",
                "vivid_ratio": vivid_ratio,
                "bright_ratio": bright_ratio,
                "white_ratio": white_ratio,
            },
        )

    def _find_fish_hooked_prompt(self, frame: np.ndarray) -> tuple[int, int, int, int, float] | None:
        frame_height, frame_width = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        fixed_candidate = self._find_fish_hooked_fixed_banner(hsv, frame_width, frame_height)
        if fixed_candidate is not None:
            return fixed_candidate

        fixed_candidate = self._find_fish_hooked_expected_roi(hsv, frame_width, frame_height)
        if fixed_candidate is not None:
            return fixed_candidate

        dark_candidate = self._find_fish_hooked_dark_banner(hsv, frame_width, frame_height)
        if dark_candidate is not None:
            return dark_candidate

        text_mask = cv2.inRange(
            hsv,
            np.array([0, 0, 155], dtype=np.uint8),
            np.array([180, 125, 255], dtype=np.uint8),
        )
        roi = np.zeros(text_mask.shape, dtype=np.uint8)
        roi[
            int(frame_height * 0.12) : int(frame_height * 0.42),
            int(frame_width * 0.18) : int(frame_width * 0.84),
        ] = 255
        text_mask = cv2.bitwise_and(text_mask, roi)
        text_mask = cv2.morphologyEx(text_mask, cv2.MORPH_CLOSE, np.ones((45, 7), dtype=np.uint8))
        text_mask = cv2.dilate(text_mask, np.ones((17, 5), dtype=np.uint8), iterations=1)

        contours, _ = cv2.findContours(text_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[tuple[float, tuple[int, int, int, int, float]]] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < frame_width * frame_height * 0.0008:
                continue
            if width < frame_width * 0.25 or width > frame_width * 0.84:
                continue
            if height < frame_height * 0.018 or height > frame_height * 0.090:
                continue

            aspect = width / max(height, 1)
            if aspect < 8.0:
                continue

            prompt_x = _clamp(int(round(x - width * 0.14)), 0, frame_width - 1)
            prompt_y = _clamp(int(round(y - height * 1.05)), 0, frame_height - 1)
            prompt_width = min(frame_width - prompt_x, int(round(width * 1.18)))
            prompt_height = min(frame_height - prompt_y, int(round(height * 2.80)))
            if prompt_width <= 0 or prompt_height <= 0:
                continue

            prompt_region = hsv[prompt_y : prompt_y + prompt_height, prompt_x : prompt_x + prompt_width]
            if prompt_region.size == 0:
                continue
            dark_ratio = float(np.mean((prompt_region[:, :, 1] < 135) & (prompt_region[:, :, 2] < 170)))
            if dark_ratio < 0.08:
                prompt_y = _clamp(int(round(y - height * 0.90)), 0, frame_height - 1)
                prompt_height = min(frame_height - prompt_y, int(round(height * 2.70)))
                prompt_region = hsv[prompt_y : prompt_y + prompt_height, prompt_x : prompt_x + prompt_width]
                dark_ratio = float(np.mean((prompt_region[:, :, 1] < 135) & (prompt_region[:, :, 2] < 170))) if prompt_region.size else 0.0
                if dark_ratio < 0.05:
                    continue

            center_distance = abs((x + width / 2.0) - frame_width / 2.0) / frame_width
            score = min(1.0, 0.62 + (width / frame_width) * 0.35 + dark_ratio * 0.40 - center_distance * 0.25)
            candidates.append((score, (prompt_x, prompt_y, prompt_width, prompt_height, score)))

        if not candidates:
            return None

        candidates.sort(reverse=True, key=lambda item: item[0])
        return candidates[0][1]

    def _find_fish_hooked_fixed_banner(
        self,
        hsv: np.ndarray,
        frame_width: int,
        frame_height: int,
    ) -> tuple[int, int, int, int, float] | None:
        x = int(frame_width * 0.270)
        y = int(frame_height * 0.218)
        width = int(frame_width * 0.460)
        height = int(frame_height * 0.082)
        roi = hsv[y : y + height, x : x + width]
        if roi.size == 0:
            return None

        value = roi[:, :, 2]
        saturation = roi[:, :, 1]
        dark = (value < 150) & (saturation < 205)
        text = (value > 145) & (saturation < 190)
        very_dark = value < 80

        dark_ratio = float(np.mean(dark))
        text_ratio = float(np.mean(text))
        very_dark_ratio = float(np.mean(very_dark))
        edge_ratio = _edge_ratio(value)

        if dark_ratio < 0.22 or text_ratio < 0.180 or very_dark_ratio < 0.100 or edge_ratio < 0.035:
            return None

        dark_rows = np.mean(dark, axis=1)
        text_rows = np.mean(text, axis=1)
        very_dark_rows = np.mean(very_dark, axis=1)
        if (
            float(np.max(dark_rows)) < 0.42
            or float(np.max(text_rows)) < 0.070
            or float(np.max(very_dark_rows)) < 0.200
        ):
            return None

        score = min(1.0, 0.72 + dark_ratio * 0.35 + text_ratio * 2.2 + very_dark_ratio * 0.35)
        return (x, y, width, height, score)

    def _find_fish_hooked_expected_roi(
        self,
        hsv: np.ndarray,
        frame_width: int,
        frame_height: int,
    ) -> tuple[int, int, int, int, float] | None:
        roi_x = int(frame_width * 0.245)
        roi_y = int(frame_height * 0.205)
        roi_width = int(frame_width * 0.530)
        roi_height = int(frame_height * 0.115)
        roi = hsv[roi_y : roi_y + roi_height, roi_x : roi_x + roi_width]
        if roi.size == 0:
            return None

        text_mask = cv2.inRange(
            roi,
            np.array([0, 0, 125], dtype=np.uint8),
            np.array([180, 170, 255], dtype=np.uint8),
        )
        text_mask = cv2.morphologyEx(text_mask, cv2.MORPH_CLOSE, np.ones((13, 3), dtype=np.uint8))
        text_mask = cv2.morphologyEx(text_mask, cv2.MORPH_OPEN, np.ones((3, 2), dtype=np.uint8))

        dark_mask = cv2.inRange(
            roi,
            np.array([0, 0, 0], dtype=np.uint8),
            np.array([180, 180, 185], dtype=np.uint8),
        )

        contours, _ = cv2.findContours(text_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        components: list[tuple[int, int, int, int, float]] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < 45:
                continue
            if height < roi_height * 0.11 or height > roi_height * 0.50:
                continue
            if y > roi_height * 0.74:
                continue
            components.append((x, y, width, height, area))

        candidates: list[tuple[float, tuple[int, int, int, int, float]]] = []
        for anchor in components:
            _, anchor_y, _, anchor_height, _ = anchor
            line_y = anchor_y + anchor_height / 2.0
            line_components = [
                item
                for item in components
                if abs((item[1] + item[3] / 2.0) - line_y) < roi_height * 0.18
            ]
            if len(line_components) < 3:
                continue

            x0 = min(item[0] for item in line_components)
            y0 = min(item[1] for item in line_components)
            x1 = max(item[0] + item[2] for item in line_components)
            y1 = max(item[1] + item[3] for item in line_components)
            text_width = x1 - x0
            text_height = y1 - y0
            text_area = sum(item[4] for item in line_components)
            if text_width < roi_width * 0.42 or text_width > roi_width * 0.88:
                continue
            if text_height < roi_height * 0.13 or text_height > roi_height * 0.55:
                continue
            if text_width / max(text_height, 1) < 7.0:
                continue
            if text_area < roi_width * roi_height * 0.015:
                continue

            pad_x = int(text_width * 0.16)
            pad_y = int(text_height * 1.55)
            prompt_x = _clamp(roi_x + x0 - pad_x, 0, frame_width - 1)
            prompt_y = _clamp(roi_y + y0 - pad_y, 0, frame_height - 1)
            prompt_width = min(frame_width - prompt_x, text_width + pad_x * 2)
            prompt_height = min(frame_height - prompt_y, text_height + pad_y * 2)
            if prompt_width > frame_width * 0.64 or prompt_height > frame_height * 0.18:
                continue
            if prompt_x > frame_width * 0.45 or prompt_x + prompt_width < frame_width * 0.55:
                continue
            prompt_region = hsv[prompt_y : prompt_y + prompt_height, prompt_x : prompt_x + prompt_width]
            if prompt_region.size == 0:
                continue
            prompt_text_ratio = float(np.mean((prompt_region[:, :, 2] > 145) & (prompt_region[:, :, 1] < 190)))
            if prompt_text_ratio < 0.12:
                continue

            local_x0 = max(0, x0 - pad_x)
            local_y0 = max(0, y0 - pad_y)
            local_x1 = min(roi_width, x1 + pad_x)
            local_y1 = min(roi_height, y1 + pad_y)
            dark_region = dark_mask[local_y0:local_y1, local_x0:local_x1]
            dark_ratio = float(np.mean(dark_region > 0)) if dark_region.size else 0.0
            if dark_ratio < 0.070:
                continue

            value_region = roi[local_y0:local_y1, local_x0:local_x1, 2]
            if _edge_ratio(value_region) < 0.035:
                continue

            center_distance = abs((prompt_x + prompt_width / 2.0) - frame_width / 2.0) / frame_width
            score = min(1.0, 0.80 + (text_width / roi_width) * 0.24 + dark_ratio * 0.35 - center_distance * 0.25)
            candidates.append((score, (prompt_x, prompt_y, prompt_width, prompt_height, score)))

        if not candidates:
            return None

        candidates.sort(reverse=True, key=lambda item: item[0])
        return candidates[0][1]

    def _find_fish_hooked_dark_banner(
        self,
        hsv: np.ndarray,
        frame_width: int,
        frame_height: int,
    ) -> tuple[int, int, int, int, float] | None:
        roi = np.zeros(hsv.shape[:2], dtype=np.uint8)
        roi[
            int(frame_height * 0.16) : int(frame_height * 0.36),
            int(frame_width * 0.20) : int(frame_width * 0.80),
        ] = 255

        dark_mask = cv2.inRange(
            hsv,
            np.array([0, 0, 0], dtype=np.uint8),
            np.array([180, 145, 175], dtype=np.uint8),
        )
        dark_mask = cv2.bitwise_and(dark_mask, roi)
        dark_mask = cv2.morphologyEx(dark_mask, cv2.MORPH_CLOSE, np.ones((55, 13), dtype=np.uint8))
        dark_mask = cv2.dilate(dark_mask, np.ones((25, 9), dtype=np.uint8), iterations=1)

        white_mask = cv2.inRange(
            hsv,
            np.array([0, 0, 165], dtype=np.uint8),
            np.array([180, 95, 255], dtype=np.uint8),
        )

        contours, _ = cv2.findContours(dark_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[tuple[float, tuple[int, int, int, int, float]]] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < frame_width * frame_height * 0.002:
                continue
            if width < frame_width * 0.28 or width > frame_width * 0.78:
                continue
            if height < frame_height * 0.035 or height > frame_height * 0.105:
                continue

            aspect = width / max(height, 1)
            if aspect < 5.5:
                continue

            prompt_x = _clamp(int(round(x - width * 0.06)), 0, frame_width - 1)
            prompt_y = _clamp(int(round(y - height * 0.25)), 0, frame_height - 1)
            prompt_width = min(frame_width - prompt_x, int(round(width * 1.12)))
            prompt_height = min(frame_height - prompt_y, int(round(height * 1.50)))
            region_white = white_mask[prompt_y : prompt_y + prompt_height, prompt_x : prompt_x + prompt_width]
            white_ratio = float(np.mean(region_white > 0)) if region_white.size else 0.0
            if white_ratio < 0.015:
                continue

            center_distance = abs((x + width / 2.0) - frame_width / 2.0) / frame_width
            score = min(1.0, 0.78 + (width / frame_width) * 0.30 + white_ratio * 3.0 - center_distance * 0.45)
            candidates.append((score, (prompt_x, prompt_y, prompt_width, prompt_height, score)))

        if not candidates:
            return None

        candidates.sort(reverse=True, key=lambda item: item[0])
        return candidates[0][1]

    def _looks_like_active_fishing_hud(self, frame: np.ndarray) -> bool:
        frame_height, frame_width = frame.shape[:2]
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        roi = gray[
            int(frame_height * 0.78) : int(frame_height * 0.99),
            int(frame_width * 0.68) : int(frame_width * 0.99),
        ]
        if roi.size == 0:
            return False

        circles = cv2.HoughCircles(
            roi,
            cv2.HOUGH_GRADIENT,
            dp=1.2,
            minDist=max(24, int(frame_width * 0.035)),
            param1=80,
            param2=24,
            minRadius=max(18, int(frame_width * 0.014)),
            maxRadius=max(44, int(frame_width * 0.040)),
        )
        return circles is not None

    def _find_nte_reeling_control_elements(self, frame: np.ndarray) -> list[ElementObservation]:
        frame_height, frame_width = frame.shape[:2]
        roi_x, roi_y, roi_width, roi_height = _ratio_box(frame_width, frame_height, _NTE_BAR_ROI)
        roi = frame[roi_y : roi_y + roi_height, roi_x : roi_x + roi_width]
        if roi.size == 0:
            self._last_reel_target_x = None
            self._last_reel_marker_x = None
            return []

        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        scale = min(frame_width / 3840.0, frame_height / 2160.0)
        min_area = max(18.0, 30.0 * scale * scale)
        ignore_margin = 0.02

        target = _hsv_centroid_box(
            hsv,
            _NTE_SAFE_ZONE_LOWER,
            _NTE_SAFE_ZONE_UPPER,
            min_area,
            ignore_margin_ratio=0.0,
            last_known_x=self._last_reel_target_x,
        )
        marker = _hsv_centroid_box(
            hsv,
            _NTE_CURSOR_LOWER,
            _NTE_CURSOR_UPPER,
            min_area,
            ignore_margin_ratio=ignore_margin,
            last_known_x=self._last_reel_marker_x,
        )
        if target is None or marker is None:
            return []

        target_x, target_area, target_box = target
        marker_x, marker_area, marker_box = marker
        self._last_reel_target_x = target_x
        self._last_reel_marker_x = marker_x

        tx, ty, tw, th = target_box
        mx, my, mw, mh = marker_box
        target_bbox = (roi_x + tx, roi_y + ty, tw, th)
        marker_bbox = (roi_x + mx, roi_y + my, mw, mh)
        target_center = roi_x + target_x
        marker_center = roi_x + marker_x
        error = target_center - marker_center

        return [
            ElementObservation(
                "reel_green_target",
                0.94,
                target_bbox,
                {
                    "center_x": target_center,
                    "area": target_area,
                    "frame_width": frame_width,
                    "source": "nte_hsv_centroid",
                },
            ),
            ElementObservation(
                "reel_yellow_marker",
                0.94,
                marker_bbox,
                {
                    "center_x": marker_center,
                    "area": marker_area,
                    "frame_width": frame_width,
                    "target_error": error,
                    "suggested_key": "D" if error > 0 else "A",
                    "source": "nte_hsv_centroid",
                },
            ),
        ]

    def _find_reeling_control_elements(self, frame: np.ndarray) -> list[ElementObservation]:
        frame_height, frame_width = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        roi_y0 = int(frame_height * 0.035)
        roi_y1 = int(frame_height * 0.165)
        roi_x0 = int(frame_width * 0.100)
        roi_x1 = int(frame_width * 0.900)
        roi = hsv[roi_y0:roi_y1, roi_x0:roi_x1]
        if roi.size == 0:
            return []

        green_mask = cv2.inRange(
            roi,
            np.array([36, 70, 120], dtype=np.uint8),
            np.array([95, 255, 255], dtype=np.uint8),
        )
        green_mask = cv2.morphologyEx(green_mask, cv2.MORPH_OPEN, np.ones((3, 3), dtype=np.uint8))
        green_mask = cv2.morphologyEx(green_mask, cv2.MORPH_CLOSE, np.ones((17, 5), dtype=np.uint8))

        yellow_mask = cv2.inRange(
            roi,
            np.array([18, 90, 150], dtype=np.uint8),
            np.array([38, 255, 255], dtype=np.uint8),
        )
        yellow_mask = cv2.morphologyEx(yellow_mask, cv2.MORPH_OPEN, np.ones((3, 3), dtype=np.uint8))
        yellow_mask = cv2.morphologyEx(yellow_mask, cv2.MORPH_CLOSE, np.ones((5, 9), dtype=np.uint8))

        green = self._largest_reel_green_box(green_mask, frame_width, frame_height, roi_x0, roi_y0)
        marker = self._largest_reel_marker_box(yellow_mask, frame_width, frame_height, roi_x0, roi_y0)
        if green is None or marker is None:
            return []

        gx, gy, gw, gh = green
        mx, my, mw, mh = marker
        if abs((gy + gh / 2.0) - (my + mh / 2.0)) > frame_height * 0.020:
            return []
        green_center = gx + gw / 2.0
        marker_center = mx + mw / 2.0
        error = green_center - marker_center
        return [
            ElementObservation(
                "reel_green_target",
                0.92,
                green,
                {
                    "center_x": green_center,
                    "frame_width": frame_width,
                },
            ),
            ElementObservation(
                "reel_yellow_marker",
                0.92,
                marker,
                {
                    "center_x": marker_center,
                    "frame_width": frame_width,
                    "target_error": error,
                    "suggested_key": "D" if error > 0 else "A",
                },
            ),
        ]

    def _largest_reel_green_box(
        self,
        mask: np.ndarray,
        frame_width: int,
        frame_height: int,
        roi_x0: int,
        roi_y0: int,
    ) -> tuple[int, int, int, int] | None:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[tuple[float, tuple[int, int, int, int]]] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < frame_width * frame_height * 0.00004:
                continue
            if width < frame_width * 0.035 or width > frame_width * 0.220:
                continue
            if height < frame_height * 0.006 or height > frame_height * 0.035:
                continue
            if width / max(height, 1) < 4.0:
                continue
            candidates.append((area, (roi_x0 + x, roi_y0 + y, width, height)))

        if not candidates:
            return None
        merged_candidates = _merge_same_row_boxes([box for _, box in candidates], frame_width, frame_height)
        if merged_candidates:
            return max(merged_candidates, key=lambda box: box[2] * box[3])
        candidates.sort(reverse=True, key=lambda item: item[0])
        return candidates[0][1]

    def _largest_reel_marker_box(
        self,
        mask: np.ndarray,
        frame_width: int,
        frame_height: int,
        roi_x0: int,
        roi_y0: int,
    ) -> tuple[int, int, int, int] | None:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[tuple[float, tuple[int, int, int, int]]] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < frame_width * frame_height * 0.000006:
                continue
            if width < frame_width * 0.002 or width > frame_width * 0.018:
                continue
            if height < frame_height * 0.006 or height > frame_height * 0.055:
                continue
            if height / max(width, 1) < 1.4:
                continue
            candidates.append((area, (roi_x0 + x, roi_y0 + y, width, height)))

        if not candidates:
            return None
        candidates.sort(reverse=True, key=lambda item: item[0])
        return candidates[0][1]

    def _find_fishing_prep_elements(self, frame: np.ndarray) -> list[ElementObservation]:
        start_button = self._find_start_fishing_button(frame)
        if start_button is None:
            return []

        frame_height, frame_width = frame.shape[:2]
        sx, sy, sw, sh = start_button
        panel_width = int(round(sw / 0.70))
        panel_height = int(round(panel_width * 1.43))
        panel_x = _clamp(int(round(sx - panel_width * 0.237)), 0, frame_width - 1)
        panel_y = _clamp(int(round(sy - panel_height * 0.885)), 0, frame_height - 1)
        panel_width = min(panel_width, frame_width - panel_x)
        panel_height = min(panel_height, frame_height - panel_y)

        panel = (panel_x, panel_y, panel_width, panel_height)
        elements = [
            ElementObservation(
                "fishing_panel",
                0.95,
                panel,
                {"source": "start_button_anchor"},
            ),
            ElementObservation(
                "start_fishing_button",
                0.98,
                start_button,
                {"state": "visible"},
            ),
            ElementObservation(
                "close_button",
                0.9,
                _relative_box(panel, 0.862, 0.030, 0.090, 0.065),
                {},
            ),
            ElementObservation(
                "rod_slot",
                0.88,
                _relative_box(panel, 0.180, 0.680, 0.195, 0.130),
                {},
            ),
            ElementObservation(
                "bait_slot",
                0.88,
                _relative_box(panel, 0.630, 0.680, 0.195, 0.130),
                {},
            ),
        ]

        for index, box in enumerate(self._fish_card_boxes(panel), start=1):
            elements.append(
                ElementObservation(
                    "fish_card",
                    0.86,
                    box,
                    {"index": index},
                )
            )

        water_right = max(0, panel_x - 8)
        elements.append(
            ElementObservation(
                "water_roi",
                0.65,
                (
                    int(frame_width * 0.02),
                    int(frame_height * 0.28),
                    max(1, water_right - int(frame_width * 0.02)),
                    int(frame_height * 0.25),
                ),
                {"purpose": "bobber_and_bite_detection"},
            )
        )

        return elements

    def _find_start_fishing_button(self, frame: np.ndarray) -> tuple[int, int, int, int] | None:
        frame_height, frame_width = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(
            hsv,
            np.array([0, 0, 165], dtype=np.uint8),
            np.array([180, 55, 255], dtype=np.uint8),
        )
        roi = np.zeros(mask.shape, dtype=np.uint8)
        roi[int(frame_height * 0.70) :, int(frame_width * 0.55) :] = 255
        mask = cv2.bitwise_and(mask, roi)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, np.ones((19, 19), dtype=np.uint8))

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[tuple[float, tuple[int, int, int, int]]] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < 5000:
                continue
            aspect = width / max(height, 1)
            if aspect < 4.0 or aspect > 12.0:
                continue
            candidates.append((area, (x, y, width, height)))

        if not candidates:
            return None

        candidates.sort(reverse=True, key=lambda item: item[0])
        return candidates[0][1]

    def _fish_card_boxes(self, panel: tuple[int, int, int, int]) -> list[tuple[int, int, int, int]]:
        x_ratio = 0.070
        y_ratio = 0.405
        width_ratio = 0.165
        height_ratio = 0.120
        gap_ratio = 0.017
        boxes = []
        for index in range(5):
            boxes.append(
                _relative_box(
                    panel,
                    x_ratio + index * (width_ratio + gap_ratio),
                    y_ratio,
                    width_ratio,
                    height_ratio,
                )
            )
        return boxes

    def _find_bright_regions(self, frame: np.ndarray) -> list[ElementObservation]:
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        value = hsv[:, :, 2]
        _, mask = cv2.threshold(value, 225, 255, cv2.THRESH_BINARY)
        mask = _clean_mask(mask)
        return self._mask_to_observations("bright_region", mask, frame.shape)

    def _find_red_regions(self, frame: np.ndarray) -> list[ElementObservation]:
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        lower_red_a = np.array([0, 90, 80], dtype=np.uint8)
        upper_red_a = np.array([12, 255, 255], dtype=np.uint8)
        lower_red_b = np.array([168, 90, 80], dtype=np.uint8)
        upper_red_b = np.array([180, 255, 255], dtype=np.uint8)
        mask = cv2.bitwise_or(
            cv2.inRange(hsv, lower_red_a, upper_red_a),
            cv2.inRange(hsv, lower_red_b, upper_red_b),
        )
        mask = _clean_mask(mask)
        return self._mask_to_observations("red_region", mask, frame.shape)

    def _mask_to_observations(
        self,
        name: str,
        mask: np.ndarray,
        frame_shape: tuple[int, ...],
    ) -> list[ElementObservation]:
        frame_area = float(frame_shape[0] * frame_shape[1])
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        observations: list[ElementObservation] = []
        for contour in contours:
            area = cv2.contourArea(contour)
            if area < self.min_area:
                continue

            x, y, width, height = cv2.boundingRect(contour)
            confidence = min((area / frame_area) * 50.0, 1.0)
            observations.append(
                ElementObservation(
                    name=name,
                    confidence=confidence,
                    bbox=(x, y, width, height),
                    details={"area": area},
                )
            )

        return observations


def _clean_mask(mask: np.ndarray) -> np.ndarray:
    kernel = np.ones((3, 3), dtype=np.uint8)
    opened = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
    return cv2.dilate(opened, kernel, iterations=1)


def _relative_box(
    container: tuple[int, int, int, int],
    x_ratio: float,
    y_ratio: float,
    width_ratio: float,
    height_ratio: float,
) -> tuple[int, int, int, int]:
    x, y, width, height = container
    return (
        int(round(x + width * x_ratio)),
        int(round(y + height * y_ratio)),
        max(1, int(round(width * width_ratio))),
        max(1, int(round(height * height_ratio))),
    )


def _ratio_box(
    frame_width: int,
    frame_height: int,
    ratios: dict[str, float],
) -> tuple[int, int, int, int]:
    x = _clamp(int(round(frame_width * ratios["left"])), 0, frame_width - 1)
    y = _clamp(int(round(frame_height * ratios["top"])), 0, frame_height - 1)
    width = max(1, min(frame_width - x, int(round(frame_width * ratios["width"]))))
    height = max(1, min(frame_height - y, int(round(frame_height * ratios["height"]))))
    return x, y, width, height


def _hsv_centroid_box(
    hsv: np.ndarray,
    lower: np.ndarray,
    upper: np.ndarray,
    min_area: float,
    ignore_margin_ratio: float = 0.0,
    last_known_x: float | None = None,
) -> tuple[float, float, tuple[int, int, int, int]] | None:
    mask = cv2.inRange(hsv, lower, upper)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, _NTE_KERNEL_3X3)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None

    roi_width = hsv.shape[1]
    valid: list[tuple[int, int, int, int, float]] = []
    for contour in contours:
        x, y, width, height = cv2.boundingRect(contour)
        area = float(width * height)
        if area < min_area:
            continue
        if ignore_margin_ratio > 0.0:
            left_limit = roi_width * ignore_margin_ratio
            right_limit = roi_width * (1.0 - ignore_margin_ratio)
            if x < left_limit or x + width > right_limit:
                continue
        valid.append((x, y, width, height, area))

    if not valid:
        return None
    if last_known_x is None:
        last_known_x = roi_width / 2.0

    valid.sort(key=lambda box: abs((box[0] + box[2] / 2.0) - last_known_x))
    group = [valid[0]]
    group_min_x = valid[0][0]
    group_max_x = valid[0][0] + valid[0][2]
    gap_threshold = roi_width * 0.05

    for box in valid[1:]:
        x, _, width, _, _ = box
        gap = max(0, max(group_min_x - (x + width), x - group_max_x))
        if gap <= gap_threshold:
            group.append(box)
            group_min_x = min(group_min_x, x)
            group_max_x = max(group_max_x, x + width)

    x0 = min(item[0] for item in group)
    y0 = min(item[1] for item in group)
    x1 = max(item[0] + item[2] for item in group)
    y1 = max(item[1] + item[3] for item in group)
    total_area = sum(item[4] for item in group)
    center_x = (x0 + x1) / 2.0
    return center_x, total_area, (x0, y0, x1 - x0, y1 - y0)


def _best_mask_density_box(mask: np.ndarray, width: int, height: int) -> tuple[float, tuple[int, int, int, int]]:
    if mask.size == 0:
        return 0.0, (0, 0, 1, 1)

    height = max(1, min(height, mask.shape[0]))
    width = max(1, min(width, mask.shape[1]))
    binary = (mask > 0).astype(np.float32)
    density = cv2.boxFilter(
        binary,
        ddepth=-1,
        ksize=(width, height),
        normalize=True,
        borderType=cv2.BORDER_CONSTANT,
    )
    _, max_value, _, max_location = cv2.minMaxLoc(density)
    x = _clamp(int(round(max_location[0] - width / 2.0)), 0, max(0, mask.shape[1] - width))
    y = _clamp(int(round(max_location[1] - height / 2.0)), 0, max(0, mask.shape[0] - height))
    return float(max_value), (x, y, width, height)


def _clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, value))


def _edge_ratio(gray: np.ndarray) -> float:
    if gray.size == 0:
        return 0.0
    edges = cv2.Canny(gray, 80, 160)
    return float(np.mean(edges > 0))


def _merge_same_row_boxes(
    boxes: list[tuple[int, int, int, int]],
    frame_width: int,
    frame_height: int,
) -> list[tuple[int, int, int, int]]:
    groups: list[list[tuple[int, int, int, int]]] = []
    for box in sorted(boxes, key=lambda item: (item[1], item[0])):
        x, y, width, height = box
        center_y = y + height / 2.0
        placed = False
        for group in groups:
            gx0 = min(item[0] for item in group)
            gx1 = max(item[0] + item[2] for item in group)
            group_center_y = np.mean([item[1] + item[3] / 2.0 for item in group])
            gap = max(0, max(gx0, x) - min(gx1, x + width))
            if abs(center_y - group_center_y) <= frame_height * 0.010 and gap <= frame_width * 0.020:
                group.append(box)
                placed = True
                break
        if not placed:
            groups.append([box])

    merged: list[tuple[int, int, int, int]] = []
    for group in groups:
        if len(group) == 1:
            merged.append(group[0])
            continue
        x0 = min(item[0] for item in group)
        y0 = min(item[1] for item in group)
        x1 = max(item[0] + item[2] for item in group)
        y1 = max(item[1] + item[3] for item in group)
        merged.append((x0, y0, x1 - x0, y1 - y0))
    return merged


def _bbox_close(
    a: tuple[int, int, int, int],
    b: tuple[int, int, int, int],
    frame_shape: tuple[int, ...],
) -> bool:
    frame_height, frame_width = frame_shape[:2]
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    center_delta = abs((ax + aw / 2.0) - (bx + bw / 2.0)) + abs((ay + ah / 2.0) - (by + bh / 2.0))
    size_delta = abs(aw - bw) + abs(ah - bh)
    return center_delta < (frame_width + frame_height) * 0.025 and size_delta < (frame_width + frame_height) * 0.035
