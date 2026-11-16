# Apoio de IA no backend

Usamos o Codex para refatorar o servidor para Python tipado, mantendo o contrato do firmware e a persistência Postgres. A decisão de usar Python e separar a interface em `dashboard/` foi da equipe em 16/11/2026. Node-RED não foi apresentado como requisito obrigatório do enunciado.

O apoio incluiu modelos Pydantic, rotas FastAPI, mapeamento SQLAlchemy tipado, autenticação, deduplicação, histórico, CSV, CRUD de acompanhamentos, adaptadores MQTT/FIWARE, Docker, configuração do Render e testes. Nenhum painel ou código de interface pertence a esta pasta.

Conferimos o contrato contra os encoders do firmware. Lint, tipagem estrita e testes com Postgres real fazem parte da validação, registrada em [backend-validation.md](../docs/backend-validation.md). Resultados antigos de Node-RED não comprovam a implementação Python. A equipe deve conferir os resultados e compreender o fluxo de uma mensagem antes de apresentá-lo.

O requisito de plataforma FIWARE/IoT Cloud é separado da linguagem Python. Adaptador disponível não comprova integração configurada; testes com mensagens sintéticas não comprovam a conexão de um Wokwi real nem medição física de água. A classificação local e a documentação não garantem potabilidade.
