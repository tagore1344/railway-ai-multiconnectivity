# V13–V17 Execution Guide

## Current repository state
V13–V17 prototype/design files are present in the repository. They are not experimentally validated until executed and their outputs are archived.

## Execution order
1. Run V13 predictive-bonding controller against the unseen-seed V8.2 dataset.
2. Run V14 failover tests with healthy/degraded/failed links.
3. Run V15 passenger-aware allocation tests at multiple capacities and passenger mixes.
4. Run V16 route/digital-twin predictions over multiple speeds and forecast horizons.
5. Use V17 as the hardware implementation specification; collect real multi-modem measurements before claiming hardware performance.

## Evidence policy
Record raw stdout/CSV/JSON, version/commit, input dataset, random seeds, and configuration for every experiment. Do not label synthetic results as real railway measurements.

## Important correction for the ns-3 path
The current V12.5 source reports the offered rate with a unit-label bug (the numerical value is 64000 while the configured interval corresponds to about 64 Mbps). Preserve the raw run output, but fix the unit conversion in the next ns-3 revision before using the printed value in a paper.
