# Validação — API Python e dashboard

Revisão de **16/11/2026**. Os resultados abaixo pertencem à implementação Python, não ao servidor Node-RED anterior.

## Verificações executadas

- Python 3.12.14 no Windows: Ruff e `mypy` em modo estrito, com proibição de `Any` explícito. Nenhum erro de tipagem nos 14 arquivos Python da implementação e dos testes.
- Docker Engine 29.10.0 no Ubuntu/WSL, Python 3.12 e Postgres 17 real: **24 testes aprovados**, sem falhas. A suíte apresentou um aviso de depreciação do TestClient/HTTPX da biblioteca, sem afetar os resultados.
- Os testes cobrem validação estrita de tipos, estados e canais; duplicatas; ingestões concorrentes; ordenação e boots anteriores; conservação de dados ao reinicializar o repositório; outbox FIWARE com HTTP simulado; adaptador MQTT; autenticação; histórico; CSV; CRUD; CORS; limite de tamanho e ausência de interface na API.
- Um registro legado de evento que omitia o índice filtrado foi preservado e marcado como incompleto, sem criar um número. Um teste de regressão verifica esse caso; a ingestão nova continua rejeitando esse formato.
- Smoke test HTTP em container com Postgres real: aprovado, incluindo autenticação, persistência, duplicatas, evento sem regressão do estado e CSV. Apenas mensagens sintéticas identificadas como teste foram usadas.
- Dashboard: **4 testes aprovados**, checagem TypeScript estrita e build Vite aprovados. A auditoria npm no momento da instalação informou zero vulnerabilidades.
- Navegador integrado com API local e Postgres: criação, edição, conclusão e exclusão de uma anotação de teste confirmadas na interface. O evento original permaneceu no histórico.
- Layout conferido na largura padrão do navegador e em 390 px. Na verificação de celular, o conteúdo não ultrapassou a largura disponível. Leituras antigas ficaram identificadas como último estado recebido, com valores atuais ocultos.

## Limites

O teste FIWARE automatizado usa respostas HTTP simuladas; isso não comprova execução de Orion em nuvem. O teste do adaptador MQTT não comprova uma conexão de broker real. As verificações acima também não comprovam, por si, que uma mensagem de um Wokwi real chegou ao servidor publicado: essa integração precisa do próprio registro e da conferência no dashboard.

O registro anterior de 69 testes do firmware pertence à revisão anterior do dispositivo e não é substituído pelos testes Python. Uma nova compilação ou alteração de transporte exige repetir a verificação do Wokwi.

Nenhum teste desta revisão mede água, precisão metrológica, contaminação ou potabilidade. Os dados de demonstração não são resultados de ensaios físicos ou testes com moradores.
