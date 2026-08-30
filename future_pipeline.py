from __future__ import annotations
from pathlib import Path
import subprocess, sys

ROOT=Path(__file__).resolve().parent
steps=[
    [sys.executable,str(ROOT/'v13'/'predictive_bonding_controller.py')],
]
for cmd in steps:
    print('>>>',' '.join(cmd)); subprocess.run(cmd,cwd=ROOT,check=True)
print('V13-V17 package installed as design + offline controller modules; ns-3/hardware stages require local execution.')
