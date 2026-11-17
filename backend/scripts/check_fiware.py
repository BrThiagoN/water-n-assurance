"""Confere Orion real. Ingestao sintetica e limpeza do cache so com flags locais."""

import json
import os
import secrets
import sys
import time
from urllib.parse import quote, urlsplit

import httpx

from vigia.demo import sample
from vigia.models import CurrentResponse, FiwareContextResponse, State


def main() -> None:
    base = os.environ.get("FIWARE_CHECK_URL", "http://127.0.0.1:8000").rstrip("/")
    send_demo = "--send-demo" in sys.argv
    recovery = "--exercise-recovery" in sys.argv
    if (send_demo or recovery) and urlsplit(base).hostname not in ("127.0.0.1", "localhost"):
        raise SystemExit("Os ensaios que alteram dados estao limitados ao ambiente local.")
    path = "/api/v1/devices/vigia-sim-01"
    with httpx.Client(timeout=10) as client:
        deadline = time.monotonic() + 150
        while True:
            try:
                client.get(base + "/healthz").raise_for_status()
                break
            except httpx.HTTPError:
                if time.monotonic() >= deadline:
                    raise SystemExit("API nao ficou disponivel.") from None
                time.sleep(1)
        boot = "check-" + secrets.token_hex(4)

        def verify(expected_seq: int | None = None) -> CurrentResponse:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                response = client.get(base + path + "/context")
                if response.status_code == 200:
                    context = FiwareContextResponse.model_validate_json(response.content)
                    current = CurrentResponse.model_validate_json(
                        client.get(base + path + "/current").content
                    )
                    row = current.current
                    if (
                        context.matches_current
                        and current.source == "fiware-orion"
                        and row is not None
                        and (
                            expected_seq is None
                            or (row.seq == expected_seq and row.boot_id == boot)
                        )
                    ):
                        print(
                            json.dumps(
                                {
                                    "source": current.source,
                                    "entity": context.entity.id,
                                    "boot": row.boot_id,
                                    "seq": row.seq,
                                    "state": row.payload.assessment_state,
                                }
                            ),
                            flush=True,
                        )
                        return current
                time.sleep(1)
            raise SystemExit("Contexto FIWARE nao correspondeu ao snapshot persistido.")

        if send_demo:
            states: tuple[State, ...] = ("NO_CHANGE", "ATTENTION", "CHECK")
            for seq, state in enumerate(states, 1):
                body = sample(boot_id=boot, seq=seq, state=state)
                response = client.post(
                    base + "/api/v1/ingest",
                    headers={"Authorization": "Bearer " + os.environ["INGEST_TOKEN"]},
                    json=body.model_dump(mode="json", exclude_unset=True),
                )
                response.raise_for_status()
                verify(seq)
        else:
            verify()
        if recovery:
            before = verify()
            entity_id = "urn:ngsi-ld:WaterMonitor:vigia-sim-01"
            response = client.delete(
                "http://127.0.0.1:1026/v2/entities/" + quote(entity_id, safe=""),
                headers={"Fiware-Service": "waternassurance", "Fiware-ServicePath": "/"},
            )
            response.raise_for_status()
            after = verify()
            assert before.current and after.current
            assert before.current.id == after.current.id
            assert before.current.received_at == after.current.received_at
            print("Contexto reconstruido sem nova leitura nem alteracao do horario original.")


if __name__ == "__main__":
    main()
