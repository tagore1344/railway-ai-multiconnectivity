# Software-first development plan

The project will be developed and validated in GitHub before hardware is purchased or connected.

## Working rules
- Keep simulation as the default execution mode.
- Label every synthetic value `SIMULATED_NOT_MEASURED`.
- Preserve existing V1–V17 research artifacts; the dashboard will live in its own `software_gateway/` package.
- Use GitHub feature branches and pull requests rather than pushing unfinished feature work directly to `main`.
- Run source compilation and automated tests in GitHub Actions.
- Never describe synthetic outputs as measured railway performance.

## Software milestones
1. Simulation dashboard: telemetry cards, link table, health states, event log, and safe outage injection.
2. Controller testing: quality scoring, allocation recommendations, down-link exclusion, deterministic tests.
3. Dataset workflow: synthetic generator, schema/quality checks, explicit provenance labels.
4. Research evaluation: reactive vs predictive baselines, seed-disjoint evaluation, and archived raw results/configuration.
5. Reproducible simulations: run scripts and CI checks for Python experiments; document ns-3 prerequisites separately.
6. Hardware adapters later: modem, GNSS, routing and Linux interfaces stay behind adapters, but none are required for software validation.

## Current boundary
The dashboard is a local software simulation. It does not configure routes, NAT, firewalls, modems, passenger Wi-Fi, or railway systems. Hardware validation is explicitly deferred.
