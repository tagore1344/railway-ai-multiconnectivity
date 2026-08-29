# V9 — Reactive vs Predictive Gateway Evaluation

V9 moves the project from **prediction accuracy** to **control-policy evaluation**.

## Research question

> Does using a predicted next-best network provide a better decision than waiting for the current reactive controller to switch?

## Compared policies

- **Reactive:** use the network selected by the current controller at time `t`.
- **Predictive:** train on earlier randomized simulation seeds and predict the network that will be selected at `t + Δt`.
- **Oracle:** use the actual next selected network as an upper-bound reference.

## Evaluation

The script `v9_predictive_vs_reactive.py`:

1. Loads `railway-v8.2-dataset.csv`.
2. Creates a next-decision target for each seed.
3. Trains a Random Forest only on seeds `< 1049`.
4. Evaluates on unseen seeds `>= 1049`.
5. Compares reactive and predictive next-step decisions.
6. Reports next-step accuracy, switch-preparation rate, and mean next-state controller score.

The score comparison is an **offline synthetic utility metric**, not measured network throughput. The V8.2 dataset does not contain per-interval throughput, so throughput improvement must be evaluated in a future ns-3 experiment with interval-level traffic measurements.

## Current milestone

- V8.2 unseen-seed prediction accuracy: **99.24%** on 132 test observations.
- V9 objective: determine whether that predictive capability translates into a better gateway policy.

## Next

After V9, the project should integrate the predictive policy into ns-3 itself and compare reactive vs predictive gateways under the same randomized scenarios using real simulated throughput, latency, packet loss, switching count, and degraded-link exposure.
