from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import cv2
import numpy as np


@dataclass(frozen=True)
class Detection:
    event: str
    confidence: float
    details: dict[str, Any]

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "event": self.event,
            "confidence": round(self.confidence, 4),
            "details": self.details,
        }


class BiteDetector:
    """Small starter detector that flags bright bobber-like motion regions."""

    def __init__(self, min_area: int = 32) -> None:
        self.min_area = min_area
        self._previous_gray: np.ndarray | None = None

    def analyze(self, frame: np.ndarray) -> Detection:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        if self._previous_gray is None:
            self._previous_gray = gray
            return Detection("calibrating", 0.0, {"reason": "first_frame"})

        diff = cv2.absdiff(gray, self._previous_gray)
        self._previous_gray = gray

        _, mask = cv2.threshold(diff, 35, 255, cv2.THRESH_BINARY)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        areas = [cv2.contourArea(contour) for contour in contours]
        max_area = max(areas, default=0.0)

        confidence = min(max_area / 700.0, 1.0)
        if max_area >= self.min_area:
            return Detection("motion", confidence, {"max_area": max_area})

        return Detection("idle", confidence, {"max_area": max_area})

