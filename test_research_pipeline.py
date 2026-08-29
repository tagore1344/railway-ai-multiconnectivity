from pathlib import Path

import pandas as pd

ROOT = Path(__file__).parent


def test_v82_dataset_schema_and_size():
    path = ROOT / "railway-v8.2-dataset.csv"
    df = pd.read_csv(path)
    required = {
        "seed", "time", "train_position",
        "n1_quality", "n2_quality", "n3_quality",
        "n1_loss", "n2_loss", "n3_loss",
        "n1_load", "n2_load", "n3_load",
        "selected_link",
    }
    assert required.issubset(df.columns)
    assert len(df) >= 700
    assert df["seed"].nunique() >= 50


def test_v82_target_can_be_shifted_without_crossing_seeds():
    path = ROOT / "railway-v8.2-dataset.csv"
    df = pd.read_csv(path).sort_values(["seed", "time"])
    df["future"] = df.groupby("seed")["selected_link"].shift(-1)

    for seed, group in df.groupby("seed"):
        assert group["future"].iloc[:-1].notna().all()
        assert pd.isna(group["future"].iloc[-1])


def test_v12_source_exists():
    path = ROOT / "ns3" / "railway_v12_bonding.cc"
    assert path.exists()
    text = path.read_text(encoding="utf-8")
    assert "class BondingSender" in text
    assert "class BondingReceiver" in text
    assert "sequence" in text
