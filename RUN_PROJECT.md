# Running the project

## WSL + ns-3

From Windows PowerShell or Command Prompt:

```powershell
wsl -d ubuntu
```

Then:

```bash
cd ~/ns-3.48
./ns3 build -j1
```

The historical railway program is under `scratch/railway/` in the local ns-3 tree. Do not keep two runnable `.cc` files containing `main()` in the same scratch subtree unless both are intended ns-3 targets.

## Python analysis

From the repository root:

```bash
sudo apt install -y python3-pandas python3-sklearn python3-matplotlib
python3 train_v82.py
python3 v9_predictive_vs_reactive.py
python3 v10_bonding_emulation.py
python3 v11_predictive_bonding_experiment.py
```

## V12

`ns3/railway_v12_bonding.cc` is a self-contained packet-level prototype. Copy it into the local ns-3 `scratch/railway/` directory, compile, and run it locally. The file has not been executed in this environment, so treat compilation and runtime results as unvalidated until the local test succeeds.

## Research rule

Do not report simulation/model outputs as real railway-network performance. Keep seed splits and configuration parameters with every reported result.
