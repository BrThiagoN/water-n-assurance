"""Configuração validada; credenciais nunca são incluídas nas respostas da API."""

import re
from typing import Literal, Self
from urllib.parse import urlsplit

from pydantic import Field, SecretStr, field_validator, model_validator
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore", hide_input_in_errors=True)

    database_url: SecretStr
    database_ssl: Literal["disable", "require"] = "disable"
    ingest_token: SecretStr
    admin_token: SecretStr
    port: int = Field(default=8000, ge=1, le=65535)
    device_ids: str = "vigia-sim-01"
    stale_after_seconds: int = Field(default=20, ge=1, le=3600)
    allowed_origins: str = "http://localhost:5173"
    orion_url: str = ""
    fiware_service: str = "waternassurance"
    mqtt_url: str = ""
    mqtt_username: str = ""
    mqtt_password: SecretStr = SecretStr("")

    @field_validator("ingest_token", "admin_token")
    @classmethod
    def secure_token(cls, value: SecretStr) -> SecretStr:
        raw = value.get_secret_value()
        if not re.fullmatch(r"[!-~]{32,128}", raw) or raw.startswith("replace-"):
            raise ValueError("Use um token aleatório de 32 a 128 caracteres, sem espaços.")
        return value

    @field_validator("database_url")
    @classmethod
    def postgres_only(cls, value: SecretStr) -> SecretStr:
        if not value.get_secret_value().startswith(("postgresql://", "postgres://")):
            raise ValueError("DATABASE_URL deve apontar para um Postgres.")
        return value

    @field_validator("device_ids")
    @classmethod
    def valid_devices(cls, value: str) -> str:
        devices = [item.strip() for item in value.split(",") if item.strip()]
        if not devices or any(not re.fullmatch(r"[A-Za-z0-9_-]{1,40}", x) for x in devices):
            raise ValueError(
                "DEVICE_IDS deve conter identificadores válidos separados por vírgula."
            )
        return ",".join(dict.fromkeys(devices))

    @field_validator("orion_url")
    @classmethod
    def valid_orion(cls, value: str) -> str:
        if value and not value.startswith(("http://", "https://")):
            raise ValueError("ORION_URL deve usar http:// ou https://.")
        return value.rstrip("/")

    @model_validator(mode="after")
    def separate_credentials(self) -> Self:
        if self.ingest_token == self.admin_token:
            raise ValueError("INGEST_TOKEN e ADMIN_TOKEN precisam ser diferentes.")
        return self

    @field_validator("mqtt_url")
    @classmethod
    def valid_mqtt(cls, value: str) -> str:
        if value:
            parsed = urlsplit(value)
            if parsed.scheme not in ("mqtt", "mqtts") or not parsed.hostname:
                raise ValueError("MQTT_URL deve usar mqtt:// ou mqtts:// com um hostname.")
            if parsed.username or parsed.password:
                raise ValueError("Use MQTT_USERNAME e MQTT_PASSWORD separados da URL.")
            if parsed.port is not None and not 1 <= parsed.port <= 65535:
                raise ValueError("Porta MQTT inválida.")
        return value

    @property
    def devices(self) -> tuple[str, ...]:
        return tuple(self.device_ids.split(","))

    @property
    def origins(self) -> list[str]:
        return [
            item.strip().rstrip("/") for item in self.allowed_origins.split(",") if item.strip()
        ]
