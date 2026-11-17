# Dependencias oficiais fixadas por digest. Apenas PORT da API fica publico.
FROM mongo:8.0@sha256:bdc216e9d5a6ec2cfdac789b5832fdab64cd32055f59885cae918a92de63c361 AS mongo
RUN mkdir /mongo-runtime && ldd /usr/bin/mongod | \
    awk '/=> \// {print $3} /^[[:space:]]*\// {print $1}' | \
    xargs -r cp -L -t /mongo-runtime

FROM quay.io/fiware/orion:4.2.0@sha256:24eb7fb792eeade947c49118d3bc9778f116b74e7761ae2991c786f4d46db00f AS orion

FROM python:3.12-slim-bookworm@sha256:935c3c5b12357a7e10caa9c1e6ca5692344d97cf229832ef584be2cec9fc03df
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 PORT=8000 \
    ORION_URL=http://127.0.0.1:1026
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-thread1.74.0 libcurl4 libgnutls30 libssl3 libsasl2-2 libuuid1 libstdc++6 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=mongo /usr/bin/mongod /opt/mongodb/mongod
COPY --from=mongo /mongo-runtime/ /opt/mongodb/lib/
COPY --from=orion /usr/bin/contextBroker /usr/bin/contextBroker
# O allocator do Mongo consulta a topologia da CPU, indisponivel no builder
# isolado do Render. A inicializacao real e verificada na CI e no runtime.
RUN ldd /usr/bin/contextBroker && /usr/bin/contextBroker --version
WORKDIR /app
COPY backend/requirements.txt ./
RUN pip install --no-cache-dir --require-hashes -r requirements.txt
COPY backend/vigia ./vigia
COPY backend/scripts ./scripts
RUN useradd --create-home vigia && chown -R vigia:vigia /app
USER vigia
EXPOSE 8000
CMD ["python", "-m", "scripts.run_fiware"]
