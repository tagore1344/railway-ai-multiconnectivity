from __future__ import annotations

"""V11: predictive link selection + multi-link bonding model.

This is an offline research model built on the randomized V8.2 dataset.
It combines the V8.2 RandomForest future-link predictor with a capacity-aware
bonding model. It does not claim real packet-level Internet bonding.
"""

import json
from pathlib import Path

import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score

ROOT = Path(__file__).resolve().parents[1]
DATASET = ROOT / "railway-v8.2-dataset.csv"
RESULTS = ROOT / "v11-predictive-bonding-results.json"

CAPACITY = {1: 30.0, 2: 20.0, 3: 15.0}
QUALITY = {1: "n1_quality", 2: "n2_quality", 3: "n3_quality"}
LOSS = {1: "n1_loss", 2: "n2_loss", 3: "n3_loss"}
FEATURES = [
    "train_position",
    "n1_quality", "n2_quality", "n3_quality",
    "n1_latency", "n2_latency", "n3_latency",
    "n1_loss", "n2_loss", "n3_loss",
    "n1_load", "n2_load", "n3_load",
]


def usable_capacity(row: pd.Series, link: int) -> float:
    q = max(0.0, min(1.0, float(row[QUALITY[link]])))
    loss = max(0.0, min(100.0, float(row[LOSS[link]]))) / 100.0
    return CAPACITY[link] * q * (1.0 - loss)


def main() -> None:
    df = pd.read_csv(DATASET).sort_values(["seed", "time"]).reset_index(drop=True)
    df["future_link"] = df.groupby("seed")["selected_link"].shift(-1)
    df = df.dropna(subset=["future_link"]).copy()
    df["future_link"] = df["future_link"].astype(int)

    train = df[df["seed"] < 1049].copy()
    test = df[df["seed"] >= 1049].copy()

    model = RandomForestClassifier(
        n_estimators=400,
        random_state=42,
        class_weight="balanced",
        min_samples_leaf=2,
        n_jobs=-1,
    )
    model.fit(train[FEATURES], train["future_link"])
    test["predicted_link"] = model.predict(test[FEATURES])

    test["reactive_link"] = test["selected_link"].astype(int)

    rows = []
    for _, row in test.iterrows():
        caps = {link: usable_capacity(row, link) for link in (1, 2, 3)}
        available = {k: v for k, v in caps.items() if v > 0.05}
        predicted = int(row["predicted_link"])
        reactive = int(row["reactive_link"])

        reactive_capacity = caps.get(reactive, 0.0)
        predictive_capacity = caps.get(predicted, 0.0)
        bonded_capacity = sum(available.values())

        rows.append((reactive_capacity, predictive_capacity, bonded_capacity))

    values = pd.DataFrame(
        rows,
        columns=["reactive_capacity", "predictive_capacity", "bonded_capacity"],
    )

    predictive_accuracy = accuracy_score(test["future_link"], test["predicted_link"])
    reactive_accuracy = accuracy_score(test["future_link"], test["reactive_link"])

    results = {
        "experiment": "V11 predictive selection plus bonding-capacity model",
        "test_seeds": sorted(test["seed"].unique().tolist()),
        "test_rows": int(len(test)),
        "reactive_next_step_accuracy": float(reactive_accuracy),
        "predictive_next_step_accuracy": float(predictive_accuracy),
        "predictive_accuracy_gain_percentage_points": float((predictive_accuracy - reactive_accuracy) * 100),
        "mean_reactive_capacity_mbps": float(values["reactive_capacity"].mean()),
        "mean_predictive_capacity_mbps": float(values["predictive_capacity"].mean()),
        "mean_bonded_capacity_mbps": float(values["bonded_capacity"].mean()),
        "mean_predictive_gain_mbps": float((values["predictive_capacity"] - values["reactive_capacity"]).mean()),
        "mean_bonding_gain_over_reactive_mbps": float((values["bonded_capacity"] - values["reactive_capacity"]).mean()),
        "limitation": (
            "V11 is an offline synthetic capacity model. It does not implement "
            "packet striping, sequence-space scheduling, reordering, retransmission, "
            "MPTCP, MPQUIC, or a production bonding tunnel."
        ),
    }

    RESULTS.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    print("\n=================================================")
    print(" Railway V11: Predictive + Bonding")
    print("=================================================")
    print(f"Test rows                         : {results['test_rows']}")
    print(f"Predictive accuracy               : {predictive_accuracy * 100:.2f}%")
    print(f"Reactive accuracy                 : {reactive_accuracy * 100:.2f}%")
    print(f"Mean reactive capacity            : {results['mean_reactive_capacity_mbps']:.3f} Mbps")
    print(f"Mean predictive capacity          : {results['mean_predictive_capacity_mbps']:.3f} Mbps")
    print(f"Mean bonded capacity              : {results['mean_bonded_capacity_mbps']:.3f} Mbps")
    print(f"Predictive gain                   : {results['mean_predictive_gain_mbps']:.3f} Mbps")
    print(f"Bonding gain over reactive        : {results['mean_bonding_gain_over_reactive_mbps']:.3f} Mbps")
    print(f"Saved                             : {RESULTS}")
    print("=================================================\n")


if __name__ == "__main__":
    main()
