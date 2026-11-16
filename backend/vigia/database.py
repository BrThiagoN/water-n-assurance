"""Postgres com mapeamento tipado e transações compartilhadas por HTTP/MQTT."""

from dataclasses import dataclass
from datetime import UTC, datetime
from uuid import uuid4

from pydantic import JsonValue, TypeAdapter, ValidationError
from sqlalchemy import (
    BigInteger,
    CheckConstraint,
    DateTime,
    ForeignKey,
    Index,
    Text,
    UniqueConstraint,
    create_engine,
    delete,
    func,
    select,
    text,
    update,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.engine import Engine
from sqlalchemy.orm import DeclarativeBase, Mapped, Session, mapped_column, sessionmaker

from vigia.config import Settings
from vigia.models import (
    Accepted,
    Event,
    EventRecord,
    FollowupCreate,
    FollowupRecord,
    FollowupStatus,
    FollowupWrite,
    Payload,
    StoredEvent,
    Telemetry,
    TelemetryRecord,
)

json_object: TypeAdapter[dict[str, JsonValue]] = TypeAdapter(dict[str, JsonValue])


class MissingRecord(Exception):
    """Identificador não encontrado; convertido em HTTP 404 pela camada da API."""


def valid_event_contract(payload: dict[str, JsonValue]) -> bool:
    try:
        Event.model_validate(payload)
        return True
    except ValidationError:
        return False


class Base(DeclarativeBase):
    pass


class TelemetryRow(Base):
    __tablename__ = "telemetry"
    __table_args__ = (
        UniqueConstraint("device_id", "boot_id", "seq"),
        Index("telemetry_device_id_idx", "device_id", "id"),
    )
    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(Text)
    boot_id: Mapped[str] = mapped_column(Text)
    seq: Mapped[int] = mapped_column(BigInteger)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )
    payload: Mapped[dict[str, JsonValue]] = mapped_column(JSONB)


class EventRow(Base):
    __tablename__ = "events"
    __table_args__ = (Index("events_device_time_idx", "device_id", "received_at"),)
    event_id: Mapped[str] = mapped_column(Text, primary_key=True)
    device_id: Mapped[str] = mapped_column(Text)
    boot_id: Mapped[str] = mapped_column(Text)
    seq: Mapped[int] = mapped_column(BigInteger)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )
    payload: Mapped[dict[str, JsonValue]] = mapped_column(JSONB)


class DeviceRow(Base):
    __tablename__ = "devices"
    device_id: Mapped[str] = mapped_column(Text, primary_key=True)
    boot_id: Mapped[str] = mapped_column(Text)
    seq: Mapped[int] = mapped_column(BigInteger)
    telemetry_id: Mapped[int] = mapped_column(ForeignKey("telemetry.id"))
    fiware_synced_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))


class OutboxRow(Base):
    __tablename__ = "fiware_outbox"
    device_id: Mapped[str] = mapped_column(Text, primary_key=True)
    telemetry_id: Mapped[int] = mapped_column(ForeignKey("telemetry.id"))


class FollowupRow(Base):
    __tablename__ = "followups"
    __table_args__ = (CheckConstraint("status IN ('OPEN','CLOSED')"),)
    id: Mapped[str] = mapped_column(Text, primary_key=True)
    event_id: Mapped[str] = mapped_column(ForeignKey("events.event_id"))
    note: Mapped[str] = mapped_column(Text)
    status: Mapped[FollowupStatus] = mapped_column(Text)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


def build_engine(settings: Settings) -> Engine:
    raw = settings.database_url.get_secret_value()
    url = raw.replace("postgres://", "postgresql://", 1).replace(
        "postgresql://", "postgresql+psycopg://", 1
    )
    return create_engine(
        url,
        pool_size=5,
        max_overflow=0,
        pool_pre_ping=True,
        connect_args={
            "connect_timeout": 10,
            "sslmode": "verify-full" if settings.database_ssl == "require" else "disable",
        },
    )


def telemetry_record(row: TelemetryRow, synced_at: datetime | None = None) -> TelemetryRecord:
    return TelemetryRecord(
        id=row.id,
        device_id=row.device_id,
        boot_id=row.boot_id,
        seq=row.seq,
        received_at=row.received_at,
        payload=Telemetry.model_validate(row.payload),
        fiware_synced_at=synced_at,
    )


@dataclass(frozen=True)
class PendingSnapshot:
    telemetry_id: int
    record: TelemetryRecord


