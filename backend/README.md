# Backend Python — Vigia d’Água

API da **water n' assurance**, escrita em **Python 3.12, FastAPI, Pydantic e SQLAlchemy**, com Postgres. Esta pasta contém somente o backend, sua configuração, documentação e testes. A interface está em [`../dashboard`](../dashboard/README.md).

## O que o servidor faz

- Recebe telemetria e eventos do contrato `schema_version: 2`, sem recalcular a decisão do ESP32.
- Valida tipos, unidades, estados e validade por canal. Sequências não aceitam texto ou booleanos.
- Persiste leituras, eventos, último estado e acompanhamentos em transações Postgres.
- Reconhece duplicatas sem renovar o horário original. Eventos e boots já encerrados não fazem o estado atual regredir.
- Expõe histórico, exportação CSV e CRUD de acompanhamentos, com tokens separados de ingestão e administração.
- Integra MQTT e FIWARE Orion quando configurados. Sem configuração, a API informa isso explicitamente.

Todos os dados aceitos nesta etapa são simulados. Nenhum estado equivale a uma confirmação de potabilidade.

## Começar no Ubuntu/WSL com Docker

Você já tem Ubuntu e Docker dentro do WSL. No terminal Ubuntu, entre na pasta do projeto:

```bash
git clone https://github.com/BrThiagoN/water-n-assurance.git
cd water-n-assurance/backend
```

Se já clonou, use a pasta existente e atualize com `git pull --ff-only`, depois de preservar alterações locais. Confira o daemon com `sudo docker info`.

1. Crie a configuração local:

```bash
python3 scripts/setup_env.py
```

O script usa só a biblioteca padrão do Python. Ele cria `.env` com tokens aleatórios e diferentes, sem imprimi-los e sem substituir um arquivo existente.

2. Inicie a API e o banco:

```bash
sudo docker compose up -d --build --wait backend
```

3. Confira a saúde:

```bash
curl --fail-with-body http://localhost:8000/healthz
```

O JSON deve informar `status: ok`, `backend: python` e `framework: fastapi`. A raiz também responde JSON. `/docs`, `/red`, `/admin` e os antigos arquivos de interface não existem nesta API. O contrato OpenAPI está em `/openapi.json`.

4. Envie dados sintéticos para conferir a API:

```bash
sudo docker compose exec backend python -m scripts.smoke
```

Este teste deixa registros identificados como simulação e entrada de teste. Não representa uma leitura produzida pelo Wokwi. O `boot_id` é aleatório e os tokens não são impressos.

5. Abra a interface separada seguindo [`dashboard/README.md`](../dashboard/README.md). No Windows, ela acessa a API do WSL por `http://localhost:8000`.

```bash
sudo docker compose logs --tail 50 backend
sudo docker compose stop
sudo docker compose start db backend
```

Os registros permanecem no volume do Postgres. Não use `down -v` para uma reinicialização comum. Sem acesso ao daemon pelo seu usuário, mantenha `sudo`; `docker -v` sozinho não verifica essa permissão.

## Tipagem e testes

`mypy` usa **modo estrito**, sem `Any` explícito. Pydantic valida os dados em execução; os modelos também descrevem as respostas da API. O mapeamento SQLAlchemy usa `Mapped[...]`, e o dashboard valida o JSON recebido separadamente.

Para repetir lint, formatação, tipagem e testes com Postgres real, sem instalar dependências no Ubuntu:

```bash
sudo docker compose --profile test up -d test-db
sudo docker compose --profile test run --rm --build tests
```

Os testes usam um banco isolado chamado `vigia_test` e um schema temporário por caso. Não usam o banco de demonstração nem SQLite. A esteira do GitHub repete essas verificações.

Para desenvolver com Python 3.12 instalado, crie um ambiente virtual e instale o arquivo de desenvolvimento:

```bash
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install --require-hashes -r requirements-dev.txt
python scripts/setup_env.py
ruff check .
ruff format --check .
mypy vigia scripts tests
```

Ajuste `DATABASE_URL` para um Postgres acessível ao processo local. Para iniciar:

```bash
python -m vigia
```

No PowerShell, a ativação equivalente é `.venv\Scripts\Activate.ps1`. Para executar `python -m pytest` diretamente, forneça `TEST_DATABASE_URL` apontando para um banco exclusivo chamado `vigia_test`. O Compose já faz essa configuração.

## Endpoints

| Método e caminho | Acesso | Uso |
|---|---|---|
| `GET /healthz` | Público | Saúde do banco e identificação do backend. |
| `GET /api/v1/status` | Público | Dispositivos de demonstração e estado das integrações. |
| `POST /api/v1/ingest` | `INGEST_TOKEN` | Telemetria ou evento do firmware. |
| `GET /api/v1/devices/{id}/current` | Público | Último snapshot e idade da comunicação. |
| `GET /api/v1/devices/{id}/telemetry?limit=100` | Público | Até 200 leituras. |
| `GET /api/v1/devices/{id}/events?limit=100` | Público | Até 200 eventos. |
| `GET /api/v1/devices/{id}/export.csv` | Público | Últimas 200 leituras em CSV. |
| `GET, POST /api/v1/followups` | `ADMIN_TOKEN` | Listar/criar acompanhamentos. |
| `PATCH, DELETE /api/v1/followups/{id}` | `ADMIN_TOKEN` | Editar/excluir a anotação; o evento é preservado. |
| `GET /openapi.json` | Público | Contrato JSON da API, sem interface Swagger. |

