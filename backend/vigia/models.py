"""Contrato schema_version=2, compatível com os encoders do firmware Wokwi."""

from datetime import datetime
from typing import Annotated, Literal, Self

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator

type UInt32 = Annotated[int, Field(strict=True, ge=0, le=4_294_967_295)]
type DeviceId = Annotated[str, Field(pattern=r"^[A-Za-z0-9_-]{1,40}$")]
type BootId = Annotated[str, Field(pattern=r"^[A-Za-z0-9_-]{1,16}$")]
type State = Literal["NO_CHANGE", "ATTENTION", "CHECK", "UNAVAILABLE"]
type Quality = Literal["NOT_READY", "VALID", "INVALID", "OUT_OF_RANGE", "STALE"]
type FlowContext = Literal["UNKNOWN", "FLOWING", "STOPPED", "RENEWING"]
type Observation = Literal["NONE", "CHANGE", "ELEVATED"]
type AlertState = Literal["UNKNOWN", "NONE", "NO_CHANGE", "ATTENTION", "CHECK"]
type FollowupStatus = Literal["OPEN", "CLOSED"]


class StrictModel(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True, allow_inf_nan=False)


class Channel(StrictModel):
    quality: Quality
    valid: bool
    unit: Literal["degC", "relative_index", "L/min", "bar"]
    source: Literal["simulator", "override"]
    value: float | None = None
    age_ms: UInt32 | None = None

    @model_validator(mode="after")
    def coherent_value(self) -> Self:
        if self.valid != (self.quality == "VALID"):
            raise ValueError("A validade do canal deve corresponder à qualidade.")
        if self.valid and self.value is None:
            raise ValueError("Um canal válido precisa de um valor atual.")
        if not self.valid and "value" in self.model_fields_set:
            raise ValueError("Um canal inválido deve omitir o valor atual.")
        return self


class Channels(StrictModel):
    temperature: Channel
    optical: Channel
    flow: Channel
    pressure: Channel

    @model_validator(mode="after")
    def units_and_ranges(self) -> Self:
        definitions = (
            (self.temperature, "degC", -55, 125),
            (self.optical, "relative_index", 0, 100),
            (self.flow, "L/min", 0, 20),
            (self.pressure, "bar", 0, 6),
        )
        for channel, unit, low, high in definitions:
            if channel.unit != unit:
                raise ValueError("Unidade incompatível com o canal.")
            if channel.value is not None and not low <= channel.value <= high:
                raise ValueError("Valor fora da faixa do canal simulado.")
        return self

    @property
    def all_valid(self) -> bool:
        return all(c.valid for c in (self.temperature, self.optical, self.flow, self.pressure))


def validate_assessment(
    *,
    state: State,
    available: bool,
    channels: Channels,
    readings_valid: bool,
    filtered_valid: bool,
    filtered: float | None,
) -> None:
    if available != (state != "UNAVAILABLE"):
        raise ValueError("Estado e disponibilidade incompatíveis.")
    if available and not (channels.optical.valid and channels.flow.valid):
        raise ValueError("A avaliação exige óptica e vazão válidas.")
    if readings_valid != channels.all_valid:
        raise ValueError("readings_valid deve corresponder à validade dos quatro canais.")
    if filtered_valid:
        if not (channels.optical.valid and channels.flow.valid):
            raise ValueError("O filtro exige óptica e vazão válidas.")
        if filtered is None or not 0 <= filtered <= 100:
            raise ValueError("Índice filtrado inválido.")
    elif filtered is not None:
        raise ValueError("Sem filtro válido, o índice filtrado deve ser omitido.")


class Envelope(StrictModel):
    schema_version: Literal[2]
    device_id: DeviceId
    boot_id: BootId
    simulated: Literal[True]
    time_synced: Literal[False]
    sent_uptime_ms: UInt32
    dropped_events: UInt32 = 0

    @field_validator("simulated", "time_synced", mode="before")
    @classmethod
    def booleans_only(cls, value: object) -> object:
        if not isinstance(value, bool):
            raise ValueError("Este campo deve ser um booleano JSON.")
        return value


class Telemetry(Envelope):
    message_kind: Literal["telemetry"]
    rule_version: Annotated[str, Field(min_length=1, max_length=64)]
    seq: UInt32
    uptime_ms: UInt32
    test_override: bool
    assessment_available: bool
    assessment_scope: Literal["relative_optical_change"]
    optical_observation: str
    observation_provisional: bool
    readings_valid: bool
    filtered_valid: bool
    assessment_state: State
    reason: Annotated[str, Field(max_length=64)]
    flow_context: str
    last_alert_status: Literal["UNKNOWN", "ACTIVE", "CLEARED"]
    last_alert_state: AlertState
    channels: Channels
    temperature_c: float | None = None
    optical_index_raw: float | None = None
    optical_index_filtered: float | None = None
    flow_l_min: float | None = None
    pressure_bar: float | None = None

    @model_validator(mode="after")
    def coherent_assessment(self) -> Self:
        validate_assessment(
            state=self.assessment_state,
            available=self.assessment_available,
            channels=self.channels,
            readings_valid=self.readings_valid,
            filtered_valid=self.filtered_valid,
            filtered=self.optical_index_filtered,
        )
        return self


