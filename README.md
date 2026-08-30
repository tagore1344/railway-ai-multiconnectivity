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
- V12.2–V12.5: Packet-level striping, reordering, bounded buffering, and latency-aware bonding prototypes
- V13: Predictive path selection + bonding controller
- V14: Autonomous failover/recovery controller
- V15: Passenger-aware traffic allocation
- V16: Route/digital-twin prediction
- V17: Hardware gateway architecture

## Current validated evidence

- V7 dataset: 840 observations from 40 simulation conditions.
- V8.2: 99.24% next-network prediction accuracy on 132 observations from 12 unseen randomized seeds.
- V12.5 local ns-3 run: 61.7514 Mbps unique received goodput and 59.8382 Mbps in-order goodput with bonding enabled, compared with 28.966 Mbps and 27.0396 Mbps for the single-link baseline under the same 64 Mbps offered load.

These are simulation results from the project's synthetic ns-3 model. They do not establish real-world railway or cellular-network performance.

## Repository structure

```text
v11/                         Predictive + bonding integration
v12/                         Packet-level bonding experiments
v13/                         Predictive bonding controller
v14/                         Failover/recovery controller
v15/                         Passenger-aware traffic policy
v16/                         Route digital twin
v17/                         Hardware gateway specification
v9_predictive_vs_reactive.py Offline V9 policy evaluator
v10_bonding_emulation.py      V10 capacity/packet model
train_v81.py                  V8.1 predictor
train_v82.py                  V8.2 unseen-seed predictor
railway-v7-dataset.csv        V7 dataset
railway-v8.2-dataset.csv      Randomized V8.2 dataset
```

## Research roadmap

```text
adaptive monitoring
  -> future-link prediction
  -> predictive bonding
  -> failover/recovery
  -> passenger-aware policy
  -> route/digital-twin prediction
  -> packet-level bonding
  -> hardware gateway
  -> real-world multi-WAN/railway validation
```

## Validation boundary

No V13–V17 stage is considered experimentally validated until it is executed and its raw results are archived. Offline models and hardware designs must not be described as measured railway performance.
