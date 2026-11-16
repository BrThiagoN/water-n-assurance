"""Gera configuração local; não imprime tokens e não sobrescreve um .env existente."""

import os
import secrets
from pathlib import Path


def main() -> None:
    target = Path(".env")
    if target.exists():
        print(".env já existe; configuração preservada.")
        return
    content = (
        "PORT=8000\n"
        "DATABASE_URL=postgresql://vigia:vigia_local@localhost:5432/vigia\n"
        "DATABASE_SSL=disable\n"
        f"INGEST_TOKEN={secrets.token_urlsafe(32)}\n"
        f"ADMIN_TOKEN={secrets.token_urlsafe(32)}\n"
        "DEVICE_IDS=vigia-sim-01\nSTALE_AFTER_SECONDS=20\n"
        "ALLOWED_ORIGINS=http://localhost:5173\n"
        "MQTT_URL=\nMQTT_USERNAME=\nMQTT_PASSWORD=\n"
        "ORION_URL=\nFIWARE_SERVICE=waternassurance\n"
    )
    descriptor = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
        output.write(content)
    print(".env criado com tokens distintos. Guarde-o apenas no ambiente local.")


if __name__ == "__main__":
    main()
