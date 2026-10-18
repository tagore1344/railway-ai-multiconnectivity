"""Synthetic WAN telemetry for software-only development and test."""
from __future__ import annotations

from collections import deque
from datetime import datetime, timezone
import math
import random
from typing import Any

from .controller import quality_score, recommend_weights


class RailwaySimulator:
    def __init__(self, seed: int = 2026, history_limit: int = 180) -> None:
        self.rng = random.Random(seed)
        self.tick_count = 0
        self.links: list[dict[str, Any]] = [
            {"id": "wan1", "operator": "Carrier A", "base_rtt": 42.0, "rtt_ms": 42.0, "loss_pct": 0.4, "rssi_dbm": -78.0, "state": "ACTIVE", "quality_score": 0.0, "recommended_weight_pct": 0, "faulted": False},
            {"id": "wan2", "operator": "Carrier B", "base_rtt": 65.0, "rtt_ms": 65.0, "loss_pct": 0.8, "rssi_dbm": -84.0, "state": "ACTIVE", "quality_score": 0.0, "recommended_weight_pct": 0, "faulted": False},
            {"id": "wan3", "operator": "Carrier C", "base_rtt": 88.0, "rtt_ms": 88.0, "loss_pct": 1.2, "rssi_dbm": -90.0, "state": "ACTIVE", "quality_score": 0.0, "recommended_weight_pct": 0, "faulted": False},
        ]
        self.history: deque[dict[str, Any]] = deque(maxlen=history_limit)
        self.events: deque[dict[str, Any]] = deque(maxlen=100)
        self._event("info", "Simulation started", "Synthetic telemetry only; no network interfaces are used.")
        self._recalculate()

    @staticmethod
    def _now() -> str:
        return datetime.now(timezone.utc).isoformat()

    def _event(self, level: str, title: str, detail: str) -> None:
        self.events.appendleft({
            "timestamp": self._now(), "level": level,
            "title": title, "detail": detail,
        })

    def _recalculate(self) -> None:
        for link in self.links:
            link["quality_score"] = quality_score(
                link["rtt_ms"], link["loss_pct"], link["rssi_dbm"]
            )
        self.links = recommend_weights(self.links)

    def step(self) -> dict[str, Any]:
        self.tick_count += 1
        now = self._now()
        for index, link in enumerate(self.links):
            old_state = link["state"]
            if link["faulted"]:
                link.update({"rtt_ms": 999.0, "loss_pct": 100.0, "state": "DOWN"})
            else:
                wave = math.sin((self.tick_count / 8.0) + index * 1.7)
                link["rtt_ms"] = round(max(
                    18.0, min(320.0, link["base_rtt"] + 20.0 * wave + self.rng.uniform(-5, 5))
                ), 2)
                link["loss_pct"] = round(max(
                    0.0, min(8.0, 0.9 + 0.8 * abs(wave) + self.rng.uniform(-0.3, 0.4))
                ), 2)
                link["rssi_dbm"] = round(max(
                    -112.0, min(-55.0, link["rssi_dbm"] + self.rng.uniform(-1.1, 1.1))
                ), 1)
                link["state"] = (
                    "DEGRADED" if link["rtt_ms"] > 180 or link["loss_pct"] > 5
                    else "ACTIVE"
                )
            link["quality_score"] = quality_score(
                link["rtt_ms"], link["loss_pct"], link["rssi_dbm"]
            )
            if old_state != link["state"]:
                level = "warning" if link["state"] == "DOWN" else "info"
                self._event(
                    level,
                    f'{link["id"]}: {old_state} → {link["state"]}',
                    "Simulated state transition.",
                )
            self.history.append({
                "timestamp": now, "link_id": link["id"],
                "rtt_ms": link["rtt_ms"], "loss_pct": link["loss_pct"],
                "quality_score": link["quality_score"],
                "source": "SIMULATED_NOT_MEASURED",
            })
        self.links = recommend_weights(self.links)
        return self.snapshot()

    def set_fault(self, link_id: str, faulted: bool) -> dict[str, Any]:
        link = next((item for item in self.links if item["id"] == link_id), None)
        if link is None:
            raise KeyError(link_id)
        was_faulted = link["faulted"]
        link["faulted"] = bool(faulted)
        if was_faulted != link["faulted"]:
            message = "Simulated WAN outage injected" if faulted else "Simulated WAN outage cleared"
            self._event(
                "warning" if faulted else "info",
                f"{link_id}: {message}",
                "No real interface or route was changed.",
            )
        self.step()
        return self.snapshot()

    def snapshot(self) -> dict[str, Any]:
        active = sum(link["state"] != "DOWN" for link in self.links)
        return {
            "timestamp": self._now(),
            "mode": "SIMULATION",
            "evidence_label": "SIMULATED_NOT_MEASURED",
            "tick": self.tick_count,
            "active_links": active,
            "total_links": len(self.links),
            "links": [dict(link) for link in self.links],
            "history": list(self.history),
            "events": list(self.events),
        }
