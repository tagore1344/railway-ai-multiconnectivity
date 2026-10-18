from software_gateway.app.controller import quality_score, recommend_weights


def test_quality_score_is_bounded():
    assert 0 <= quality_score(10, 0, -55) <= 100
    assert 0 <= quality_score(999, 100, -140) <= 100


def test_down_link_receives_no_recommended_traffic():
    links = [
        {"id": "wan1", "state": "ACTIVE", "quality_score": 80},
        {"id": "wan2", "state": "DOWN", "quality_score": 100},
        {"id": "wan3", "state": "ACTIVE", "quality_score": 60},
    ]
    result = recommend_weights(links)
    by_id = {link["id"]: link for link in result}
    assert by_id["wan2"]["recommended_weight_pct"] == 0
    assert sum(link["recommended_weight_pct"] for link in result) == 100


def test_all_down_means_zero_allocation():
    links = [
        {"id": "wan1", "state": "DOWN", "quality_score": 80},
        {"id": "wan2", "state": "DOWN", "quality_score": 50},
    ]
    assert [x["recommended_weight_pct"] for x in recommend_weights(links)] == [0, 0]
