"""Ponto de entrada ASGI usado pelo Render e pelo Uvicorn."""

from vigia.api import create_app
from vigia.config import Settings

app = create_app(Settings())
