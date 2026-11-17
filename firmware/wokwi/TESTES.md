# Vigia d’Água — roteiro de reprodução e testes

> Revisão documental: **14/11/2026**.
> Os cenários abaixo foram conferidos contra os comandos e as regras do firmware. **Esta atualização não executou novamente o Wokwi.** “Esperado” descreve o comportamento a conferir; resultados de uma execução devem ser registrados ao final.

## 1. Abrir e preparar

1. Abra o [projeto Wokwi](https://wokwi.com/projects/480416382701050881), ou sua cópia recriada pelo [guia do firmware](README.md).
2. Inicie a simulação, aguarde a compilação e confira o resumo dos autotestes.
3. Envie os comandos no **Serial Monitor do Wokwi**, uma linha por vez com Enter. Eles não são comandos Bash.
4. Mantenha a chave de falha do circuito na posição sem falha.

### Preparação comum

Repetir esta preparação quando um cenário pedir “voltar à base”:

```text
RESET
TEST NET OFF
TEST FLOW 6
TEST OPTICAL 20
```

Aguarde aproximadamente **15 segundos de simulação** e envie `STATUS`.

Base esperada: `state=NO_CHANGE`, `assessment_available=true`, `flow_context=FLOWING`, LED verde e nenhum alarme pendente. O JSON usa aspas e dois-pontos; as expressões com `=` neste guia são apenas uma forma curta de indicar campos e valores.

Inicialização, renovação da amostra, formação do filtro e confirmação antecedem a avaliação. Se a simulação estiver lenta, acompanhe seu relógio e as mensagens; os tempos do roteiro são margens de espera, não uma medição de desempenho.

## 2. Quatro estados e recuperação

| Cenário | Preparação e ação | Resultado esperado |
|---|---|---|
| **Sem alteração detectada** | Voltar à base e enviar `STATUS`. | `state=NO_CHANGE`, avaliação disponível, LED verde. |
| **Atenção** | Da base, enviar `TEST OPTICAL 40`, esperar cerca de 10 s simulados e consultar `STATUS`. | `state=ATTENTION`, LED amarelo e aviso sonoro pendente de reconhecimento. |
| **Verifique** | Da base, enviar `TEST OPTICAL 80`, esperar cerca de 10 s e consultar `STATUS`. | `state=CHECK`, LED vermelho e aviso sonoro pendente de reconhecimento. |
| **Leitura indisponível por falha** | Da base, enviar `TEST FAULT OPTICAL ON`, aguardar novas atualizações e consultar `STATUS`. | `state=UNAVAILABLE`, `reason=SENSOR_INVALID`, canal óptico inválido. |
| **Recuperação da falha** | Após a falha anterior, enviar `TEST FAULT OPTICAL OFF`, esperar cerca de 15 s e consultar `STATUS`. | Recomeço da estabilização e retorno a `NO_CHANGE`, pois a referência óptica continua em 20. |

A cor é acompanhada de texto e símbolo. “Sem alteração detectada” se refere aos parâmetros monitorados; não significa laudo de potabilidade. O índice 0–100 é relativo e não é NTU.

## 3. Alarme contínuo, reconhecimento e reativação

### 3.1. Conferir que normalizar não reconhece uma ocorrência

1. Voltar à base.
2. Enviar `TEST OPTICAL 80` e esperar a confirmação de `CHECK`.
3. Sem silenciar, enviar `TEST OPTICAL 20`.
4. Aguardar cerca de 15 s e consultar `STATUS`.

Esperado: `state=NO_CHANGE`, mas `alarm_latched=true` e `buzzer_on=true` permanecem enquanto o aviso não for reconhecido. O OLED deve indicar aviso pendente.

Enviar `MUTE` e consultar novamente: o som deve parar e a ocorrência pendente deve ser reconhecida. O comando não altera o valor óptico.

### 3.2. Conferir os dois cliques do botão físico

1. Voltar à base e confirmar `CHECK` com `TEST OPTICAL 80`.
2. Pressionar uma vez o botão **Silenciar** do circuito.
3. Consultar `STATUS`: som desligado e `muted=true`; o estado da água continua `CHECK`.
4. Sem normalizar a entrada, pressionar o botão novamente.
5. Consultar `STATUS`: o aviso sonoro deve voltar.

O comando `MUTE` só silencia; não usar esse comando para substituir o segundo clique. Com a leitura normalizada, um clique deixa o som preparado para próximos avisos, sem recriar uma ocorrência antiga.

## 4. Falhas por canal e dados antigos

### 4.1. Falha de temperatura

Da base:

```text
TEST FAULT TEMP ON
```

Consultar `STATUS` após novas amostras. Esperado: temperatura inválida e `valid=false` no conjunto, mas avaliação óptica ainda disponível e `state=NO_CHANGE`, com óptica e vazão válidas.

Restaurar com:

```text
TEST FAULT TEMP OFF
```

A mesma distinção entre falha contextual e avaliação óptica se aplica ao teste de pressão com `TEST FAULT PRESSURE ON` e `TEST FAULT PRESSURE OFF`.

### 4.2. Falha de vazão

Da base, enviar `TEST FAULT FLOW ON`. Esperado: avaliação indisponível por falha técnica. Restaurar com `TEST FAULT FLOW OFF` e aguardar nova estabilização.

### 4.3. Interromper a atualização das leituras

Da base:

```text
TEST STALE ON
```

Consultar `STATUS` depois de **mais de 11 segundos simulados**. Esperado: `state=UNAVAILABLE`, `reason=SENSOR_STALE` e canais antes válidos marcados como antigos.

Antes desse prazo, a avaliação já pode estar indisponível por confirmação/ausência de lote novo. Isso não deve ser apresentado como normalidade.

Retomar com `TEST STALE OFF`, aguardar cerca de 15 s e conferir o retorno à base.

## 5. Circulação e filtro

### 5.1. Parar e retomar o fluxo

Da base, enviar:

```text
TEST FLOW 0
```

Após nova aquisição, consultar `STATUS`. Esperado: `state=UNAVAILABLE`, `reason=NO_FLOW`, sem transformar falta de circulação em leitura boa.

Retomar com `TEST FLOW 6` e aguardar cerca de 15 s. A avaliação passa novamente pela renovação da amostra e pelo filtro antes de voltar à base.

### 5.2. Pico isolado

Voltar à base e enviar:

```text
TEST PULSE 100 1
```

Observar o Serial durante o pico e consultar `STATUS` após cerca de 5 s. Esperado: pode haver indicação provisória durante a amostra alta, mas não uma alteração persistente confirmada com alarme. A referência deve voltar a `NO_CHANGE`.

O pico usa a contagem de amostras, não segundos de relógio. A base óptica fixa em 20 deve continuar ativa antes desse cenário.

## 6. Rede desligada e limites atuais

1. Voltar à base; a preparação já envia `TEST NET OFF`.
2. Enviar `TEST OPTICAL 80`, esperar a confirmação e consultar `STATUS`.
3. Conferir que o estado e o aviso local mudam mesmo com `network=OFFLINE`.
4. Enviar `TEST NET ON`.

O último comando habilita as tentativas de conexão. O envio exige carregar `INGEST_TOKEN` com `NET TOKEN <chave>` no Serial, conforme o [guia do firmware](README.md). A credencial não deve constar no código nem no registro do teste.

Para verificar a integração, aguarde a confirmação HTTPS da API após gravação no **Postgres**. No [dashboard do Render](https://water-n-assurance-dashboard.onrender.com/), confira o horário de recebimento e altere `TEST OPTICAL 40`, depois `80`, aguardando filtro e confirmação entre cenários. Confira a falha óptica e a volta para `20`. Desative a rede e confirme que a comunicação fica antiga sem apresentar o último valor como atual; reative e confira a recuperação. Registre boot, sequência, estados e horários sem registrar tokens. FIWARE é uma integração adicional: a confirmação HTTPS não comprova sua execução.

## 7. Entradas incorretas e limpeza

Da base, experimentar uma linha por vez:

```text
TEST OPTICAL 120
TEST FLOW -1
TEST PULSE 100 11
```

Esperado: mensagens `[CMD] ERRO` e preservação da configuração válida anterior. Os intervalos aceitos são óptica 0–100, vazão 0–20 e pulsos com 1–10 amostras inteiras.

Ao finalizar, remover modos de teste e devolver controles aos componentes:

```text
RESET
TEST NET OFF
```

A partir daí, o estado depende dos valores dos potenciômetros e da chave física. Para uma comparação controlada, repetir a preparação comum.

## 8. Resultado registrado anteriormente

O README anterior registra uma execução da revisão 3 no Wokwi em **12/11/2026** com:

- **69 autotestes aprovados e zero falhas**: 58 do motor/política do alarme e 11 dos encoders JSON.
- Ensaios interativos de pulsos, mudanças persistentes, falhas, falta de atualização, circulação, silêncio/reativação e reset.
- Um trecho de alarme ativo abrangendo 44 amostras e 43 segundos do relógio do ESP32, sem medição acústica.

Fonte: [README no commit 4438861](https://github.com/BrThiagoN/water-n-assurance/blob/44388615920890d68deb67e4eb6bde132da8da94/README.md). Esse registro foi preservado da documentação anterior; esta atualização não repetiu a execução nem acrescenta logs brutos daquela sessão. Não usar a duração relatada como ensaio de durabilidade.

Os autotestes rodam no início com `Config::RUN_SELF_TESTS=true`. Se as dependências ou o código mudarem, conferir o resumo da nova execução; não copiar o resultado anterior como se fosse uma validação nova.

## 9. Como registrar sua própria reprodução

Preencher depois de executar, anexando a saída do Serial e capturas necessárias:

| Item | Registro da execução |
|---|---|
| Data | A preencher. |
| Pessoa que executou | A preencher. |
| Commit do GitHub | A preencher. |
| Link da cópia no Wokwi | A preencher. |
| Versões das bibliotecas/core disponíveis | A preencher. |
| Resumo dos autotestes | Copiar a linha real do Serial. |
| Cenários executados | Listar quais seções foram repetidas. |
| Resultados e diferenças | Descrever observações, sem substituir pelo esperado. |
| Evidências | Links ou caminhos de logs/capturas versionados. |
| Rede/backend | Informar se houve apenas teste local ou integração real. |

[Voltar ao guia do firmware](README.md) · [Voltar ao README principal](../../README.md)
