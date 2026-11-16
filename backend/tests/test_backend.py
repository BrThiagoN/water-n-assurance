from concurrent.futures import ThreadPoolExecutor
from datetime import UTC, datetime, timedelta

import httpx
import pytest
from fastapi.testclient import TestClient
from pydantic import SecretStr, ValidationError
from sqlalchemy import update

from vigia.config import Settings
from vigia.database import EventRow, Store, TelemetryRow, json_object
from vigia.demo import event, sample
from vigia.integrations import FiwareWorker, MqttWorker
from vigia.models import (
    Accepted,
    Channel,
    CurrentResponse,
    FollowupRecord,
    Items,
    Telemetry,
    payload_adapter,
)

INGEST = {"Authorization": "Bearer " + "a" * 40}
ADMIN = {"Authorization": "Bearer " + "b" * 40}
DEVICE = "vigia-sim-01"


def test_contract_preserves_contextual_failure() -> None:
    original = sample()
    channels = original.channels.model_copy(
        update={
            "temperature": Channel(quality="INVALID", valid=False, unit="degC", source="simulator")
        }
    )
    partial = original.model_copy(update={"channels": channels, "readings_valid": False})
    parsed = Telemetry.model_validate_json(partial.model_dump_json(exclude_none=True))
    assert parsed.assessment_state == "NO_CHANGE"
    assert parsed.channels.temperature.value is None


@pytest.mark.parametrize(
    "field,value",
    [
        ("simulated", False),
        ("simulated", 1),
        ("seq", True),
        ("seq", "10"),
        ("seq", -1),
        ("schema_version", 1),
        ("assessment_state", "SAFE_TO_DRINK"),
        ("filtered_valid", "true"),
        ("temperature_c", float("inf")),
    ],
)
def test_rejects_invalid_types_and_states(field: str, value: object) -> None:
    body = sample().model_dump(mode="json")
    body[field] = value
    with pytest.raises(ValidationError):
        payload_adapter.validate_python(body)


def test_invalid_channel_cannot_publish_current_value() -> None:
    with pytest.raises(ValidationError):
        Channel(quality="INVALID", valid=False, unit="degC", source="simulator", value=25.0)


def test_bad_units_ranges_and_availability() -> None:
    original = sample()
    broken = original.model_copy(
        update={
            "channels": original.channels.model_copy(
                update={
                    "flow": Channel(
                        quality="VALID", valid=True, unit="bar", source="simulator", value=6.0
                    )
                }
            )
        }
    )
    with pytest.raises(ValidationError):
        Telemetry.model_validate_json(broken.model_dump_json())
    unavailable = original.model_copy(update={"assessment_available": False})
    with pytest.raises(ValidationError):
        Telemetry.model_validate_json(unavailable.model_dump_json())


def test_duplicates_do_not_refresh_time_or_duplicate_events(store: Store) -> None:
    assert store.ingest(sample()).current_updated
    first = store.current(DEVICE)
    assert store.ingest(sample()).duplicate
    assert len(store.telemetry(DEVICE, 100)) == 1
    assert store.current(DEVICE) == first
    store.ingest(event())
    assert store.ingest(event()).duplicate
    assert len(store.events(DEVICE, 100)) == 1
    assert store.current(DEVICE) == first


def test_legacy_event_is_preserved_without_inventing_a_filtered_value(store: Store) -> None:
    original = event()
    payload = json_object.validate_json(original.model_dump_json(exclude_unset=True))
    del payload["event_optical_index_filtered"]
    with store.sessions.begin() as session:
        session.add(
            EventRow(
                event_id=original.event_id,
                device_id=original.device_id,
                boot_id=original.boot_id,
                seq=original.event_seq,
                payload=payload,
            )
        )
    stored = store.events(DEVICE, 10)
    assert len(stored) == 1 and not stored[0].contract_valid
    assert stored[0].payload.event_optical_index_filtered is None
    with pytest.raises(ValidationError):
        payload_adapter.validate_python(payload)


def test_old_sample_and_closed_boot_do_not_regress_current(store: Store) -> None:
    store.ingest(sample())
    assert not store.ingest(sample(seq=9, state="CHECK")).current_updated
    store.ingest(sample("newboot", 1, "ATTENTION"))
    assert not store.ingest(sample(seq=1000, state="CHECK")).current_updated
    current = store.current(DEVICE)
    assert current and current.boot_id == "newboot"
    assert current.payload.assessment_state == "ATTENTION"
    restarted = Store(store.engine)
    restarted.initialize()
    assert restarted.current(DEVICE) == current


def test_concurrent_duplicate_is_committed_once(store: Store) -> None:
    with ThreadPoolExecutor(max_workers=4) as executor:
        results = list(executor.map(store.ingest, [sample()] * 8))
    assert sum(not result.duplicate for result in results) == 1
    assert len(store.telemetry(DEVICE, 100)) == 1


def test_fiware_outbox_retries_and_sends_full_snapshot(store: Store, settings: Settings) -> None:
    store.orion_enabled = True
    store.ingest(sample())
    configured = settings.model_copy(update={"orion_url": "http://orion"})
    with httpx.Client(transport=httpx.MockTransport(lambda request: httpx.Response(503))) as failed:
        worker = FiwareWorker(store, configured, failed)
        worker.flush()
        assert worker.status.last_error
        assert len(store.pending_snapshots()) == 1
    requests: list[httpx.Request] = []

    def respond(request: httpx.Request) -> httpx.Response:
        requests.append(request)
        return httpx.Response(404 if request.method == "PATCH" else 201)

    with httpx.Client(transport=httpx.MockTransport(respond)) as reachable:
        worker = FiwareWorker(store, configured, reachable)
        worker.flush()
        assert worker.status.last_success
    assert json_object.validate_json(requests[-1].content)["type"] == "WaterMonitor"
    assert store.pending_snapshots() == []
    current = store.current(DEVICE)
    assert current and current.fiware_synced_at


