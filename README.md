# AI Railway Multi-Connectivity Gateway

Simulation and machine-learning research prototype for reliable passenger Wi-Fi on high-mobility railway networks.

## Project progression

- V1: Multi-connectivity baseline
- V2: Capacity-aware load balancing
- V3: Automatic failover
- V4: Train mobility and changing link quality
- V5.1: Traffic measurement and time-series logging
- V6: Adaptive link monitoring and scoring
- V7: Multi-condition simulation dataset
- V8/V8.1: Machine-learning prediction baselines
- V8.2: Randomized conditions and unseen-seed future-network prediction
- V9: Reactive vs predictive policy evaluation
- V10: Multi-link bonding capacity-model stage
- V11: Predictive selection + bonding-capacity integration

## Current evidence

- V7 dataset: 840 observations from 40 simulation conditions.
- V8.2 dataset: 720 observations from 60 randomized seeds.
- V8.2 future-network Random Forest: 99.24% accuracy on 132 observations from 12 unseen seeds.

These are results from the project's synthetic ns-3 model and should not be interpreted as measured railway or cellular-network performance.

## Repository structure

```text
v11/                       Predictive + bonding integration
v9_predictive_vs_reactive.py   Offline V9 policy evaluator
v10_bonding_emulation.py       V10 capacity-model evaluator
train_v81.py                    V8.1 predictor
train_v82.py                    V8.2 unseen-seed predictor
railway-v7-dataset.csv          V7 dataset
railway-v8.2-dataset.csv        Randomized V8.2 dataset
```

## Research roadmap

```text
Simulation
  -> adaptive monitoring
  -> future-link prediction
  -> reactive vs predictive evaluation
  -> bonding-capacity model
  -> predictive + bonding integration
  -> packet-level bonding in ns-3
  -> hardware gateway prototype
  -> real-world measurements
```

## Important limitation

The current V10/V11 bonding work is still an offline capacity model. It does not yet prove real Internet speed aggregation or implement production packet-level bonding. The next engineering milestone is packet-level striping, sequencing, reordering/reassembly, retransmission handling, and measured goodput in ns-3 or a real multi-WAN testbed.
