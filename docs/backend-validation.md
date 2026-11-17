# Validação — API Python e dashboard

Revisão de **16/11/2026**. Os resultados abaixo pertencem à implementação Python, não ao servidor Node-RED anterior.

## Verificações executadas

- Python 3.12.14 no Windows: Ruff e `mypy` em modo estrito, com proibição de `Any` explícito. Nenhum erro de tipagem nos 14 arquivos Python da implementação e dos testes.
- Docker Engine 29.10.0 no Ubuntu/WSL, Python 3.12 e Postgres 17 real: **24 testes aprovados**, sem falhas. A suíte apresentou um aviso de depreciação do TestClient/HTTPX da biblioteca, sem afetar os resultados.
- Os testes cobrem validação estrita de tipos, estados e canais; duplicatas; ingestões concorrentes; ordenação e boots anteriores; conservação de dados ao reinicializar o repositório; outbox FIWARE com HTTP simulado; adaptador MQTT; autenticação; histórico; CSV; CRUD; CORS; limite de tamanho e ausência de interface na API.
- Um registro legado de evento que omitia o índice filtrado foi preservado e marcado como incompleto, sem criar um número. Um teste de regressão verifica esse caso; a ingestão nova continua rejeitando esse formato.
- Smoke test HTTP em container com Postgres real: aprovado, incluindo autenticação, persistência, duplicatas, evento sem regressão do estado e CSV. Apenas mensagens sintéticas identificadas como teste foram usadas.
- Dashboard: **5 testes aprovados**, checagem TypeScript estrita e build Vite aprovados. A auditoria npm no momento da instalação informou zero vulnerabilidades.
- Navegador integrado com API local e Postgres: criação, edição, conclusão e exclusão de uma anotação de teste confirmadas na interface. O evento original permaneceu no histórico.
- Layout conferido na largura padrão do navegador e em 390 px. Na verificação de celular, o conteúdo não ultrapassou a largura disponível. Leituras antigas ficaram identificadas como último estado recebido, com valores atuais ocultos.

## Limites

O teste FIWARE automatizado usa respostas HTTP simuladas; isso não comprova execução de Orion em nuvem. O teste do adaptador MQTT não comprova uma conexão de broker real. As verificações acima também não comprovam, por si, que uma mensagem de um Wokwi real chegou ao servidor publicado: essa integração precisa do próprio registro e da conferência no dashboard.

O registro anterior de 69 testes do firmware pertence à revisão anterior do dispositivo e não é substituído pelos testes Python. Uma nova compilação ou alteração de transporte exige repetir a verificação do Wokwi.

Nenhum teste desta revisão mede água, precisão metrológica, contaminação ou potabilidade. Os dados de demonstração não são resultados de ensaios físicos ou testes com moradores.

## Integração Wokwi → Render — 17/11/2026

O Wokwi público compilou com 69 autotestes aprovados e zero falhas. A sessão `boot_id=1879de1c` enviou mensagens reais do ESP32 simulado para `https://water-n-assurance.onrender.com/api/v1/ingest`. O Serial confirmou respostas HTTPS 201 após a gravação. A API pública retornou, entre outras, a sequência 26 em `NO_CHANGE` e a sequência 91 em `ATTENTION`, com índice óptico 40, comunicação recente e horário de recebimento do servidor. Esses dados são de simulação, não medições físicas.

A captura de rede mostrou o servidor encerrando TCP antes de a CPU emulada terminar TLS. A correção usa o limite de CPU `max`, evita transmitir bitmaps repetidos ao OLED, mantém uma conexão HTTPS reutilizável e verifica o certificado pelo bundle WE1/GTS Root R4. A intermediária WE1 foi extraída de uma conexão com validação TLS do serviço e seu fingerprint SHA-256 conferido (`1dfc1605fbad358d8bc844f76d15203fac9ca5c1a79fd4857ffaf2864fbebf96`). Hostname, assinatura e validade continuam verificados. O token só existe na RAM da sessão; nenhum token foi incluído no projeto público.

