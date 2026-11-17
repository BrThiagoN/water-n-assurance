# water n' assurance — backend Python no Linux/WSL

> Vigia d’Água · revisão de 17/11/2026.

O servidor fica em [`backend/`](../backend/README.md), com Python 3.12, FastAPI, Pydantic, SQLAlchemy e Postgres. A interface React/TypeScript fica em [`dashboard/`](../dashboard/README.md), separada na raiz. A decisão e os avisos do dispositivo permanecem no ESP32.

## 1. Arquitetura

```mermaid
flowchart LR
    ESP["ESP32 no Wokwi"] -->|"HTTPS autenticado, schema v2"| API["API Python tipada"]
    MQTT["Broker MQTT opcional"] --> API
    API --> PG["Postgres: estado, histórico e acompanhamentos"]
    UI["dashboard/: React + TypeScript"] -->|"HTTPS e JSON"| API
    API -->|"Outbox NGSI-v2"| ORION["FIWARE Orion"]
    ORION -->|"Contexto correspondente ao snapshot"| API
```

Python é a linguagem da API. O perfil do Render inclui FIWARE Orion como plataforma IoT; o vídeo pode mostrar sua entidade NGSI-v2 pelo endpoint de contexto. Node-RED foi uma proposta anterior, não uma exigência escrita confirmada.

## 2. Começar com o Docker já instalado no Ubuntu

No WSL:

```bash
sudo docker info
mkdir -p ~/projetos
cd ~/projetos
git clone https://github.com/BrThiagoN/water-n-assurance.git
cd water-n-assurance/backend
python3 scripts/setup_env.py
sudo docker compose up -d --build --wait backend
curl --fail-with-body http://localhost:8000/healthz
sudo docker compose exec backend python -m scripts.smoke
```

Se já clonou, atualize a pasta existente com `git pull --ff-only`, preservando alterações locais. O gerador não substitui `.env` existente. Ele funciona com o Python do Ubuntu e não exige as bibliotecas da API. Os containers usam Python 3.12.

Mantenha uma sessão Ubuntu aberta ao trabalhar localmente. Se seu usuário tem acesso ao daemon, `sudo` pode ser omitido; `docker -v` sozinho não verifica essa permissão.

## 3. Abrir o dashboard separado

Em outro terminal, com Node.js 24.15+ da linha 24:

```bash
cd water-n-assurance/dashboard
npm ci
npm run dev
```

Abra `http://localhost:5173`. A configuração padrão consulta `http://localhost:8000`. A URL pública da API pode ser definida em `dashboard/.env.local` por `VITE_API_URL`. Não coloque tokens nesse arquivo: variáveis `VITE_` entram no JavaScript público.

O dashboard usa a paleta Merino `#F5EEDD`, Rock Blue `#84B3CE`, Venice Blue `#16587B` e as fontes dos slides: DM Serif Display e Source Sans 3. Valores inválidos ou antigos não são apresentados como leituras atuais. Os acompanhamentos usam a chave administrativa digitada na sessão, sem armazenamento local.

## 4. Tipagem e testes

```bash
cd backend
sudo docker compose --profile test up -d test-db
sudo docker compose --profile test run --rm --build tests
```

Esse comando roda Ruff, `mypy` estrito e testes com Postgres real em banco isolado. No dashboard:

```bash
npm test
npm run build
```

O teste HTTP de demonstração gera dados sintéticos identificados como teste. Não confundir esses registros com uma mensagem realmente produzida pelo ESP32. O [registro de validação](backend-validation.md) diferencia as verificações executadas das integrações pendentes.

## 5. Conectar o Wokwi ao Render

O firmware usa `POST https://water-n-assurance.onrender.com/api/v1/ingest`, sem depender de um broker MQTT. A biblioteca HTTPClient do ESP32 envia o JSON ao mesmo endpoint usado pelos testes da API. Não trocar esse endereço por `localhost`: o gateway público do Wokwi não alcança o servidor local do notebook.

