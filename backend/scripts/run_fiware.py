"""Perfil academico Render Free: API, Orion e Mongo de contexto no mesmo container.

Mongo e Orion ouvem somente em loopback. O Postgres e o historico duravel;
o worker reconstrui o contexto NGSI sem alterar a data da leitura original.
"""

import os
import signal
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path
from types import FrameType


def main() -> int:
    stop = threading.Event()
    children: list[tuple[str, subprocess.Popen[bytes]]] = []

    def shutdown(signum: int, frame: FrameType | None) -> None:
        stop.set()

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)
    os.environ["ORION_URL"] = "http://127.0.0.1:1026"
    directory = Path("/tmp/vigia-mongo")
    directory.mkdir(exist_ok=True)

    def start(name: str, command: list[str]) -> None:
        process = subprocess.Popen(command)
        children.append((name, process))
        print(f"[FIWARE] {name} iniciado.", flush=True)

    def wait_ready(port: int, url: str | None = None) -> None:
        deadline = time.monotonic() + 150
        while not stop.is_set() and time.monotonic() < deadline:
            if any(process.poll() is not None for _, process in children):
                raise RuntimeError("Um processo terminou durante a inicializacao.")
            try:
                if url:
                    with urllib.request.urlopen(url, timeout=3) as response:
                        if response.status == 200:
                            return
                else:
                    with socket.create_connection(("127.0.0.1", port), timeout=2):
                        return
            except (OSError, urllib.error.URLError):
                pass
            stop.wait(1)
        raise RuntimeError("Tempo de inicializacao esgotado ou encerramento solicitado.")

    try:
        start(
            "MongoDB local",
            [
                "/opt/mongodb/lib/ld-linux-x86-64.so.2",
                "--library-path",
                "/opt/mongodb/lib",
                "/opt/mongodb/mongod",
                "--bind_ip",
                "127.0.0.1",
                "--port",
                "27017",
                "--dbpath",
                str(directory),
                "--wiredTigerCacheSizeGB",
                "0.25",
                "--nounixsocket",
                "--setParameter",
                "diagnosticDataCollectionEnabled=false",
                "--logpath",
                "/tmp/vigia-mongo.log",
            ],
        )
        wait_ready(27017)
        start(
            "Orion 4.2.0",
            [
                "contextBroker",
                "-fg",
                "-localIp",
                "127.0.0.1",
                "-port",
                "1026",
                "-ipv4",
                "-dbURI",
                "mongodb://127.0.0.1:27017",
                "-dbPoolSize",
                "2",
                "-reqPoolSize",
                "2",
                "-maxConnections",
                "20",
                "-connectionMemory",
                "64",
                "-disableNgsiv1",
                "-disableFileLog",
                "-logLevel",
                "WARN",
            ],
        )
        wait_ready(1026, "http://127.0.0.1:1026/version")
        start("API Python", [sys.executable, "-m", "vigia"])
        while not stop.wait(1):
            for name, process in children:
                if process.poll() is not None:
                    print(f"[FIWARE] {name} terminou; encerrando o conjunto.", flush=True)
                    return 1
        return 0
    except (OSError, RuntimeError) as error:
        print(f"[FIWARE] Falha na inicializacao: {error}", flush=True)
        return 1
    finally:
        for _, process in reversed(children):
            if process.poll() is None:
                process.terminate()
        for _, process in reversed(children):
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
