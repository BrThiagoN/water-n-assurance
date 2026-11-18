# FIWARE no Render Free

O perfil de demonstração executa **API Python 3.12, FIWARE Orion 4.2.0 e MongoDB 8.0** no mesmo Web Service Free. O dashboard permanece um Static Site separado e o histórico permanece no Postgres existente. Nenhuma conta MongoDB Atlas é necessária.

Somente a API escuta em `0.0.0.0:$PORT`. Orion (`127.0.0.1:1026`) e Mongo (`127.0.0.1:27017`) são internos ao container; suas operações de escrita não são publicadas na internet. O processo roda como usuário `vigia`, sem root. As imagens de dependências estão fixadas por digest no Dockerfile.

## Caminho real dos dados

1. O ESP32 do Wokwi envia `telemetry` via HTTPS autenticado para a API.
2. A API grava a transação no Postgres e agenda a sincronização na outbox.
3. O worker atualiza uma entidade `WaterMonitor` no Orion usando **NGSI-v2**.
4. A consulta de estado lê o Orion. Quando o contexto corresponde ao snapshot atual do Postgres, a resposta informa `source: "fiware-orion"` e utiliza seu payload.
5. O dashboard consome essa resposta. Se o Orion estiver indisponível ou atrasado, a API conserva a leitura persistida, informa `source: "postgres"` e não regride para um contexto antigo.

O endpoint público de leitura `/api/v1/devices/vigia-sim-01/context` retorna a entidade realmente consultada no Orion, com `protocol: "NGSI-v2"` e `matches_current`. Ele contém somente os mesmos dados sintéticos já disponibilizados pela API. Não é uma implementação substituta do broker.

## Persistência e reinícios

O filesystem do Render Free é temporário. O Mongo deste perfil mantém **contexto atual reconstruível**, sem ser o histórico oficial. A cada ciclo, o worker verifica os contextos e reconstrói os ausentes ou divergentes a partir do Postgres, preservando boot, sequência, valores e horário original de recebimento. Isso não cria uma nova medição nem transforma um registro antigo em atual.

O supervisor `backend/scripts/run_fiware.py` inicia Mongo, Orion e API nessa ordem. Se um dos processos terminar, encerra o conjunto para que o ambiente identifique a falha. O perfil é acadêmico: depende dos limites do plano Free e da disponibilidade do Postgres, que expira após 30 dias no plano gratuito.

## Reproduzir o mesmo conjunto no Linux/WSL

Na raiz do repositório, com Docker e Compose:

```bash
sudo docker compose -f backend/compose.yaml --profile fiware-demo up -d --build fiware-demo
sudo docker compose -f backend/compose.yaml exec -T fiware-demo python -m scripts.check_fiware --send-demo --exercise-recovery
sudo docker stats --no-stream
```

Esse perfil usa um Postgres isolado de demonstração, expõe a API em `http://localhost:8001` e impõe **512 MiB e 0.1 CPU** ao container com os três processos. As chaves fixas do Compose são somente desse teste local. O Render mantém suas próprias chaves geradas, nunca as chaves de teste.

O teste injeta mensagens sintéticas identificadas por um boot `check-*`, confere três estados, apaga somente a entidade de contexto do ambiente local e verifica sua reconstrução sem uma nova leitura. As opções que alteram dados são bloqueadas pelo script quando a URL não é localhost.

Para encerrar somente a demonstração:

```bash
sudo docker compose -f backend/compose.yaml --profile fiware-demo stop fiware-demo fiware-demo-db
```

## Conferir a publicação e preparar o vídeo

```bash
curl --fail https://water-n-assurance.onrender.com/api/v1/status
curl --fail https://water-n-assurance.onrender.com/api/v1/devices/vigia-sim-01/context
curl --fail https://water-n-assurance.onrender.com/api/v1/devices/vigia-sim-01/current
```

Após a sincronização, procurar `fiware.configured: true`, `fiware.connected: true`, `matches_current: true` e `source: "fiware-orion"`. O campo `stale` continua indicando se o dispositivo enviou algo recentemente; a reconstrução do contexto não reinicia essa idade.

No Wokwi, carregar a chave da sessão fora da gravação, iniciar a rede e alterar o índice óptico. Mostrar no vídeo o aparelho, a entidade consultada no Orion e a alteração no dashboard. Dados de um `check-*` foram produzidos pelo teste HTTP; não apresentá-los como uma execução do ESP32.

## Implantação

O `render.yaml` usa runtime **Docker**, `infra/fiware/Dockerfile` e contexto da raiz. Sincronizar o Blueprint existente preserva o serviço, o banco e os tokens. Conferir que o plano continua **Free**, que `ORION_URL=http://127.0.0.1:1026` e que o deploy fica Live antes da gravação.

Referências: [Orion oficial](https://fiware-orion.readthedocs.io/en/4.2.0/admin/install.html), [Render Free](https://render.com/docs/free). Para repetir os testes e consultar seus limites, veja [tipagem e testes do backend](../../backend/README.md#tipagem-e-testes) e o [roteiro de testes do firmware](../../firmware/wokwi/TESTES.md).
