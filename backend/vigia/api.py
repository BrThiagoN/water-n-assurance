"""API HTTP sem arquivos estáticos nem interface de usuário."""

import asyncio
import csv
import hmac
import io
import logging
import time
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager, suppress
from datetime import UTC, datetime
from typing import Annotated, Literal

import httpx
from fastapi import Depends, FastAPI, HTTPException, Query, Request, Response
from fastapi.exceptions import RequestValidationError
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy.exc import SQLAlchemyError
from starlette.middleware.base import RequestResponseEndpoint
from starlette.responses import JSONResponse
from starlette.types import ASGIApp, Message, Receive, Scope, Send

from vigia.config import Settings
from vigia.database import MissingRecord, Store, build_engine
from vigia.integrations import FiwareWorker, MqttWorker
from vigia.models import (
    Accepted,
    Communication,
    CurrentResponse,
    EventRecord,
    FiwareContextResponse,
    FollowupCreate,
    FollowupRecord,
    FollowupWrite,
    Integrations,
    Items,
    Payload,
    StatusResponse,
    TelemetryRecord,
)

logger = logging.getLogger(__name__)
bearer = HTTPBearer(auto_error=False)
type Credential = Annotated[HTTPAuthorizationCredentials | None, Depends(bearer)]
type Limit = Annotated[int, Query(ge=1, le=200)]


class BodyLimitMiddleware:
    def __init__(self, app: ASGIApp) -> None:
        self.app = app

    async def __call__(self, scope: Scope, receive: Receive, send: Send) -> None:
        if scope["type"] != "http":
            await self.app(scope, receive, send)
            return
        chunks: list[bytes] = []
        size = 0
        while True:
            message = await receive()
            if message["type"] == "http.disconnect":
                return
            chunk = message.get("body", b"")
            size += len(chunk)
            if size > 32_768:
                await JSONResponse({"error": "Mensagem excede 32 KiB."}, 413)(scope, receive, send)
                return
            chunks.append(chunk)
            if not message.get("more_body", False):
                break
        sent = False

        async def replay() -> Message:
            nonlocal sent
            if not sent:
                sent = True
                return {"type": "http.request", "body": b"".join(chunks), "more_body": False}
            return await receive()

        await self.app(scope, replay, send)


