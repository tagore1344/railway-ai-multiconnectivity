from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
from sklearn.ensemble import RandomForestClassifier

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "railway-v8.2-dataset.csv"
OUT = ROOT / "v11-predictive-bonding-results.json"

FEATURES = [
    "train_position",
    "n1_quality", "n2_quality", "n3_quality",
    "n1_latency", "n2_latency", "n3_latency",
    "n1_loss", "n2_loss", "n3_loss",
    "n1_load", "n2_load", "n3_load",
]
SCORES = ["n1_score", "n2_score", "n3_score"]


def main() -> None:
    df = pd.read_csv(DATA).sort_values(["seed", "time"]).reset_index(drop=True)
    df["future_link"] = df.groupby("seed")["selected_link"].shift(-1)
    df = df.dropna(subset=["future_link"]).copy()
    df["future_link"] = df["future_link"].astype(int)

    train = df[df["seed"] < 1049]
    test = df[df["seed"] >= 1049].copy()

    model = RandomForestClassifier(
        n_estimators=500,
        random_state=42,
        class_weight="balanced",
        min_samples_leaf=2,
        n_jobs=-1,
    )
    model.fit(train[FEATURES], train["future_link"])
    test["predicted_link"] = model.predict(test[FEATURES])
    test["reactive_link"] = test["selected_link"].astype(int)
    test["actual_link"] = test["future_link"].astype(int)

    score_matrix = test[SCORES].to_numpy()

    def picked_mean(column: str) -> float:
        values = [row[int(link) - 1] for row, link in zip(score_matrix, test[column])]
        return float(sum(values) / len(values))

    reactive_score = picked_mean("reactive_link")
    predictive_score = picked_mean("predicted_link")
    oracle_score = picked_mean("actual_link")

    results = {
        "experiment": "V11 predictive bonding policy evaluation",
        "training_seeds": sorted(train["seed"].unique().tolist()),
        "testing_seeds": sorted(test["seed"].unique().tolist()),
        "test_rows": int(len(test)),
        "predictive_next_step_accuracy": float((test["predicted_link"] == test["actual_link"]).mean()),
        "reactive_next_step_accuracy": float((test["reactive_link"] == test["actual_link"]).mean()),
        "reactive_mean_score": reactive_score,
        "predictive_mean_score": predictive_score,
        "oracle_mean_score": oracle_score,
        "score_gain": predictive_score - reactive_score,
        "note": "Offline replay only. This does not measure packet-level throughput or end-to-end bonding goodput.",
    }
    OUT.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    print("\nV11 Predictive + Bonding Evaluation")
    print("====================================")
    print(f"Test rows                 : {len(test)}")
    print(f"Predictive accuracy       : {results['predictive_next_step_accuracy']*100:.2f}%")
    print(f"Reactive accuracy         : {results['reactive_next_step_accuracy']*100:.2f}%")
    print(f"Reactive mean score      : {reactive_score:.4f}")
    print(f"Predictive mean score    : {predictive_score:.4f}")
    print(f"Oracle mean score        : {oracle_score:.4f}")
    print(f"Predictive score gain    : {results['score_gain']:.4f}")
    print(f"Saved                    : {OUT}")


if __name__ == "__main__":
    main()
