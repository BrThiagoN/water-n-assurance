# Verificação da revisão 3

Em 12/11/2026, o sketch compilou e executou no ESP32 do Wokwi: **69 autotestes aprovados, zero falhas**: 58 do processamento local e da política do alarme, mais 11 dos encoders JSON. A lista de casos e o resumo registrado estão em [evidence/resultado-resumido.json](evidence/resultado-resumido.json).

## O que os autotestes cobrem

- Inicialização indisponível, dados inválidos, NaN, faixa inválida e leituras antigas.
- Mediana de cinco amostras, ruído impulsivo, confirmação com novas amostras, histerese e retorno gradual.
- Circulação, renovação da amostra e recuperação após interrupções.
- Falha de cada canal e preservação de dados contextuais sem esconder a indisponibilidade relevante.
- Alarme que permanece pendente sem timeout, reconhecimento, silêncio, reativação, escalada, nova ocorrência e reset.
- Codificação JSON dos dados e estados.

Os testes de encoder JSON não iniciam Wi-Fi nem exigem servidor. Os 69 casos são testes de software no simulador, não medições de água.

## Ensaio com o botão físico

Com entradas simuladas (índice óptico 40 e vazão de 6 L/min), a condição Atenção foi mantida por 44 amostras, cobrindo 43 segundos do relógio do ESP32. O firmware manteve alarm_latched=true e buzzer_on=true durante o intervalo observado. Depois:

1. O primeiro clique no botão GPIO 27 silenciou a ocorrência: muted=true, alarm_latched=false, buzzer_on=false.
2. O segundo clique, ainda com Atenção confirmada, reativou o som: muted=false, alarm_latched=true, buzzer_on=true.

Os registros e a contagem do ensaio estão no arquivo JSON resumido.

buzzer_on confirma o comando do firmware ao piezo. Não houve gravação com microfone ou medição do nível sonoro. A duração de 43 segundos é apenas o intervalo observado; a ausência de timeout também está coberta por autoteste e pelo estado do código.

## Limites

As entradas são sintéticas do Wokwi. Os testes avaliam o comportamento do programa diante de cenários definidos; não medem exatidão, seletividade, calibração, potabilidade ou desempenho de sensores físicos. Também não comprovam o caminho até um backend, que permanece sem configuração.
