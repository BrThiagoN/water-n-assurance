# Vídeo e entrega de Edge — Vigia d’Água

A integração técnica foi validada em 17/11/2026: Wokwi, API Python, Postgres, FIWARE Orion real e dashboard no Render gratuito. As instruções para repetir os checks automatizados estão no [README do backend](../../backend/README.md#tipagem-e-testes), e os cenários do ESP32 simulado no [roteiro de testes](TESTES.md). **Ainda falta gravar/publicar o vídeo, conferir os links e fazer a entrega pela equipe.**

## Preparar antes de gravar

1. Abrir o [Wokwi público](https://wokwi.com/projects/480416382701050881), o [dashboard](https://water-n-assurance-dashboard.onrender.com/), o [status da API](https://water-n-assurance.onrender.com/api/v1/status) e o [contexto real do Orion](https://water-n-assurance.onrender.com/api/v1/devices/vigia-sim-01/context).
2. Acordar o serviço gratuito e aguardar `fiware.connected: true`. Preparar as abas e o zoom para o texto ficar legível no vídeo.
3. Iniciar a simulação e conferir `[TEST] SUMMARY passed=69 failed=0 result=PASS`.
4. **Fora da gravação**, carregar `NET TOKEN <chave da sessão>` no Serial. Nunca mostrar o valor da chave, o ambiente do Render ou senhas. Reiniciar o Wokwi apaga a chave da memória.
5. Preparar a referência com os comandos abaixo, um por vez. Aguardar a estabilização e conferir `STATUS`:

```text
TEST NET ON
TEST FLOW 6
TEST OPTICAL 20
```

Não enviar `RESET` entre todos os cenários: demonstrar as transições, a persistência do aviso e a recuperação. Os tempos dos filtros usam o relógio da simulação; aguardar a confirmação antes de narrar o resultado. Cortes de espera são aceitáveis quando identificados, sem simular respostas que não aconteceram.

## Roteiro sugerido — 6 min 50 s

| Tempo | O que mostrar e explicar |
|---|---|
| 0:00–0:40 | **water n' assurance**, produto **Vigia d’Água**, cidadão beneficiado e ODS 6. A dúvida sobre alterações de cor/sabor na água de poço da avó em Iguatu/CE motivou a ideia; a causa não foi comprovada. Instalação conceitual próxima ao registro. Vila Prudente/SP é o recorte operacional inicial pela proximidade com a equipe. |
| 0:40–1:30 | Circuito organizado: ESP32; DS18B20 para temperatura; potenciômetros para índice óptico relativo, vazão e pressão; OLED; LEDs; buzzer; botão de reconhecimento e chave de falha. Explicar a função de cada componente e quais entradas representam sensores simulados. |
| 1:30–2:30 | Mostrar código: funções e constantes, aquisição com `millis()`, validação por canal, mediana de cinco amostras, três confirmações novas, faixas 30/60 e retornos 25/55. Explicar por que filtro/persistência evitam reagir a um pico. São limites didáticos, não sanitários. |
| 2:30–5:15 | Alterar entradas e narrar **todos os quatro estados**, mostrando OLED, palavra/símbolo/cor, som, falha, reconhecimento e recuperação. Usar a sequência abaixo. Mostrar um trecho do código que produz cada resposta. |
| 5:15–6:15 | Mostrar a mesma alteração no dashboard e no endpoint `/context`: `protocol: NGSI-v2`, `matches_current: true`, estado e índice correspondentes. A API `/current` informa `source: fiware-orion`. Explicar o caminho real e a continuidade da decisão local sem internet. |
| 6:15–6:50 | Desafios e soluções: ruído/picos, falhas sem falsa normalidade, transporte HTTPS e reconstrução de contexto no Render Free. Delimitar a simulação: não filtra, não identifica todos os contaminantes, não comprova potabilidade nem substitui laboratório. |

Reservar dez segundos de margem. A divisão acima é uma sugestão; o enunciado exige o conteúdo e o limite total de **sete minutos**.

### Sequência para a demonstração

| Entrada/ação | Resposta a conferir |
|---|---|
| `TEST OPTICAL 20` com fluxo 6 | **Sem alteração detectada**, verde, sem apresentar garantia de água potável. |
| `TEST OPTICAL 40` | Depois do filtro e das confirmações: **Atenção**, amarelo, símbolo e som. |
| Pressionar **Silenciar** | Reconhece o aviso sonoro, preservando o estado e o registro da alteração. Um segundo clique, enquanto a alteração continua confirmada, reativa o som. |
| `TEST OPTICAL 80` | **Verifique**, vermelho e resposta sonora. |
| `TEST FAULT OPTICAL ON` | **Leitura indisponível**, com o motivo; falha não aparece como normalidade. |
| `TEST FAULT OPTICAL OFF` e `TEST OPTICAL 20` | Renovação/estabilização e recuperação. Reconhecer o alarme pendente, se houver. |
| `TEST NET OFF` e uma mudança local | O aparelho continua avaliando e avisando. Se mostrar o painel, aguardar a idade superar 20 s: ele identifica comunicação antiga. |
| `TEST NET ON` | Retomada HTTPS, contexto sincronizado e painel atualizado, sem apagar o histórico. |

O [roteiro completo de testes](TESTES.md) inclui pico breve, ausência/retorno de fluxo, dados antigos e recuperação. Se mostrar um pico no vídeo, esperar o filtro renovar antes de afirmar que foi rejeitado. Apresentar somente cenários efetivamente executados.

## O que torna a integração verificável

O ESP32 calcula o estado local e envia HTTPS. A API grava no Postgres; a outbox sincroniza a entidade `WaterMonitor` no Orion via NGSI-v2. O dashboard consulta a API, que usa o contexto do Orion quando ele corresponde ao snapshot persistido. Se houver atraso do broker, a API conserva o valor persistido e informa a origem `postgres`.

Orion e Mongo executam internamente no mesmo serviço gratuito do Render. O Mongo guarda contexto reconstruível; o Postgres mantém o histórico. Uma reconstrução após reinício preserva a idade da última mensagem e não cria leitura recente. O [guia de infraestrutura](../../infra/fiware/README.md) permite reproduzir esse conjunto no Linux/WSL.

## Entrega

O enunciado de Edge, páginas 35–38 do documento CitizenTech, pede Wokwi público, README de descrição/componentes/lógica/execução e vídeo narrado no YouTube, público ou não listado, com até sete minutos. O envio da tarefa é feito por **um integrante**, incluindo os dois links e **todos os nomes/RMs**. O README está neste repositório público; incluir seu link na descrição do vídeo facilita a avaliação.

Na nova consulta ao Teams em 17/11/2026, a tarefa **GS - Edge Computing** mostrou **19/11/2026 às 23:59**. Conferir novamente na conta antes de enviar.

```text
water n' assurance — Vigia d’Água
GS — Edge Computing & Computer Systems | 1ESPV-2026

Wokwi: https://wokwi.com/projects/480416382701050881
YouTube: [inserir o link real após publicar e conferir]

Thiago Gomes Nascimento — RM569436
Rafael De Sousa — RM568777
Vitor Assis Florenzano — RM572192
Raul Moreira Andrade — RM571042
```

Antes do envio, assistir ao vídeo completo, conferir duração/áudio/legibilidade e abrir os links sem autenticação. A equipe realiza a submissão; nenhuma entrega foi enviada pelo assistente.
