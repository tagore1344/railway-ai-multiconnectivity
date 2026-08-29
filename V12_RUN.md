# V12 Local Validation

The V12 source lives at `v12/railway_v12_bonding.cc`.

Because ns-3 discovers scratch programs from the local filesystem, the source must be copied into `~/ns-3.48/scratch/railway/` before compilation.

```bash
cd ~/ns-3.48
cp ~/railway-ai-multiconnectivity/v12/railway_v12_bonding.cc scratch/railway/
./ns3 build -j1
./ns3 run railway_v12_bonding
```

Baseline comparison with bonding disabled:

```bash
./ns3 run 'railway_v12_bonding --bonding=false'
```

Useful parameterized runs:

```bash
./ns3 run 'railway_v12_bonding --seed=1001 --simulationSeconds=30'
./ns3 run 'railway_v12_bonding --bonding=true --packetBytes=1200'
```

The output reports received packets, delivered packets, dropped/late packets, reordered packets, and effective application goodput.

Do not copy this C++ file into `scratch/railway/` alongside another source containing `main()` unless you intentionally want multiple scratch targets. ns-3's scratch build checks for multiple independent programs.

V12 numerical results are considered validated only after successful compilation and execution in the local ns-3.48 environment.
