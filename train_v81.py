import pandas as pd

from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score, classification_report
from sklearn.model_selection import GroupShuffleSplit

DATASET = "railway-v7-dataset.csv"

df = pd.read_csv(DATASET)

# Each combination represents one simulation run.
df["run_id"] = (
    df["train_speed"].astype(str)
    + "_"
    + df["passenger_count"].astype(str)
    + "_"
    + df["decision_interval"].astype(str)
)

# Sort within each simulation run.
df = df.sort_values(
    ["run_id", "time"]
).reset_index(drop=True)

# ------------------------------------------------------------
# FUTURE TARGET
#
# Predict the selected network at the NEXT controller
# decision, using only information available at the current
# decision.
# ------------------------------------------------------------

df["future_selected_link"] = (
    df.groupby("run_id")["selected_link"]
    .shift(-1)
)

# Last observation of each run has no future label.
df = df.dropna(
    subset=["future_selected_link"]
).copy()

df["future_selected_link"] = (
    df["future_selected_link"].astype(int)
)

# ------------------------------------------------------------
# FEATURES
#
# Deliberately exclude:
#   selected_link
#   future_selected_link
#
# Current controller scores are also excluded so that the
# model relies on raw observable network conditions.
# ------------------------------------------------------------

features = [
    "train_position",
    "train_speed",
    "passenger_count",
    "decision_interval",

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

groups = df["run_id"]

# ------------------------------------------------------------
# GROUPED TRAIN/TEST SPLIT
#
# Entire simulation runs stay together.
# ------------------------------------------------------------

splitter = GroupShuffleSplit(
    n_splits=1,
    test_size=0.20,
    random_state=42,
)

train_idx, test_idx = next(
    splitter.split(X, y, groups=groups)
)

X_train = X.iloc[train_idx]
X_test = X.iloc[test_idx]

y_train = y.iloc[train_idx]
y_test = y.iloc[test_idx]

train_groups = groups.iloc[train_idx]
test_groups = groups.iloc[test_idx]

# ------------------------------------------------------------
# MODEL
# ------------------------------------------------------------

model = RandomForestClassifier(
    n_estimators=300,
    random_state=42,
    class_weight="balanced",
    min_samples_leaf=2,
)

model.fit(
    X_train,
    y_train
)

predictions = model.predict(X_test)

accuracy = accuracy_score(
    y_test,
    predictions
)

# ------------------------------------------------------------
# RESULTS
# ------------------------------------------------------------

print("\n=================================================")
print(" Railway Connectivity Predictor V8.1")
print(" FUTURE NETWORK PREDICTION")
print("=================================================\n")

print(
    f"Usable samples      : {len(df)}"
)

print(
    f"Simulation runs     : {df['run_id'].nunique()}"
)

print(
    f"Training samples    : {len(X_train)}"
)

print(
    f"Testing samples     : {len(X_test)}"
)

print(
    f"Training runs       : {train_groups.nunique()}"
)

print(
    f"Testing runs        : {test_groups.nunique()}"
)

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

# ------------------------------------------------------------
# FEATURE IMPORTANCE
# ------------------------------------------------------------

importance = pd.Series(
    model.feature_importances_,
    index=features,
).sort_values(
    ascending=False
)

print("\nFeature importance:")
print(
    importance.to_string()
)

print("\n=================================================")
