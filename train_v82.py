import pandas as pd

from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score, classification_report

DATASET = "railway-v8.2-dataset.csv"

df = pd.read_csv(DATASET)

# Sort each randomized simulation by time.
df = df.sort_values(["seed", "time"]).reset_index(drop=True)

# Predict the network selected at the NEXT controller decision.
df["future_selected_link"] = (
    df.groupby("seed")["selected_link"].shift(-1)
)

df = df.dropna(
    subset=["future_selected_link"]
).copy()

df["future_selected_link"] = (
    df["future_selected_link"].astype(int)
)

# ---------------------------------------------------------
# IMPORTANT:
# Do not use current selected_link or current controller
# scores. Those are outputs of the existing decision logic.
# ---------------------------------------------------------

features = [
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

X = df[features]
y = df["future_selected_link"]

# ---------------------------------------------------------
# SEED-BASED HOLDOUT
#
# Training: seeds 1001-1048
# Testing : seeds 1049-1060
# ---------------------------------------------------------

train_mask = df["seed"] < 1049
test_mask = df["seed"] >= 1049

X_train = X[train_mask]
X_test = X[test_mask]

y_train = y[train_mask]
y_test = y[test_mask]

model = RandomForestClassifier(
    n_estimators=300,
    random_state=42,
    class_weight="balanced",
    min_samples_leaf=2,
)

model.fit(X_train, y_train)

predictions = model.predict(X_test)

accuracy = accuracy_score(
    y_test,
    predictions
)

print("\n=================================================")
print(" Railway Connectivity Predictor V8.2")
print(" UNSEEN-SEED FUTURE PREDICTION")
print("=================================================\n")

print(f"Total usable samples : {len(df)}")
print(f"Training samples     : {len(X_train)}")
print(f"Testing samples      : {len(X_test)}")
print(f"Training seeds       : {df.loc[train_mask, 'seed'].nunique()}")
print(f"Testing seeds        : {df.loc[test_mask, 'seed'].nunique()}")

print(
    f"\nFuture prediction accuracy: "
    f"{accuracy * 100:.2f}%\n"
)

print("Classification report:")
print(
    classification_report(
        y_test,
        predictions,
        zero_division=0
    )
)

importance = pd.Series(
    model.feature_importances_,
    index=features
).sort_values(
    ascending=False
)

print("\nFeature importance:")
print(importance.to_string())

print("\n=================================================")
