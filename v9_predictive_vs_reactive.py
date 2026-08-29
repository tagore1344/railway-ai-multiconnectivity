from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score

DATASET = Path("railway-v8.2-dataset.csv")
RESULTS = Path("v9-results.json")

FEATURES = [
    "train_position",
    "n1_quality",
    "n2_quality",
    "n3_quality",
    "n1_latency",
    "n2_latency",
    "n3_latency",
    "n1_loss",
    "n2_loss",
    "n3_loss",
    "n1_load",
    "n2_load",
    "n3_load",
]

LINK_SCORE_COLUMNS = ["n1_score", "n2_score", "n3_score"]
LINK_QUALITY_COLUMNS = ["n1_quality", "n2_quality", "n3_quality"]


def choose_best_available(row: pd.Series) -> int:
    values = row[LINK_SCORE_COLUMNS].astype(float).to_numpy()
    if values.max() <= 0:
        # No usable link in this synthetic observation; keep a safe default.
        return 1
    return int(values.argmax() + 1)


def main() -> None:
    if not DATASET.exists():
        raise FileNotFoundError(f"Missing dataset: {DATASET}")

    df = pd.read_csv(DATASET)
    df = df.sort_values(["seed", "time"]).reset_index(drop=True)

    # Future label: which link the reference controller selected at t+1.
    df["future_selected_link"] = (
        df.groupby("seed")["selected_link"].shift(-1)
    )
    df = df.dropna(subset=["future_selected_link"]).copy()
    df["future_selected_link"] = df["future_selected_link"].astype(int)

    train_mask = df["seed"] < 1049
    test_mask = df["seed"] >= 1049

    train = df.loc[train_mask].copy()
    test = df.loc[test_mask].copy()

    model = RandomForestClassifier(
        n_estimators=300,
        random_state=42,
        class_weight="balanced",
        min_samples_leaf=2,
    )
    model.fit(train[FEATURES], train["future_selected_link"])
    test["predicted_link"] = model.predict(test[FEATURES])

    # Reactive baseline: use the controller's current choice at time t.
    test["reactive_link"] = test["selected_link"].astype(int)
    test["actual_next_link"] = test["future_selected_link"].astype(int)

    predictive_accuracy = accuracy_score(
        test["actual_next_link"], test["predicted_link"]
    )
    reactive_accuracy = accuracy_score(
        test["actual_next_link"], test["reactive_link"]
    )

    # Offline utility proxy: the next interval's link score.
    # This is intentionally not called throughput: the dataset does not
    # contain interval throughput for V8.2, so using it as throughput would
    # overstate what the experiment proves.
    score_matrix = test[LINK_SCORE_COLUMNS].to_numpy()

    predictive_scores = [
        row[link - 1] for row, link in zip(score_matrix, test["predicted_link"])
    ]
    reactive_scores = [
        row[link - 1] for row, link in zip(score_matrix, test["reactive_link"])
    ]
    oracle_scores = [
        row[link - 1] for row, link in zip(score_matrix, test["actual_next_link"])
    ]

    predictive_mean_score = float(sum(predictive_scores) / len(predictive_scores))
    reactive_mean_score = float(sum(reactive_scores) / len(reactive_scores))
    oracle_mean_score = float(sum(oracle_scores) / len(oracle_scores))

    # A switch is counted when the next selected network differs from the
    # current decision. Predictive lead-ahead correctness means the model
    # picks that upcoming network before the next observation arrives.
    required_switches = int(
        (test["reactive_link"] != test["actual_next_link"]).sum()
    )
    predictive_prepared = int(
        ((test["reactive_link"] != test["actual_next_link"]) &
         (test["predicted_link"] == test["actual_next_link"])).sum()
    )

    results = {
        "experiment": "V9 reactive vs predictive offline policy evaluation",
        "total_test_rows": int(len(test)),
        "testing_seeds": sorted(test["seed"].unique().tolist()),
        "reactive_next_step_accuracy": reactive_accuracy,
        "predictive_next_step_accuracy": predictive_accuracy,
        "predictive_accuracy_gain": predictive_accuracy - reactive_accuracy,
        "reactive_mean_next_score": reactive_mean_score,
        "predictive_mean_next_score": predictive_mean_score,
        "oracle_mean_next_score": oracle_mean_score,
        "next_step_score_gain": predictive_mean_score - reactive_mean_score,
        "required_switches": required_switches,
        "predictive_switches_prepared": predictive_prepared,
        "switch_preparation_rate": (
            predictive_prepared / required_switches
            if required_switches
            else 0.0
        ),
        "note": (
            "The score comparison is an offline synthetic utility metric. "
            "It is not measured throughput because the V8.2 dataset does not "
            "contain per-interval throughput."
        ),
    }

    RESULTS.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    print("\n=================================================")
    print(" Railway V9: Reactive vs Predictive")
    print("=================================================")
    print(f"Testing rows                 : {len(test)}")
    print(f"Testing seeds                : {len(results['testing_seeds'])}")
    print(f"Reactive next-step accuracy  : {reactive_accuracy * 100:.2f}%")
    print(f"Predictive next-step accuracy: {predictive_accuracy * 100:.2f}%")
    print(f"Accuracy gain                : {(predictive_accuracy - reactive_accuracy) * 100:.2f} percentage points")
    print(f"Reactive mean next score     : {reactive_mean_score:.4f}")
    print(f"Predictive mean next score   : {predictive_mean_score:.4f}")
    print(f"Oracle mean next score       : {oracle_mean_score:.4f}")
    print(f"Required next-step switches  : {required_switches}")
    print(f"Predictively prepared        : {predictive_prepared}")
    print(f"Preparation rate             : {results['switch_preparation_rate'] * 100:.2f}%")
    print("\nSaved:", RESULTS)
    print("=================================================\n")


if __name__ == "__main__":
    main()
