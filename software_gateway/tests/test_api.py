from fastapi.testclient import TestClient

from software_gateway.app.main import app

client = TestClient(app)


def test_health_endpoint_marks_data_as_simulated():
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json()["evidence_label"] == "SIMULATED_NOT_MEASURED"


def test_dashboard_and_links_endpoints():
    dashboard = client.get("/api/dashboard")
    assert dashboard.status_code == 200
    payload = dashboard.json()
    assert payload["mode"] == "SIMULATION"
    assert len(payload["links"]) == 3
    assert all(link["id"].startswith("wan") for link in payload["links"])

    links = client.get("/api/links")
    assert links.status_code == 200
    assert len(links.json()["links"]) == 3


def test_fault_injection_is_simulation_only_and_down_link_weight_zero():
    response = client.post(
        "/api/simulation/fault",
        json={"link_id": "wan2", "faulted": True},
    )
    assert response.status_code == 200
    dashboard = response.json()["dashboard"]
    wan2 = next(link for link in dashboard["links"] if link["id"] == "wan2")
    assert wan2["state"] == "DOWN"
    assert wan2["recommended_weight_pct"] == 0
    assert all(row["source"] == "SIMULATED_NOT_MEASURED" for row in dashboard["history"])
    client.post("/api/simulation/fault", json={"link_id": "wan2", "faulted": False})


def test_unknown_link_is_rejected():
    response = client.post(
        "/api/simulation/fault",
        json={"link_id": "wan9", "faulted": True},
    )
    assert response.status_code == 422


def test_static_dashboard_is_served():
    response = client.get("/")
    assert response.status_code == 200
    assert "SIMULATION MODE" in response.text
