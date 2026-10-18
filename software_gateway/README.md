# Railway Software Gateway — Simulation Mode

A software-only dashboard and API for developing the railway multi-connectivity controller before hardware is available. It runs on Windows, Linux, or GitHub Codespaces using synthetic data.

> **Evidence label:** all values produced by this simulator are `SIMULATED_NOT_MEASURED`. They are not real cellular measurements or railway performance results.

## Run locally

From the repository root:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r software_gateway/requirements.txt
python -m uvicorn software_gateway.app.main:app --reload
```

Open the dashboard at http://127.0.0.1:8000, API docs at http://127.0.0.1:8000/docs, or health check at http://127.0.0.1:8000/health.

## Tests

From the repository root:

```powershell
python -m pytest -q software_gateway/tests
python -m compileall -q software_gateway/app software_gateway/tests
```

GitHub Actions runs these checks for pushes and pull requests.

## Included

- Three simulated WAN paths and continuously changing synthetic telemetry.
- Link-quality scoring from RTT, packet loss, and signal strength.
- Recommended traffic allocation that excludes down links.
- Simulated outage injection and recovery.
- A rolling RTT chart and event log.
- Read-only monitoring UI; no system routes or network settings are changed.

## API

- `GET /health`
- `GET /api/status`
- `GET /api/dashboard` — advances the simulation by one sample
- `GET /api/links`
- `GET /api/history?limit=120`
- `GET /api/events?limit=80`
- `POST /api/simulation/fault` — body: `{"link_id":"wan1","faulted":true}`
- `WS /ws` — streams simulated dashboard snapshots

## GitHub-first workflow

Work on a feature branch and open a pull request. CI checks source compilation and tests. GitHub is the source of truth; the simulator does not require a Raspberry Pi, cellular modem, GNSS receiver, or SIM cards.

A GitHub repository does not itself run a permanent web server. For interactive execution use a local environment or GitHub Codespaces; a hosted demo requires a suitable app-hosting service.
