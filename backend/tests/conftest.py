import os
from collections.abc import Iterator
from uuid import uuid4

import pytest
from fastapi.testclient import TestClient
from pydantic import SecretStr
from sqlalchemy.engine import make_url
from sqlalchemy.schema import CreateSchema, DropSchema

from vigia.api import create_app
from vigia.config import Settings
from vigia.database import Store, build_engine


@pytest.fixture
def settings() -> Settings:
    url = os.environ.get("TEST_DATABASE_URL", "")
    if not url or make_url(url).database != "vigia_test":
        pytest.fail("Configure TEST_DATABASE_URL para um banco exclusivo chamado vigia_test.")
    return Settings(
        _env_file=None,
        database_url=SecretStr(url),
        ingest_token=SecretStr("a" * 40),
        admin_token=SecretStr("b" * 40),
        device_ids="vigia-sim-01",
        orion_url="",
        mqtt_url="",
        allowed_origins="http://localhost:5173",
    )


@pytest.fixture
def store(settings: Settings) -> Iterator[Store]:
    root = build_engine(settings)
    schema = "test_" + uuid4().hex
    with root.begin() as connection:
        connection.execute(CreateSchema(schema))
    engine = root.execution_options(schema_translate_map={None: schema})
    repository = Store(engine)
    repository.initialize()
    try:
        yield repository
    finally:
        with root.begin() as connection:
            connection.execute(DropSchema(schema, cascade=True))
        root.dispose()


@pytest.fixture
def client(settings: Settings, store: Store) -> Iterator[TestClient]:
    with TestClient(create_app(settings, store)) as connection:
        yield connection