Abra o [dashboard publicado](https://water-n-assurance-dashboard.onrender.com/) e inicie o [Wokwi](https://wokwi.com/projects/480416382701050881). Na conta Render da equipe, copie `INGEST_TOKEN` do serviço da API e envie `NET TOKEN <chave>` no Serial Monitor, substituindo o marcador pelo valor. Se necessário envie `TEST NET ON`. A chave é sensível a maiúsculas e minúsculas e fica somente na RAM; ao reiniciar a simulação é necessário carregá-la novamente. Não usar `ADMIN_TOKEN` nem salvar a chave nas abas públicas.

O corpo JSON continua em `schema_version: 2`, com `telemetry` ou `event`. O transporte preserva filas, identificadores, retentativas e execução independente da lógica local. TLS verifica a CA pública e usa o relógio sincronizado; não há `setInsecure()`. A linha `[NET] HTTPS 201: persistencia confirmada pelo backend Python.` indica aceitação após gravação; 200 pode indicar repetição. O dashboard consulta a API a cada dez segundos. O [guia do firmware](../firmware/wokwi/README.md) explica testes e diagnóstico. MQTT permanece como entrada opcional da API, desligada enquanto não houver broker.

## 6. FIWARE e Render

Para Orion local, defina `ORION_URL=http://orion:1026` em `backend/.env` e inicie `sudo docker compose --profile fiware up -d --build`. A sincronização NGSI-v2 usa uma outbox persistente; sem Orion disponível, permanece pendente.

O [`render.yaml`](../render.yaml) define a API Python **Free**, Postgres **Free** e o dashboard como Static Site. Ao sincronizar o Blueprint, preserve banco e tokens e confira os endereços usados em `VITE_API_URL` e `ALLOWED_ORIGINS`. Nenhum serviço pago foi autorizado.

O Web Service gratuito suspende por inatividade; o Postgres gratuito expira após 30 dias. A suspensão interrompe coleta em segundo plano. O histórico fica no Postgres, não no filesystem temporário do Render. O perfil Docker inclui Orion e Mongo internos ao container. O contexto Mongo é temporário e reconstruído pelo Postgres; o [guia específico](../infra/fiware/README.md) explica como testar e demonstrar essa arquitetura.

## 7. Antes do vídeo de Edge

1. Repetir os cenários do [roteiro do firmware](../firmware/wokwi/TESTES.md).
2. Fazer uma mensagem realmente gerada no Wokwi chegar ao backend e aparecer no dashboard.
3. Mostrar `/api/v1/devices/vigia-sim-01/context` com `matches_current: true` e a consulta atual com `source: fiware-orion`, após uma alteração no Wokwi.
4. Mostrar os estados e os avisos locais, inclusive falha e reconhecimento do som.
5. Mostrar o histórico e a distinção entre comunicação antiga e leitura atual.
6. Gravar o vídeo narrado de até sete minutos, identificando simulação, componentes, limites e os quatro integrantes.

## Equipe

| Integrante | RM |
|---|---|
| Thiago Gomes Nascimento | RM569436 |
| Rafael De Sousa | RM568777 |
| Vitor Assis Florenzano | RM572192 |
| Raul Moreira Andrade | RM571042 |

Companhia: **water n' assurance**. Produto: **Vigia d’Água**. Vila Prudente/SP facilita a execução inicial pela proximidade da equipe; o relato familiar de água de poço em Iguatu/CE motivou a ideia, sem causa laboratorialmente confirmada.

Referências: [FastAPI](https://fastapi.tiangolo.com/), [Pydantic](https://docs.pydantic.dev/latest/), [SQLAlchemy](https://docs.sqlalchemy.org/en/20/orm/declarative_tables.html), [Render Python](https://render.com/docs/deploy-fastapi), [Render Free](https://render.com/docs/free), [FIWARE NGSI-v2](https://fiware-orion.readthedocs.io/en/master/orion-api.html).
