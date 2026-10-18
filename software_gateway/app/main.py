"""FastAPI entry point for the software-only railway gateway."""
from __future__ import annotations

import asyncio
from pathlib import Path

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from .simulator import RailwaySimulator

APP_DIR = Path(__file__).parent
simulator = RailwaySimulator()

app = FastAPI(
    title="Railway Multi-Connectivity Gateway — Software Simulator",
    version="0.1.0",
    description="Software-only research prototype. All generated telemetry is synthetic.",
)
app.mount("/static", StaticFiles(directory=str(APP_DIR / "static")), name="static")


class FaultRequest(BaseModel):
    link_id: str = Field(min_length=4, max_length=8, pattern=r"^wan[1-3]$")
    faulted: bool


@app.get("/", response_class=HTMLResponse, include_in_schema=False)
def dashboard() -> str:
    return (APP_DIR / "static" / "index.html").read_text(encoding="utf-8")


@app.get("/health")
def health() -> dict[str, str]:
    return {
        "status": "ok",
        "mode": "SIMULATION",
        "evidence_label": "SIMULATED_NOT_MEASURED",
    }


@app.get("/api/status")
def status() -> dict:
    state = simulator.snapshot()
    return {
        "status": "running",
        "mode": state["mode"],
        "evidence_label": state["evidence_label"],
        "tick": state["tick"],
        "active_links": state["active_links"],
        "total_links": state["total_links"],
    }


@app.get("/api/dashboard")
def dashboard_data() -> dict:
    return simulator.step()


@app.get("/api/links")
def links() -> dict:
    return {
        "links": simulator.snapshot()["links"],
        "evidence_label": "SIMULATED_NOT_MEASURED",
    }


@app.get("/api/history")
def history(limit: int = 120) -> dict:
    if not 1 <= limit <= 180:
        raise HTTPException(status_code=422, detail="limit must be between 1 and 180")
    return {
        "history": list(simulator.history)[-limit:],
        "evidence_label": "SIMULATED_NOT_MEASURED",
    }


@app.get("/api/events")
def events(limit: int = 80) -> dict:
    if not 1 <= limit <= 100:
        raise HTTPException(status_code=422, detail="limit must be between 1 and 100")
    return {"events": list(simulator.events)[:limit]}


@app.post("/api/simulation/fault")
def inject_fault(request: FaultRequest) -> dict:
    try:
        state = simulator.set_fault(request.link_id, request.faulted)
    except KeyError:
        raise HTTPException(status_code=404, detail="Unknown simulated link") from None
    return {"ok": True, "dashboard": state}


@app.websocket("/ws")
async def websocket_feed(websocket: WebSocket) -> None:
    await websocket.accept()
    try:
        while True:
            await websocket.send_json(simulator.step())
            await asyncio.sleep(2)
    except (WebSocketDisconnect, RuntimeError):
        return