def test_mqtt_uses_same_contract_and_store(store: Store, settings: Settings) -> None:
    worker = MqttWorker(store, settings)
    body = sample().model_dump_json(exclude_unset=True).encode()
    worker.accept(f"water-n-assurance/v1/{DEVICE}/telemetry", body)
    assert store.current(DEVICE)
    with pytest.raises(ValueError, match="Tópico"):
        worker.accept("wrong/topic", body)


def test_http_auth_validation_current_history_and_csv(client: TestClient) -> None:
    body = sample().model_dump(mode="json", exclude_unset=True)
    assert client.post("/api/v1/ingest", json=body).status_code == 401
    assert client.post("/api/v1/ingest", json=body, headers=ADMIN).status_code == 401
    assert client.post("/api/v1/ingest", json=body, headers=INGEST).status_code == 201
    again = client.post("/api/v1/ingest", json=body, headers=INGEST)
    assert again.status_code == 200 and Accepted.model_validate(again.json()).duplicate
    assert client.get("/healthz").json()["backend"] == "python"
    current = CurrentResponse.model_validate(client.get(f"/api/v1/devices/{DEVICE}/current").json())
    assert current.current and current.current.payload.assessment_state == "NO_CHANGE"
    assert not current.communication.stale
    rows = Items[Telemetry].model_validate({"items": [current.current.payload.model_dump()]})
    assert len(rows.items) == 1
    assert client.get(f"/api/v1/devices/{DEVICE}/telemetry?limit=500").status_code == 400
    assert client.get("/api/v1/devices/unknown/current").status_code == 404
    assert "NO_CHANGE" in client.get(f"/api/v1/devices/{DEVICE}/export.csv").text
    assert (
        client.post("/api/v1/ingest", json={**body, "simulated": False}, headers=INGEST).status_code
        == 400
    )
    assert (
        client.post(
            "/api/v1/ingest", content="{bad", headers={**INGEST, "Content-Type": "application/json"}
        ).status_code
        == 400
    )
    assert client.post("/api/v1/ingest", content="x" * 33_000, headers=INGEST).status_code == 413


def test_stale_communication_does_not_change_edge_decision(
    client: TestClient, store: Store
) -> None:
    store.ingest(sample())
    with store.sessions.begin() as session:
        session.execute(
            update(TelemetryRow).values(received_at=datetime.now(UTC) - timedelta(minutes=1))
        )
    current = CurrentResponse.model_validate(client.get(f"/api/v1/devices/{DEVICE}/current").json())
    assert current.communication.stale
    assert current.current and current.current.payload.assessment_state == "NO_CHANGE"


def test_api_has_no_html_or_editor(client: TestClient) -> None:
    assert client.get("/").headers["content-type"].startswith("application/json")
    assert client.get("/openapi.json").json()["info"]["version"] == "0.2.0"
    for path in ("/docs", "/redoc", "/red", "/admin", "/index.html", "/style.css", "/dashboard.js"):
        assert client.get(path).status_code == 404


def test_followup_crud_keeps_source_event(client: TestClient) -> None:
    client.post("/api/v1/ingest", json=event().model_dump(mode="json"), headers=INGEST)
    body = {"event_id": event().event_id, "note": "Solicitar verificação", "status": "OPEN"}
    assert client.post("/api/v1/followups", json=body, headers=INGEST).status_code == 401
    assert (
        client.post("/api/v1/followups", json={**body, "note": " "}, headers=ADMIN).status_code
        == 400
    )
    created = client.post("/api/v1/followups", json=body, headers=ADMIN)
    assert created.status_code == 201
    item = FollowupRecord.model_validate(created.json())
    changed = client.patch(
        f"/api/v1/followups/{item.id}",
        json={"note": "Verificação registrada", "status": "CLOSED"},
        headers=ADMIN,
    )
    assert FollowupRecord.model_validate(changed.json()).status == "CLOSED"
    assert (
        len(
            Items[FollowupRecord]
            .model_validate(client.get("/api/v1/followups", headers=ADMIN).json())
            .items
        )
        == 1
    )
    assert client.delete(f"/api/v1/followups/{item.id}", headers=ADMIN).status_code == 204
    assert len(client.get(f"/api/v1/devices/{DEVICE}/events").json()["items"]) == 1


def test_cors_only_allows_configured_dashboard(client: TestClient) -> None:
    allowed = client.options(
        "/api/v1/status",
        headers={"Origin": "http://localhost:5173", "Access-Control-Request-Method": "GET"},
    )
    assert allowed.headers["access-control-allow-origin"] == "http://localhost:5173"
    denied = client.options(
        "/api/v1/status",
        headers={"Origin": "https://unknown.example", "Access-Control-Request-Method": "GET"},
    )
    assert "access-control-allow-origin" not in denied.headers


def test_tokens_must_be_distinct_and_not_placeholders(settings: Settings) -> None:
    with pytest.raises(ValidationError):
        Settings(
            _env_file=None,
            database_url=settings.database_url,
            ingest_token=SecretStr("a" * 40),
            admin_token=SecretStr("a" * 40),
        )
    with pytest.raises(ValidationError):
        Settings(
            _env_file=None,
            database_url=settings.database_url,
            ingest_token=SecretStr("replace-with-at-least-32-random-characters"),
            admin_token=SecretStr("b" * 40),
        )