Foi encontrado também um desvio de três horas entre o relógio do computador e o servidor. O painel calculava a validade pelo relógio local e ocultava valores recém-recebidos. A correção usa `communication.age_ms` da API e o tempo monotônico desde a resposta; os filtros de período usam o horário do servidor. O teste de regressão cobre relógio local adiantado e atrasado, além do envelhecimento sem novas respostas. Cinco testes do dashboard e o build TypeScript/Vite passaram.

### Conferência da publicação

O commit [`5138b49`](https://github.com/BrThiagoN/water-n-assurance/commit/5138b49f9eb6817fd50445b4c0fbcdb1b47c4456) passou na CI do dashboard e foi publicado como Static Site no Render (deploy `dep-dbtq6ke7bikc73fdn0vg`, status Live). O navegador carregou o novo bundle e exibiu **Dados atualizados**, estado Atenção, índice 40, temperatura 25 °C, vazão 6 L/min e pressão 2 bar. O filtro **Última hora** preservou 66 registros, mesmo com o relógio do computador deslocado.

Na mesma sessão `1879de1c`, a sequência 115 chegou como `CHECK`, índice 80. A sequência 266 chegou como `UNAVAILABLE`, com o canal óptico inválido, após `TEST FAULT OPTICAL ON`. A recuperação da entrada passou por renovação, filtro e confirmação antes de voltar a Atenção.

Com `TEST NET OFF`, a sequência 330 deixou de receber atualizações: a API informou idade superior a 54 segundos e `stale=true`. O painel mostrou **Último estado recebido**, ocultou os valores atuais e identificou os valores anteriores. Após `TEST NET ON`, houve nova confirmação HTTPS 201; a sequência 358 estava em `NO_CHANGE`, índice 20 e comunicação recente. O painel publicado voltou a **Dados atualizados**, com 20 / 100, 25 °C, 6 L/min e 2 bar. O comando `MUTE` reconheceu o aviso pendente. A simulação permaneceu ligada, com credencial apenas na RAM.

O navegador precisa recarregar a página após um novo deploy para carregar o bundle atualizado. Reiniciar a simulação exige carregar novamente o token de ingestão; perder a conexão não cria medições e não apaga o histórico. As limitações FIWARE/IoT Cloud e de ensaio físico descritas acima permanecem.

## FIWARE real — revisão de 17/11/2026

- API, Orion 4.2.0 e MongoDB 8.0 foram executados em um mesmo container Linux, com limite de 512 MiB. O consumo observado estabilizou próximo de 166 MiB; uma leitura anterior durante a inicialização ficou em 287 MiB. Esses valores são observações do ensaio, não um limite garantido para qualquer carga.
- Com 0.1 CPU, mensagens HTTP sintéticas nos estados NO_CHANGE, ATTENTION e CHECK chegaram ao Orion real. O endpoint de contexto confirmou o payload e a consulta atual informou `source: fiware-orion`.
- A entidade de teste foi removida do Orion; o worker a reconstruiu pelo Postgres sem nova ingestão, preservando a sequência.
- Foi corrigida uma incompatibilidade real de data NGSI-v2: `receivedAt` usa UTC com milissegundos e sufixo Z. O valor original continua preservado no histórico.
- Lint e mypy estrito aprovados. A suíte com Postgres real passou em **26 testes**. Foram incluídos testes de reconstrução, leitura do contexto, contexto atrasado e fallback durante indisponibilidade.
- O script `scripts.check_fiware` e a CI exercitam o perfil real. Os ensaios que alteram dados são limitados ao ambiente local, com mensagens identificadas como sintéticas. A comprovação com o Wokwi publicado pertence ao registro de nuvem após o deploy.

A implementação usa imagens oficiais fixadas por digest e processos sem root. Orion e Mongo escutam somente em loopback; o endpoint público permite apenas consultar o contexto sintético. O Mongo do perfil Free é um cache de contexto reconstruível, e o Postgres continua sendo o histórico durável.
