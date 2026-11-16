"""Integrações opcionais. Sem configuração, seu estado fica explicitamente desativado."""

import asyncio
import logging
import ssl
from datetime import UTC, datetime
from urllib.parse import quote, urlsplit

import aiomqtt
import httpx
from pydantic import JsonValue, ValidationError

from vigia.config import Settings
from vigia.database import Store
from vigia.models import IntegrationState, payload_adapter

logger = logging.getLogger(__name__)


class FiwareWorker:
    def __init__(self, store: Store, settings: Settings, client: httpx.Client) -> None:
        self.store, self.settings, self.client = store, settings, client
        self.status = IntegrationState(configured=bool(settings.orion_url))

    def flush(self) -> None:
        if not self.status.configured:
            return
        try:
            for pending in self.store.pending_snapshots():
                row = pending.record
                entity_id = f"urn:ngsi-ld:WaterMonitor:{row.device_id}"
                attrs: dict[str, JsonValue] = {
                    "deviceId": {"type": "Text", "value": row.device_id},
                    "assessmentState": {"type": "Text", "value": row.payload.assessment_state},
                    "assessmentAvailable": {
                        "type": "Boolean",
                        "value": row.payload.assessment_available,
                    },
                    "receivedAt": {"type": "DateTime", "value": row.received_at.isoformat()},
                    "lastSnapshot": {
                        "type": "StructuredValue",
                        "value": row.payload.model_dump(mode="json", exclude_unset=True),
                    },
                }
                headers = {
                    "Fiware-Service": self.settings.fiware_service,
                    "Fiware-ServicePath": "/",
                }
                base = self.settings.orion_url
                response = self.client.patch(
                    f"{base}/v2/entities/{quote(entity_id, safe='')}/attrs",
                    json=attrs,
                    headers=headers,
                )
                if response.status_code == 404:
                    response = self.client.post(
                        f"{base}/v2/entities",
                        headers=headers,
                        json={"id": entity_id, "type": "WaterMonitor", **attrs},
                    )
                    # Outra instância pode ter criado a entidade entre PATCH e POST.
                    if response.status_code == 422:
                        response = self.client.patch(
                            f"{base}/v2/entities/{quote(entity_id, safe='')}/attrs",
                            json=attrs,
                            headers=headers,
                        )
                response.raise_for_status()
                self.store.mark_synced(pending)
                self.status.connected = True
                self.status.last_success = datetime.now(UTC)
                self.status.last_error = None
        except (httpx.HTTPError, OSError):
            self.status.connected = False
            self.status.last_error = "Sincronização FIWARE pendente; será tentada novamente."

    async def run(self) -> None:
        while True:
            try:
                await asyncio.to_thread(self.flush)
            except Exception:
                logger.error("Falha ao processar a outbox FIWARE; nova tentativa agendada.")
            await asyncio.sleep(10)


class MqttWorker:
    def __init__(self, store: Store, settings: Settings) -> None:
        self.store, self.settings = store, settings
        self.status = IntegrationState(configured=bool(settings.mqtt_url))

    def accept(self, topic: str, payload: bytes) -> None:
        if len(payload) > 32_768:
            raise ValueError("Mensagem MQTT excede 32 KiB.")
        message = payload_adapter.validate_json(payload)
        suffix = "events" if message.message_kind == "event" else "telemetry"
        if message.device_id not in self.settings.devices:
            raise ValueError("Dispositivo não autorizado.")
        if topic != f"water-n-assurance/v1/{message.device_id}/{suffix}":
            raise ValueError("Tópico incoerente com a mensagem.")
        self.store.ingest(message)
        self.status.last_success = datetime.now(UTC)
        self.status.last_error = None

    async def run(self) -> None:
        parsed = urlsplit(self.settings.mqtt_url)
        if parsed.scheme not in ("mqtt", "mqtts") or not parsed.hostname:
            raise ValueError("MQTT_URL deve usar mqtt:// ou mqtts:// com um hostname.")
        tls = ssl.create_default_context() if parsed.scheme == "mqtts" else None
        while True:
            try:
                async with aiomqtt.Client(
                    hostname=parsed.hostname,
                    port=parsed.port or (8883 if tls else 1883),
                    username=self.settings.mqtt_username or None,
                    password=self.settings.mqtt_password.get_secret_value() or None,
                    tls_context=tls,
                ) as client:
                    for device in self.settings.devices:
                        for suffix in ("telemetry", "events"):
                            await client.subscribe(f"water-n-assurance/v1/{device}/{suffix}", qos=1)
                    self.status.connected = True
                    self.status.last_error = None
                    async for message in client.messages:
                        try:
                            if not isinstance(message.payload, bytes):
                                raise ValueError("O contrato MQTT exige bytes UTF-8.")
                            await asyncio.to_thread(
                                self.accept, str(message.topic), message.payload
                            )
                        except (ValidationError, ValueError):
                            self.status.last_error = "Mensagem MQTT rejeitada pelo contrato."
                        except Exception:
                            self.status.last_error = "Falha ao persistir mensagem MQTT."
            except (aiomqtt.MqttError, OSError):
                self.status.last_error = "MQTT indisponível; nova tentativa em 10 segundos."
            finally:
                self.status.connected = False
            await asyncio.sleep(10)