def create_app(
    settings: Settings, store: Store | None = None, integration_client: httpx.Client | None = None
) -> FastAPI:
    repository = store or Store(build_engine(settings), orion_enabled=bool(settings.orion_url))
    http_client = integration_client or httpx.Client(timeout=8)
    fiware = FiwareWorker(repository, settings, http_client)
    mqtt = MqttWorker(repository, settings)

    @asynccontextmanager
    async def lifespan(app: FastAPI) -> AsyncIterator[None]:
        await asyncio.to_thread(repository.initialize)
        tasks: list[asyncio.Task[None]] = []
        if fiware.status.configured:
            tasks.append(asyncio.create_task(fiware.run()))
        if mqtt.status.configured:
            tasks.append(asyncio.create_task(mqtt.run()))
        try:
            yield
        finally:
            for task in tasks:
                task.cancel()
            for task in tasks:
                with suppress(asyncio.CancelledError):
                    await task
            http_client.close()
            repository.engine.dispose()

    app = FastAPI(
        title="water n' assurance — API do Vigia d’Água",
        version="0.2.0",
        description="Backend de demonstração. Estados locais não comprovam potabilidade.",
        lifespan=lifespan,
        docs_url=None,
        redoc_url=None,
        openapi_url="/openapi.json",
    )
    app.add_middleware(BodyLimitMiddleware)
    app.add_middleware(
        CORSMiddleware,
        allow_origins=settings.origins,
        allow_methods=["GET", "POST", "PATCH", "DELETE"],
        allow_headers=["Authorization", "Content-Type"],
    )
    hits: dict[str, tuple[float, int]] = {}

    @app.middleware("http")
    async def security_headers(request: Request, call_next: RequestResponseEndpoint) -> Response:
        if request.url.path.startswith("/api/"):
            now = time.monotonic()
            client_id = request.client.host if request.client else "unknown"
            previous, count = hits.get(client_id, (now, 0))
            if now - previous >= 60:
                previous, count = now, 0
            if len(hits) > 2000:
                hits.clear()
            hits[client_id] = previous, count + 1
            if count >= 240:
                return JSONResponse(
                    {"error": "Limite de requisições; tente novamente em um minuto."},
                    429,
                    headers={"Retry-After": "60"},
                )
        response = await call_next(request)
        response.headers.update(
            {
                "X-Content-Type-Options": "nosniff",
                "Referrer-Policy": "no-referrer",
                "Cache-Control": "no-store",
                "Content-Security-Policy": "default-src 'none'; frame-ancestors 'none'",
            }
        )
        return response

    def check_token(credential: HTTPAuthorizationCredentials | None, expected: str) -> None:
        actual = credential.credentials if credential else ""
        if not hmac.compare_digest(actual.encode(), expected.encode()):
            raise HTTPException(401, "Credencial inválida.", headers={"WWW-Authenticate": "Bearer"})

    def ingest_auth(credential: Credential) -> None:
        check_token(credential, settings.ingest_token.get_secret_value())

    def admin_auth(credential: Credential) -> None:
        check_token(credential, settings.admin_token.get_secret_value())

    def require_device(device: str) -> None:
        if device not in settings.devices:
            raise HTTPException(404, "Dispositivo não encontrado.")

    @app.exception_handler(RequestValidationError)
    async def invalid_request(request: Request, exc: RequestValidationError) -> JSONResponse:
        return JSONResponse(
            {"error": "Dados inválidos. Confira os campos e tipos do contrato."}, 400
        )

    @app.exception_handler(MissingRecord)
    async def missing_record(request: Request, exc: MissingRecord) -> JSONResponse:
        return JSONResponse({"error": str(exc)}, 404)

    @app.exception_handler(SQLAlchemyError)
    async def unavailable_database(request: Request, exc: SQLAlchemyError) -> JSONResponse:
        logger.error("Operação não confirmada pelo banco: %s", type(exc).__name__)
        return JSONResponse(
            {"error": "Não foi possível confirmar a operação; tente novamente."}, 503
        )

    @app.get("/")
    def service() -> dict[str, str]:
        return {
            "service": "water-n-assurance",
            "product": "Vigia d’Água",
            "backend": "python",
            "health": "/healthz",
            "schema": "/openapi.json",
        }

    @app.get("/healthz")
    def health(response: Response) -> dict[str, str]:
        try:
            if repository.healthy():
                return {"status": "ok", "backend": "python", "framework": "fastapi"}
        except SQLAlchemyError:
            pass
        response.status_code = 503
        return {"status": "unavailable", "backend": "python"}

    @app.get("/api/v1/status")
    def status() -> StatusResponse:
        return StatusResponse(
            devices=settings.devices,
            server_time=datetime.now(UTC),
            integrations=Integrations(mqtt=mqtt.status, fiware=fiware.status),
        )

    @app.post("/api/v1/ingest", status_code=201, dependencies=[Depends(ingest_auth)])
    def ingest(message: Payload, response: Response) -> Accepted:
        if message.device_id not in settings.devices:
            raise HTTPException(400, "Dispositivo não autorizado.")
        result = repository.ingest(message)
        response.status_code = 200 if result.duplicate else 201
        return result

    @app.get("/api/v1/devices/{device}/context", dependencies=[Depends(require_device)])
    def context(device: str) -> FiwareContextResponse:
        if not fiware.status.configured:
            raise HTTPException(503, "FIWARE nao configurado.")
        try:
            entity = fiware.context(device)
        except httpx.HTTPStatusError as error:
            if error.response.status_code == 404:
                raise HTTPException(404, "Contexto FIWARE ainda nao sincronizado.") from None
            raise HTTPException(503, "FIWARE temporariamente indisponivel.") from None
        except (httpx.HTTPError, ValueError):
            raise HTTPException(503, "Resposta FIWARE indisponivel ou invalida.") from None
        row = repository.current(device)
        return FiwareContextResponse(
            device_id=device,
            entity=entity,
            matches_current=row is not None and fiware.matches(entity, row),
        )

    @app.get("/api/v1/devices/{device}/current", dependencies=[Depends(require_device)])
    def current(device: str) -> CurrentResponse:
        row = repository.current(device)
        age = (
            max(0, int((datetime.now(UTC) - row.received_at).total_seconds() * 1000))
            if row
            else None
        )
        source: Literal["postgres", "fiware-orion"] = "postgres"
        if row is not None and fiware.status.configured:
            try:
                entity = fiware.context(device)
                if fiware.matches(entity, row):
                    row = row.model_copy(update={"payload": entity.lastSnapshot.value})
                    source = "fiware-orion"
            except (httpx.HTTPError, ValueError):
                fiware.status.connected = False
                fiware.status.last_error = (
                    "Consulta FIWARE indisponivel; usando historico persistido."
                )
        return CurrentResponse(
            source=source,
            device_id=device,
            current=row,
            communication=Communication(
                stale=age is None or age > settings.stale_after_seconds * 1000,
                age_ms=age,
                threshold_seconds=settings.stale_after_seconds,
            ),
        )

    @app.get("/api/v1/devices/{device}/telemetry", dependencies=[Depends(require_device)])
    def telemetry(device: str, limit: Limit = 100) -> Items[TelemetryRecord]:
        return Items(items=repository.telemetry(device, limit))

    @app.get("/api/v1/devices/{device}/events", dependencies=[Depends(require_device)])
    def events(device: str, limit: Limit = 100) -> Items[EventRecord]:
        return Items(items=repository.events(device, limit))

    @app.get("/api/v1/devices/{device}/export.csv", dependencies=[Depends(require_device)])
    def export(device: str) -> Response:
        stream = io.StringIO(newline="")
        writer = csv.writer(stream)
        writer.writerow(
            ["received_at", "device_id", "boot_id", "seq", "assessment_state", "simulated"]
        )
        for row in repository.telemetry(device, 200):
            writer.writerow(
                [
                    row.received_at.isoformat(),
                    row.device_id,
                    row.boot_id,
                    row.seq,
                    row.payload.assessment_state,
                    row.payload.simulated,
                ]
            )
        return Response(
            stream.getvalue(),
            media_type="text/csv",
            headers={"Content-Disposition": 'attachment; filename="vigia-dagua-leituras.csv"'},
        )

    @app.get("/api/v1/followups", dependencies=[Depends(admin_auth)])
    def followups() -> Items[FollowupRecord]:
        return Items(items=repository.followups())

    @app.post("/api/v1/followups", status_code=201, dependencies=[Depends(admin_auth)])
    def create_followup(body: FollowupCreate) -> FollowupRecord:
        return repository.create_followup(body)

    @app.patch("/api/v1/followups/{record_id}", dependencies=[Depends(admin_auth)])
    def update_followup(record_id: str, body: FollowupWrite) -> FollowupRecord:
        return repository.update_followup(record_id, body)

    @app.delete(
        "/api/v1/followups/{record_id}", status_code=204, dependencies=[Depends(admin_auth)]
    )
    def delete_followup(record_id: str) -> Response:
        repository.delete_followup(record_id)
        return Response(status_code=204)

    return app
