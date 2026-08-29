from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "v11" / "predictive_bonding_model.py"

spec = importlib.util.spec_from_file_location("v11_model", MODULE_PATH)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def test_usable_capacity_bounds():
    row = {
        "n1_quality": 1.0,
        "n1_loss": 0.0,
    }
    assert module.usable_capacity(row, 1) == 30.0

    row["n1_quality"] = 0.5
    row["n1_loss"] = 10.0
    assert abs(module.usable_capacity(row, 1) - 13.5) < 1e-9


def test_capacity_cannot_exceed_nominal():
    row = {"n2_quality": 1.5, "n2_loss": -20.0}
    assert module.usable_capacity(row, 2) == 20.0


def test_dataset_exists():
    assert (ROOT / "railway-v8.2-dataset.csv").exists()
