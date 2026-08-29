from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

steps = [
    [sys.executable, str(ROOT / "train_v8.py")],
    [sys.executable, str(ROOT / "train_v81.py")],
    [sys.executable, str(ROOT / "train_v82.py")],
    [sys.executable, str(ROOT / "v9_predictive_vs_reactive.py")],
    [sys.executable, str(ROOT / "v10_bonding_emulation.py")],
    [sys.executable, str(ROOT / "v11" / "predictive_bonding_model.py")],
]

for command in steps:
    print("\n>>>", " ".join(command))
    result = subprocess.run(command, cwd=ROOT)
    if result.returncode != 0:
        raise SystemExit(result.returncode)

print("\nAll offline research analyses completed successfully.")