class Store:
    def __init__(self, engine: Engine, *, orion_enabled: bool = False) -> None:
        self.engine = engine
        self.sessions = sessionmaker(engine, expire_on_commit=False)
        self.orion_enabled = orion_enabled

    def initialize(self) -> None:
        # As mesmas tabelas/colunas da versão anterior: não apaga leituras existentes.
        Base.metadata.create_all(self.engine)

    def healthy(self) -> bool:
        with self.engine.connect() as connection:
            result: object = connection.scalar(text("SELECT 1"))
            return result == 1

    def ingest(self, message: Payload) -> Accepted:
        with self.sessions.begin() as session:
            # Também serializa concorrência entre processos/instâncias, não só threads.
            session.execute(
                text("SELECT pg_advisory_xact_lock(hashtext(:device))"),
                {"device": message.device_id},
            )
            if isinstance(message, Event):
                return self._event(session, message)
            return self._telemetry(session, message)

    @staticmethod
    def _event(session: Session, message: Event) -> Accepted:
        existing = session.get(EventRow, message.event_id)
        if existing is not None:
            return Accepted(duplicate=True, current_updated=False, message_kind="event")
        row = EventRow(
            event_id=message.event_id,
            device_id=message.device_id,
            boot_id=message.boot_id,
            seq=message.event_seq,
            payload=json_object.validate_json(message.model_dump_json(exclude_unset=True)),
        )
        session.add(row)
        session.flush()
        return Accepted(
            duplicate=False,
            current_updated=False,
            message_kind="event",
            received_at=row.received_at,
        )

    def _telemetry(self, session: Session, message: Telemetry) -> Accepted:
        existing = session.scalar(
            select(TelemetryRow.id).where(
                TelemetryRow.device_id == message.device_id,
                TelemetryRow.boot_id == message.boot_id,
                TelemetryRow.seq == message.seq,
            )
        )
        if existing is not None:
            return Accepted(duplicate=True, current_updated=False, message_kind="telemetry")
        known_boot = (
            session.scalar(
                select(TelemetryRow.id)
                .where(
                    TelemetryRow.device_id == message.device_id,
                    TelemetryRow.boot_id == message.boot_id,
                )
                .limit(1)
            )
            is not None
        )
        current = session.get(DeviceRow, message.device_id)
        fresh = (
            current is None
            or (current.boot_id == message.boot_id and message.seq > current.seq)
            or (current.boot_id != message.boot_id and not known_boot)
        )
        row = TelemetryRow(
            device_id=message.device_id,
            boot_id=message.boot_id,
            seq=message.seq,
            payload=json_object.validate_json(message.model_dump_json(exclude_unset=True)),
        )
        session.add(row)
        session.flush()
        if fresh:
            if current is None:
                current = DeviceRow(device_id=message.device_id)
                session.add(current)
            current.boot_id = message.boot_id
            current.seq = message.seq
            current.telemetry_id = row.id
            current.fiware_synced_at = None
            if self.orion_enabled:
                pending = session.get(OutboxRow, message.device_id)
                if pending is None:
                    pending = OutboxRow(device_id=message.device_id)
                    session.add(pending)
                pending.telemetry_id = row.id
        return Accepted(
            duplicate=False,
            current_updated=fresh,
            message_kind="telemetry",
            received_at=row.received_at,
        )

    def current(self, device_id: str) -> TelemetryRecord | None:
        with self.sessions() as session:
            device = session.get(DeviceRow, device_id)
            if device is None:
                return None
            row = session.get(TelemetryRow, device.telemetry_id)
            return telemetry_record(row, device.fiware_synced_at) if row is not None else None

    def telemetry(self, device_id: str, limit: int) -> list[TelemetryRecord]:
        with self.sessions() as session:
            rows = session.scalars(
                select(TelemetryRow)
                .where(TelemetryRow.device_id == device_id)
                .order_by(TelemetryRow.received_at.desc(), TelemetryRow.id.desc())
                .limit(limit)
            )
            return [telemetry_record(row) for row in rows]

    def events(self, device_id: str, limit: int) -> list[EventRecord]:
        with self.sessions() as session:
            rows = session.scalars(
                select(EventRow)
                .where(EventRow.device_id == device_id)
                .order_by(EventRow.received_at.desc(), EventRow.seq.desc())
                .limit(limit)
            )
            return [
                EventRecord(
                    event_id=row.event_id,
                    device_id=row.device_id,
                    boot_id=row.boot_id,
                    seq=row.seq,
                    received_at=row.received_at,
                    payload=StoredEvent.model_validate(row.payload),
                    contract_valid=valid_event_contract(row.payload),
                )
                for row in rows
            ]

    def followups(self) -> list[FollowupRecord]:
        with self.sessions() as session:
            rows = session.scalars(
                select(FollowupRow).order_by(FollowupRow.created_at.desc()).limit(200)
            )
            return [FollowupRecord.model_validate(row, from_attributes=True) for row in rows]

    def create_followup(self, body: FollowupCreate) -> FollowupRecord:
        with self.sessions.begin() as session:
            if session.get(EventRow, body.event_id) is None:
                raise MissingRecord("Evento não encontrado.")
            row = FollowupRow(
                id=str(uuid4()), event_id=body.event_id, note=body.note, status=body.status
            )
            session.add(row)
            session.flush()
            return FollowupRecord.model_validate(row, from_attributes=True)

    def update_followup(self, record_id: str, body: FollowupWrite) -> FollowupRecord:
        with self.sessions.begin() as session:
            row = session.get(FollowupRow, record_id)
            if row is None:
                raise MissingRecord("Acompanhamento não encontrado.")
            row.note, row.status, row.updated_at = body.note, body.status, datetime.now(UTC)
            session.flush()
            return FollowupRecord.model_validate(row, from_attributes=True)

    def delete_followup(self, record_id: str) -> None:
        with self.sessions.begin() as session:
            row = session.get(FollowupRow, record_id)
            if row is None:
                raise MissingRecord("Acompanhamento não encontrado.")
            session.delete(row)

    def pending_snapshots(self) -> list[PendingSnapshot]:
        with self.sessions() as session:
            pending = session.scalars(select(OutboxRow).limit(10))
            snapshots: list[PendingSnapshot] = []
            for item in pending:
                row = session.get(TelemetryRow, item.telemetry_id)
                if row is not None:
                    snapshots.append(PendingSnapshot(row.id, telemetry_record(row)))
            return snapshots

    def mark_synced(self, pending: PendingSnapshot) -> None:
        with self.sessions.begin() as session:
            device_id = pending.record.device_id
            session.execute(
                delete(OutboxRow).where(
                    OutboxRow.device_id == device_id, OutboxRow.telemetry_id == pending.telemetry_id
                )
            )
            session.execute(
                update(DeviceRow)
                .where(
                    DeviceRow.device_id == device_id, DeviceRow.telemetry_id == pending.telemetry_id
                )
                .values(fiware_synced_at=datetime.now(UTC))
            )
