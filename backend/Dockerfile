FROM python:3.12-slim AS base
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 PORT=8000
WORKDIR /app
COPY requirements.txt ./
RUN pip install --no-cache-dir --require-hashes -r requirements.txt
COPY vigia ./vigia
COPY scripts ./scripts
RUN useradd --create-home vigia && chown -R vigia:vigia /app

FROM base AS development
COPY requirements-dev.txt pyproject.toml ./
RUN pip install --no-cache-dir --require-hashes -r requirements-dev.txt
COPY tests ./tests
USER vigia
CMD ["sh", "-c", "ruff check . && ruff format --check . && mypy vigia scripts tests && python -m pytest"]

FROM base AS runtime
USER vigia
EXPOSE 8000
CMD ["python", "-m", "vigia"]