Envie tokens no cabeçalho `Authorization: Bearer TOKEN`. Criação de acompanhamento usa `event_id`, `note` e `status` (`OPEN`/`CLOSED`); edição usa `note` e `status`. Tipos ou dados inválidos recebem HTTP 400; credencial inválida, 401; recurso ausente, 404; mensagens acima de 32 KiB, 413; falha de persistência, 503.

Os endpoints públicos destinam-se à demonstração com dados sintéticos. Não publique dados pessoais ou medições reais nesse ambiente. `ALLOWED_ORIGINS` lista as origens do dashboard; CORS não substitui autenticação.

## Dados, horário e persistência

`received_at` é o recebimento no servidor. O firmware não fornece UTC confiável, e `uptime_ms` não vira data da medição. Campos inválidos não devem ser apresentados como valores atuais. A idade da comunicação é independente do estado calculado no dispositivo.

O servidor conserva as tabelas da versão anterior: `telemetry`, `events`, `devices`, `fiware_outbox` e `followups`. A inicialização cria tabelas ausentes, sem apagar os registros existentes. Mudanças futuras de schema precisam de migrações explícitas.

Transações usam bloqueio Postgres por dispositivo para lidar com ingestões concorrentes. Dentro de um boot, a sequência define a ordem. Um boot já registrado e encerrado não substitui o atual. Para um boot nunca visto, a ordem ainda depende da chegada: sem relógio confiável do dispositivo, não há prova de ordem global.

## MQTT e FIWARE

Para MQTT, configure `MQTT_URL`, `MQTT_USERNAME` e `MQTT_PASSWORD`. São aceitos `mqtts://` com verificação de certificado e `mqtt://` para um ambiente local isolado. Os tópicos são:

```text
water-n-assurance/v1/vigia-sim-01/telemetry
water-n-assurance/v1/vigia-sim-01/events
```

O adaptador valida o tópico e usa a mesma persistência da API HTTP. O protocolo não oferece confirmação de aplicação de volta ao firmware nesta versão; PUBACK não comprova gravação no Postgres. Um Render Free suspenso não mantém a coleta MQTT.

Para testar FIWARE localmente, configure `ORION_URL=http://orion:1026` no `.env` e execute:

```bash
sudo docker compose --profile fiware up -d --build
```

A entidade é `urn:ngsi-ld:WaterMonitor:vigia-sim-01`, com serviço `waternassurance`, caminho `/` e API NGSI-v2. A outbox persiste no Postgres e permite retentar atualizações. Sem Orion configurado, não declarar que essa integração está funcionando. Python é a linguagem da API; o requisito acadêmico FIWARE/IoT Cloud é uma escolha de plataforma separada.

## FIWARE no Render Free

O perfil de nuvem inclui Orion e Mongo no mesmo container da API, com acesso somente por loopback. O Postgres mantém histórico e acompanhamentos; o contexto Mongo é reconstruído depois de reinícios. A consulta atual usa o payload do Orion quando corresponde ao snapshot persistido e informa sua origem em `source`. Para conferir a entidade real, use `GET /api/v1/devices/vigia-sim-01/context`.

O [guia de FIWARE no Render](../infra/fiware/README.md) explica o perfil, o teste com limite de 512 MiB/0.1 CPU e os comandos de demonstração.

## Render

O [`render.yaml`](../render.yaml) separa a API Python, o Postgres **Free** e o dashboard como **Static Site**. O Web Service usa runtime Docker e `infra/fiware/Dockerfile`, com Python 3.12 e os processos internos do FIWARE. Os pacotes Python continuam instalados com hashes; `PORT` vem do Render.

Ao atualizar o Blueprint existente, confira o runtime **Docker** e o plano **Free** e preserve o banco e os tokens gerados. Se o Render atribuir outro endereço ao dashboard, ajuste `ALLOWED_ORIGINS`; se mudar o endereço da API, ajuste `VITE_API_URL` e reconstrua o dashboard. Não selecione recursos pagos.

O [plano Free](https://render.com/docs/free) suspende o Web Service após inatividade, tem filesystem temporário e o Postgres gratuito expira após 30 dias. Preserve/exporte os dados antes da expiração. O Static Site é independente da API. Orion e Mongo executam internamente no container; o Mongo é um contexto temporário reconstruível pelo Postgres.

O Render termina o HTTPS público. A conexão interna do banco usa `DATABASE_SSL=disable`; para um banco externo com TLS, `DATABASE_SSL=require` usa `verify-full`, sem desabilitar a verificação de certificado.

## Equipe

| Integrante | RM |
|---|---|
| Thiago Gomes Nascimento | RM569436 |
| Rafael De Sousa | RM568777 |
| Vitor Assis Florenzano | RM572192 |
| Raul Moreira Andrade | RM571042 |

Uso de IA e limites da validação: [AI.md](AI.md) e [registro de validação](../docs/backend-validation.md).
