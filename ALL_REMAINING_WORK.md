# Railway Multi-Connectivity Gateway — V13–V17 Batch

This batch packages the remaining major engineering stages in one reproducible plan.

## V13 — Predictive + bonding
Integrate the V8.2 unseen-seed Random Forest into the packet scheduler. Prediction selects a preferred future path; bonding still permits multiple eligible links.

## V14 — Autonomous failover and recovery
Track health, detect unusable paths, switch traffic, and restore a recovered path only after hysteresis/cooldown.

## V15 — Passenger-aware policy
Partition traffic by service class and allocate capacity according to priority and minimum-rate requirements.

## V16 — Route digital twin
Predict future link state from train position and speed plus route/cell geometry. Weather and tower intelligence remain experimental inputs, not assumed capabilities.

## V17 — Hardware gateway
Translate the validated software architecture into a Linux edge gateway with multiple modem interfaces, local inference, watchdog/recovery, telemetry, and a real multipath transport/tunnel.

## Validation rule
No V13–V17 number is presented as experimentally validated until it is executed and its raw results are archived. The existing V8.2 99.24% result and V12.5 ~61.75 Mbps / ~59.84 Mbps result remain the current validated milestones from local runs.
