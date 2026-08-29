from __future__ import annotations

import json
from pathlib import Path

import pandas as pd

DATASET = Path("railway-v8.2-dataset.csv")
RESULTS = Path("v10-bonding-results.json")

CAPACITY = {
    1: 30.0,
    2: 20.0,
    3: 15.0,
}
QUALITY_COLUMNS = {
    1: "n1_quality",
    2: "n2_quality",
    3: "n3_quality",
}
LOSS_COLUMNS = {
    1: "n1_loss",
    2: "n2_loss",
    3: "n3_loss",
}

PASSENGER_DEMAND_MBPS = 12.0


def effective_link_capacity(row: pd.Series, link: int) -> float:
    quality = max(0.0, min(1.0, float(row[QUALITY_COLUMNS[link]])))
    loss = max(0.0, min(100.0, float(row[LOSS_COLUMNS[link]]))) / 100.0
    return CAPACITY[link] * quality * (1.0 - loss)


def main() -> None:
    if not DATASET.exists():
        raise FileNotFoundError(f"Missing dataset: {DATASET}")

    df = pd.read_csv(DATASET)

    # For this V10 experiment we compare three policies:
    # 1) Best-link: all demand uses the strongest available link.
    # 2) Capacity-aware multi-link: demand is distributed across links.
    # 3) Bonded-capacity model: all reachable link capacity is pooled.
    #
    # This is an offline capacity/utilization emulation. It is NOT yet a
    # packet-level MPTCP/MPQUIC/SD-WAN bonding implementation.

    best_values = []
    multipath_values = []
    bonded_values = []
    available_link_counts = []

    for _, row in df.iterrows():
        capacities = {
            link: effective_link_capacity(row, link)
            for link in (1, 2, 3)
        }

        available = {
            link: value
            for link, value in capacities.items()
            if value > 0.05
        }

        available_link_counts.append(len(available))

        if not available:
            best = 0.0
            multipath = 0.0
            bonded = 0.0
        else:
            best = min(PASSENGER_DEMAND_MBPS, max(available.values()))
            total_available = sum(available.values())
            multipath = min(PASSENGER_DEMAND_MBPS, total_available)
            bonded = min(PASSENGER_DEMAND_MBPS, total_available)

        best_values.append(best)
        multipath_values.append(multipath)
        bonded_values.append(bonded)

    df["best_link_capacity_mbps"] = best_values
    df["multi_link_capacity_mbps"] = multipath_values
    df["bonded_capacity_mbps"] = bonded_values

    # The bonded model's advantage is meaningful only when more than one
    # link contributes usable capacity at the same decision point.
    improvement = df["bonded_capacity_mbps"] - df["best_link_capacity_mbps"]
    active_bonding_rows = df["multi_link_capacity_mbps"] > df["best_link_capacity_mbps"]

    results = {
        "experiment": "V10 offline multi-link bonding capacity emulation",
        "rows": int(len(df)),
        "passenger_demand_mbps": PASSENGER_DEMAND_MBPS,
        "mean_best_link_mbps": float(df["best_link_capacity_mbps"].mean()),
        "mean_multi_link_mbps": float(df["multi_link_capacity_mbps"].mean()),
        "mean_bonded_mbps": float(df["bonded_capacity_mbps"].mean()),
        "mean_bonding_gain_mbps": float(improvement.mean()),
        "median_bonding_gain_mbps": float(improvement.median()),
        "rows_with_multiple_usable_links": int(active_bonding_rows.sum()),
        "fraction_with_multiple_usable_links": float(active_bonding_rows.mean()),
        "max_bonding_gain_mbps": float(improvement.max()),
        "note": (
            "This is an offline capacity/utilization emulation based on the "
            "synthetic V8.2 dataset. It does not implement packet-level "
            "striping, reordering, retransmission, or true Internet bonding."
        ),
    }

    RESULTS.write_text(
        json.dumps(results, indent=2) + "\n",
        encoding="utf-8",
    )

    print("\n=================================================")
    print(" Railway V10: Multi-Link Bonding Emulation")
    print("=================================================")
    print(f"Rows                              : {results['rows']}")
    print(f"Mean best-link capacity           : {results['mean_best_link_mbps']:.3f} Mbps")
    print(f"Mean multi-link capacity          : {results['mean_multi_link_mbps']:.3f} Mbps")
    print(f"Mean bonded capacity model        : {results['mean_bonded_mbps']:.3f} Mbps")
    print(f"Mean bonding gain                 : {results['mean_bonding_gain_mbps']:.3f} Mbps")
    print(f"Rows with multiple usable links   : {results['rows_with_multiple_usable_links']}")
    print(f"Fraction with multiple usable links: {results['fraction_with_multiple_usable_links'] * 100:.2f}%")
    print(f"Maximum modeled bonding gain      : {results['max_bonding_gain_mbps']:.3f} Mbps")
    print(f"\nSaved: {RESULTS}")
    print("=================================================\n")


if __name__ == "__main__":
    main()
