"""Teste HTTP com dados sintéticos. Uso: python -m scripts.smoke [URL]."""

import secrets
import sys

import httpx

from vigia.config import Settings
from vigia.demo import event, sample
from vigia.models import Accepted, CurrentResponse


def main() -> None:
    settings = Settings()
    base = sys.argv[1] if len(sys.argv) > 1 else f"http://127.0.0.1:{settings.port}"
    boot = secrets.token_hex(8)
    headers = {"Authorization": f"Bearer {settings.ingest_token.get_secret_value()}"}
    with httpx.Client(base_url=base, timeout=90) as client:
        client.get("/healthz").raise_for_status()
        data = sample(boot).model_dump(mode="json", exclude_unset=True)
        denied = client.post("/api/v1/ingest", json=data)
        assert denied.status_code == 401, "Ingestão sem token deveria ser rejeitada."
        first = client.post("/api/v1/ingest", json=data, headers=headers)
        assert first.status_code == 201
        again = client.post("/api/v1/ingest", json=data, headers=headers)
        assert again.status_code == 200 and Accepted.model_validate(again.json()).duplicate
        client.post(
            "/api/v1/ingest", json=event(boot).model_dump(mode="json"), headers=headers
        ).raise_for_status()
        current = CurrentResponse.model_validate(
            client.get("/api/v1/devices/vigia-sim-01/current").json()
        )
        assert current.current and current.current.payload.assessment_state == "NO_CHANGE"
        assert client.get("/docs").status_code == 404
        assert client.get("/red").status_code == 404
        assert client.get("/").headers["content-type"].startswith("application/json")
        assert "NO_CHANGE" in client.get("/api/v1/devices/vigia-sim-01/export.csv").text
    print(f"PASS: saúde, autenticação, ingestão, duplicatas, evento, CSV e API sem UI. boot={boot}")


if __name__ == "__main__":
    main()
