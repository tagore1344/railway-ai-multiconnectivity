from __future__ import annotations

import json
from pathlib import Path
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score

ROOT = Path(__file__).resolve().parents[1]
DATASET = ROOT / "railway-v8.2-dataset.csv"
RESULTS = ROOT / "v9-integrated-results.json"
FEATURES = [
    "train_position", "n1_quality", "n2_quality", "n3_quality",
    "n1_latency", "n2_latency", "n3_latency",
    "n1_loss", "n2_loss", "n3_loss",
    "n1_load", "n2_load", "n3_load",
]
SCORES = ["n1_score", "n2_score", "n3_score"]

def main() -> None:
    df = pd.read_csv(DATASET).sort_values(["seed", "time"]).reset_index(drop=True)
    df["future_link"] = df.groupby("seed")["selected_link"].shift(-1)
    df = df.dropna(subset=["future_link"]).copy()
    df["future_link"] = df["future_link"].astype(int)
    train = df[df["seed"] < 1049]
    test = df[df["seed"] >= 1049].copy()

    model = RandomForestClassifier(
        n_estimators=400, random_state=42,
        class_weight="balanced", min_samples_leaf=2, n_jobs=-1
    )
    model.fit(train[FEATURES], train["future_link"])
    test["predictive_link"] = model.predict(test[FEATURES])
    test["reactive_link"] = test["selected_link"].astype(int)

    actual = test["future_link"]
    predictive_acc = accuracy_score(actual, test["predictive_link"])
    reactive_acc = accuracy_score(actual, test["reactive_link"])
    score_matrix = test[SCORES].to_numpy()

    def picked_mean(column: str) -> float:
        values = [row[int(link) - 1] for row, link in zip(score_matrix, test[column])]
        return float(sum(values) / len(values))

    predictive_score = picked_mean("predictive_link")
    reactive_score = picked_mean("reactive_link")
    oracle_score = picked_mean("future_link")
    switch_needed = test["reactive_link"] != test["future_link"]
    prepared = switch_needed & (test["predictive_link"] == test["future_link"])

    results = {
        "experiment": "V9 integrated predictive policy replay",
        "test_rows": int(len(test)),
        "test_seeds": sorted(test["seed"].unique().tolist()),
        "reactive_next_step_accuracy": float(reactive_acc),
        "predictive_next_step_accuracy": float(predictive_acc),
        "accuracy_gain_percentage_points": float((predictive_acc - reactive_acc) * 100),
        "reactive_mean_next_score": reactive_score,
        "predictive_mean_next_score": predictive_score,
        "oracle_mean_next_score": oracle_score,
        "next_score_gain": predictive_score - reactive_score,
        "required_switches": int(switch_needed.sum()),
        "switches_predicted_ahead": int(prepared.sum()),
        "switch_preparation_rate": float(prepared.sum() / switch_needed.sum()) if switch_needed.sum() else 0.0,
        "limitation": "Offline replay over synthetic V8.2 traces; no packet-level throughput claim.",
    }
    RESULTS.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    print("\n=================================================")
    print(" Railway V9 — Integrated Predictive Policy")
    print("=================================================")
    print(f"Test rows                     : {len(test)}")
    print(f"Reactive accuracy             : {reactive_acc * 100:.2f}%")
    print(f"Predictive accuracy           : {predictive_acc * 100:.2f}%")
    print(f"Accuracy gain                 : {(predictive_acc - reactive_acc) * 100:.2f} percentage points")
    print(f"Reactive mean next score      : {reactive_score:.4f}")
    print(f"Predictive mean next score    : {predictive_score:.4f}")
    print(f"Oracle mean next score        : {oracle_score:.4f}")
    print(f"Required switches             : {int(switch_needed.sum())}")
    print(f"Predicted ahead               : {int(prepared.sum())}")
    print(f"Preparation rate              : {results['switch_preparation_rate'] * 100:.2f}%")
    print(f"Saved                         : {RESULTS}")
    print("=================================================\n")

if __name__ == "__main__":
    main()
