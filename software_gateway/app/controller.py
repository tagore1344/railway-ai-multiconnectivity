"""Deterministic link scoring and safe traffic-weight recommendations."""
from __future__ import annotations

from typing import Any


def _clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def quality_score(rtt_ms: float, loss_pct: float, rssi_dbm: float) -> float:
    """Return a transparent 0–100 quality score; not an ML prediction."""
    latency_quality = _clamp(1.0 - max(0.0, rtt_ms) / 400.0, 0.0, 1.0)
    loss_quality = _clamp(1.0 - max(0.0, loss_pct) / 10.0, 0.0, 1.0)
    signal_quality = _clamp((rssi_dbm + 115.0) / 55.0, 0.0, 1.0)
    return round(100.0 * (
        0.40 * latency_quality +
        0.35 * loss_quality +
        0.25 * signal_quality
    ), 2)


def recommend_weights(links: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Return a copy with integer recommendations summing to 100.

    Down links always receive zero. If every link is down, all recommendations
    are zero because no viable path exists. Recommendations are not applied to
    the host network.
    """
    result = [dict(link) for link in links]
    eligible = [
        i for i, link in enumerate(result)
        if str(link.get("state", "ACTIVE")).upper() != "DOWN"
    ]
    for link in result:
        link["recommended_weight_pct"] = 0

    if not eligible:
        return result

    scores = [max(0.01, float(result[i].get("quality_score", 0.01))) for i in eligible]
    total = sum(scores)
    rounded = [int(round(100.0 * score / total)) for score in scores]
    if rounded:
        rounded[-1] = max(0, 100 - sum(rounded[:-1]))
    for index, weight in zip(eligible, rounded):
        result[index]["recommended_weight_pct"] = weight
    return result