class Event(Envelope):
    message_kind: Literal["event"]
    event_rule_version: Annotated[str, Field(min_length=1, max_length=64)]
    event_id: Annotated[str, Field(max_length=72)]
    event_seq: UInt32
    event_sample_seq: UInt32
    event_uptime_ms: UInt32
    event_test_override: bool
    event_assessment_available: bool
    event_assessment_scope: Literal["relative_optical_change"]
    event_optical_observation: str
    event_observation_provisional: bool
    event_readings_valid: bool
    event_filtered_valid: bool
    event_state: State
    event_reason: Annotated[str, Field(max_length=64)]
    event_flow_context: str
    event_last_alert_status: Literal["UNKNOWN", "ACTIVE", "CLEARED"]
    event_last_alert_state: AlertState
    event_channels: Channels
    event_temperature_c: float | None = None
    event_optical_index_raw: float | None = None
    event_optical_index_filtered: float | None = None
    event_flow_l_min: float | None = None
    event_pressure_bar: float | None = None

    @model_validator(mode="after")
    def coherent_event(self) -> Self:
        if self.event_id != f"{self.device_id}:{self.boot_id}:{self.event_seq}":
            raise ValueError("event_id não corresponde ao dispositivo, boot e sequência.")
        validate_assessment(
            state=self.event_state,
            available=self.event_assessment_available,
            channels=self.event_channels,
            readings_valid=self.event_readings_valid,
            filtered_valid=self.event_filtered_valid,
            filtered=self.event_optical_index_filtered,
        )
        return self


type Payload = Annotated[Telemetry | Event, Field(discriminator="message_kind")]


class MessageKind(BaseModel):
    message_kind: Literal["telemetry", "event"]


class PayloadAdapter:
    """Despacho tipado por message_kind, seguido da validação completa do modelo."""

    @staticmethod
    def validate_python(value: object) -> Payload:
        kind = MessageKind.model_validate(value)
        if kind.message_kind == "event":
            return Event.model_validate(value)
        return Telemetry.model_validate(value)

    @staticmethod
    def validate_json(value: str | bytes | bytearray) -> Payload:
        kind = MessageKind.model_validate_json(value)
        if kind.message_kind == "event":
            return Event.model_validate_json(value)
        return Telemetry.model_validate_json(value)


payload_adapter = PayloadAdapter()


class Accepted(StrictModel):
    accepted: Literal[True] = True
    duplicate: bool
    current_updated: bool
    message_kind: Literal["telemetry", "event"]
    received_at: datetime | None = None


class TelemetryRecord(BaseModel):
    id: int
    device_id: str
    boot_id: str
    seq: int
    received_at: datetime
    payload: Telemetry
    fiware_synced_at: datetime | None = None


class StoredEvent(Event):
    """Leitura histórica tipada; não é o modelo aceito na ingestão.

    Registros anteriores podem omitir o índice mesmo com filtered_valid=true.
    Eles são preservados sem inventar o número e sinalizados por contract_valid.
    """

    @model_validator(mode="after")
    def coherent_event(self) -> Self:
        return self


class EventRecord(BaseModel):
    event_id: str
    device_id: str
    boot_id: str
    seq: int
    received_at: datetime
    payload: StoredEvent
    contract_valid: bool = True


class Communication(BaseModel):
    stale: bool
    age_ms: int | None
    threshold_seconds: int


class CurrentResponse(BaseModel):
    device_id: str
    current: TelemetryRecord | None
    communication: Communication


class Items[T](BaseModel):
    items: list[T]


class FollowupWrite(StrictModel):
    note: Annotated[str, Field(min_length=1, max_length=1000)]
    status: FollowupStatus

    @field_validator("note")
    @classmethod
    def meaningful_note(cls, value: str) -> str:
        if not value.strip():
            raise ValueError("Escreva uma observação.")
        return value.strip()


class FollowupCreate(FollowupWrite):
    event_id: Annotated[str, Field(min_length=1, max_length=72)]


class FollowupRecord(BaseModel):
    id: str
    event_id: str
    note: str
    status: FollowupStatus
    created_at: datetime
    updated_at: datetime


class IntegrationState(BaseModel):
    configured: bool
    connected: bool = False
    last_success: datetime | None = None
    last_error: str | None = None


class Integrations(BaseModel):
    mqtt: IntegrationState
    fiware: IntegrationState


class StatusResponse(BaseModel):
    devices: tuple[str, ...]
    simulated: Literal[True] = True
    backend: Literal["python"] = "python"
    integrations: Integrations
    server_time: datetime
