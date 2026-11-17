# water n' assurance

**O Vigia d’Água é um monitor residencial da water n' assurance. O aparelho acompanha alguns parâmetros da água e apresenta avisos locais; a plataforma reúne o histórico para consulta.**

Projeto acadêmico desenvolvido para a **Global Solution CitizenTech 2026.2**, do curso de **Engenharia de Software da FIAP**, turma **1ESPV-2026**.

## Comece por aqui

| Quero... | Caminho |
|---|---|
| Experimentar o aparelho sem instalar ferramentas | Siga o teste rápido abaixo. |
| Criar minha própria cópia usando os arquivos do GitHub | Veja o [guia de reprodução do firmware](firmware/wokwi/README.md). |
| Repetir os estados, falhas e comandos | Abra o [roteiro de testes](firmware/wokwi/TESTES.md). |
| Continuar o backend no Linux | Siga o [guia de Python, Postgres, MQTT e FIWARE](docs/gsBackendLinux.md). |

**O que já pode ser executado:** a simulação no Wokwi, a [API Python tipada](backend/README.md) e o [dashboard React/TypeScript](dashboard/README.md). O backend e a interface ficam em pastas separadas. O servidor mantém histórico Postgres e CRUD de acompanhamentos. O Wokwi envia leituras ao Render por HTTPS; cada reinício da simulação exige carregar a chave de ingestão na sessão. MQTT e FIWARE têm configuração própria e seu estado é informado pela API. Veja o [registro de validação](docs/backend-validation.md).

## API e dashboard

`backend/` contém a API Python tipada e a persistência; `dashboard/`, a interface React/TypeScript. Para iniciar no WSL:

```bash
cd backend
python3 scripts/setup_env.py
sudo docker compose up -d --build --wait backend
```

Em outro terminal, com Node.js 24.15+ da linha 24:

```bash
cd dashboard
npm ci
npm run dev
```

Consulte `http://localhost:8000/healthz` para conferir a API e abra `http://localhost:5173` para usar o dashboard. As instruções completas e os testes estão nos READMEs de cada pasta. Sem dados, a interface informa que aguarda leituras; não cria valores fictícios.

## Teste rápido no Wokwi

Você precisa de um navegador com internet. A simulação pronta dispensa instalar Arduino IDE, Node.js ou Docker no computador.

1. Abra o [Vigia d’Água no Wokwi](https://wokwi.com/projects/480416382701050881).
2. Clique em **iniciar a simulação**, no botão com o triângulo de reprodução, e aguarde a compilação.
3. Abra o **Serial Monitor** e localize a linha de resumo dos autotestes. A revisão 3 tem como referência `[TEST] SUMMARY passed=69 failed=0 result=PASS`. Se houver `FAIL` ou erro de compilação, consulte o [guia de solução de problemas](firmware/wokwi/README.md).
4. No campo de entrada do Serial Monitor, envie os comandos abaixo **um por vez**, confirmando cada linha com Enter. Não digite esses comandos no terminal do Linux.

```text
RESET
TEST NET OFF
TEST FLOW 6
TEST OPTICAL 20
```

5. Aguarde cerca de **15 segundos de simulação**, mantendo a chave de falha na posição sem falha, e envie:

```text
STATUS
```

**Resultado esperado:** OLED com “Sem alteração detectada”, LED verde e JSON no Serial contendo `"state":"NO_CHANGE"` e `"assessment_available":true`. Mensagens de inicialização, renovação da amostra e confirmação são esperadas antes de estabilizar.

O indicador `OFF` da rede é esperado neste teste: a conexão foi desativada para observar o comportamento local. Para enviar ao Render, abra o [dashboard publicado](https://water-n-assurance-dashboard.onrender.com/) e siga a seção **Enviar leituras ao dashboard do Render** do [guia do firmware](firmware/wokwi/README.md). A chave `INGEST_TOKEN` é carregada pelo Serial somente na sessão; não deve ser colocada no código público.

### Faça os primeiros experimentos

Partindo do teste rápido acima, envie cada comando, aguarde a estabilização e use `STATUS` para conferir:

| Experimento | Comando no Serial | O que deve acontecer |
|---|---|---|
| Mudança moderada persistente | `TEST OPTICAL 40` | Após a confirmação, **Atenção**, LED amarelo e aviso sonoro. |
| Mudança mais intensa persistente | `TEST OPTICAL 80` | Após a confirmação, **Verifique**, LED vermelho e aviso sonoro. |
| Falha na entrada óptica | `TEST FAULT OPTICAL ON` | **Leitura indisponível**, com motivo de falha. |
| Recuperação dessa entrada | `TEST FAULT OPTICAL OFF` | Nova estabilização e retorno ao estado correspondente à entrada ainda selecionada. |
| Voltar a uma referência baixa | `TEST OPTICAL 20` | Retorno a **Sem alteração detectada** após filtro e confirmações; um alarme pendente continua exigindo reconhecimento. |

Para as alterações ópticas, reserve cerca de 10 segundos de simulação; para recuperação e retorno à referência, reserve cerca de 15. Esses tempos são margens para o roteiro, não prazos garantidos nem limites sanitários.

Pressione o botão **Silenciar** do circuito para reconhecer o aviso. Enquanto a alteração continuar confirmada, um segundo clique reativa o som. O comando `MUTE` apenas silencia; ele não substitui esse teste dos dois cliques.

Ao terminar, envie `TEST OPTICAL OFF` e `TEST FLOW OFF` para devolver o controle aos potenciômetros. O [roteiro completo](firmware/wokwi/TESTES.md) explica preparação, resultados e recuperação de cada cenário.

## Clonar e continuar no Linux

Para consultar ou editar os arquivos localmente, com Git instalado:

```bash
git clone https://github.com/BrThiagoN/water-n-assurance.git
cd water-n-assurance
```

| Arquivo ou pasta | Para que serve |
|---|---|
| [firmware/wokwi/](firmware/wokwi/README.md) | Código, circuito e instruções para reconstruir a simulação. |
| [firmware/wokwi/libraries.txt](firmware/wokwi/libraries.txt) | Bibliotecas utilizadas pelo projeto no Wokwi. |
| [firmware/wokwi/TESTES.md](firmware/wokwi/TESTES.md) | Comandos de teste, critérios de conferência e registro anterior da revisão 3. |
| [docs/gsBackendLinux.md](docs/gsBackendLinux.md) | Arquitetura e ordem de implementação do backend no Linux. |
| [backend/README.md](backend/README.md) | Executar a API Python, conferir a tipagem e os testes e configurar o Render. |
| [render.yaml](render.yaml) | Blueprint com Web Service e Postgres no plano Free do Render. |

Clonar o repo baixa os arquivos. O firmware continua sendo executado pelo Wokwi no navegador, sem configuração pronta de Arduino CLI/PlatformIO. Para iniciar o backend, seguir [backend/README.md](backend/README.md); há execução por Docker Compose e por Python 3.12 com Postgres. A interface é iniciada separadamente pelo [guia do dashboard](dashboard/README.md).

No Render, o histórico usa Postgres, pois arquivos locais do serviço gratuito são temporários. O serviço Free suspende por inatividade e o banco Free expira após 30 dias; conferir esses limites antes da demonstração. Uma montagem com sensores físicos e água também exigirá desenvolvimento e validação além desta simulação.

## Ficha da CitizenTech

| Campo | Definição |
|---|---|
| Companhia | **water n' assurance** |
| Produto | **Vigia d’Água** |
| Cidade e bairro do MVP inicial | **Vila Prudente, São Paulo — SP**, escolhido pela proximidade geográfica com a equipe para facilitar a aplicação e o acompanhamento do MVP. |
| Caso real motivador | Relato familiar sobre alterações na água de poço em **Iguatu — CE**. |
| Cidadão beneficiado | Moradores de casas e apartamentos interessados em acompanhar a água da própria residência. |
| Problema | Dificuldade para perceber alterações na água e reunir informações compreensíveis para solicitar uma avaliação. |
| Como a solução funcionará | Um dispositivo próximo ao registro acompanhará alguns parâmetros, apresentará avisos locais e disponibilizará registros em uma plataforma. |
| Papel de cada parte | Sensores ou entradas simuladas fornecem dados; o ESP32 valida, interpreta e sinaliza; o backend registra; a aplicação web apresenta o histórico e os acompanhamentos; Python analisa séries e DPS explora os modelos matemáticos. |
| Indicador de impacto proposto | Compreensão dos avisos sem ajuda e capacidade de consultar um evento e obter seus registros. Ainda sem resultados de testes com usuários. |
| ODS principal | **ODS 6 — Água potável e saneamento**, com foco em acesso à informação e acompanhamento das condições da água. |
| Repositório | [BrThiagoN/water-n-assurance](https://github.com/BrThiagoN/water-n-assurance) |

## A companhia e seu primeiro produto

A **water n' assurance** tem como propósito tornar informações sobre a água mais acessíveis para que as pessoas possam acompanhar alterações e buscar providências com mais contexto.

O **Vigia d’Água** é o primeiro produto proposto pela companhia. Sua experiência principal acontece no próprio dispositivo: o morador deve conseguir olhar para o aparelho e reconhecer um aviso, mesmo sem conhecimento técnico e sem precisar abrir um celular.

A plataforma digital complementará essa experiência com detalhes das leituras, histórico do ponto monitorado, exportação de registros e acompanhamento das providências registradas pelo morador.

O primeiro público é individual: moradores que desejam acompanhar a água da própria casa ou apartamento. A expansão pretendida passa por condomínios e construtoras, com companhias de água como possibilidade posterior. Essa estratégia ainda precisa de validação; não há parcerias comerciais anunciadas.

## Problema e motivação

A ideia foi fortemente motivada por um caso familiar relatado à equipe: uma moradora de **Iguatu — CE** que utiliza água de poço já percebeu episódios de forte alteração de cor e sabor. A causa dessas alterações não foi confirmada por análises laboratoriais.

**Vila Prudente, São Paulo — SP**, é o local planejado para facilitar a aplicação inicial do MVP pela proximidade geográfica com a equipe. Essa escolha organiza a execução e o acompanhamento do projeto; a proposta da companhia atende moradores de diferentes localidades.

A investigação do projeto procura entender como o morador pode perceber mudanças, consultar seu contexto e reunir registros para solicitar uma avaliação, sem depender apenas de lembranças ou relatos dispersos.

### Pesquisa exploratória

Na análise das primeiras **18 respostas**, consultadas em **10/11/2026**:

| Resultado | Contagem | Percentual |
|---|---:|---:|
| Não sabem onde consultar análises da água | 13 de 18 | 72,2% |
| Perceberam alterações na água nos últimos 12 meses | 5 de 18 | 27,8% |
| Escolheram receber alertas como recurso mais útil | 10 de 18 | 55,6% |

Fonte: pesquisa exploratória da equipe realizada por Google Forms. A coleta está em andamento e a amostra não tem cidade registrada, portanto os resultados não representam a população de São Paulo. Os dados descrevem percepções e preferências; não medem potabilidade nem validam a compreensão da interface.

## Escopo inicial do Vigia d’Água

O MVP acadêmico planejado contempla **um ponto residencial simulado**.

- Acompanhar temperatura, sinal óptico relativo, vazão e pressão.
- Validar e suavizar leituras antes de classificar alterações.
- Apresentar avisos locais com **palavra, símbolo e cor**.
- Manter a decisão e o aviso do aparelho durante indisponibilidade de internet.
- Registrar eventos com ponto, valores e contexto de circulação; o protótipo usa tempo de operação, sem horário de calendário sincronizado.
- Consultar estado, histórico e detalhes na plataforma.
- Exportar registros por evento ou período.
- Criar, consultar, atualizar e excluir acompanhamentos de demonstração, preservando os eventos e as leituras originais.

### Parâmetros e limites de interpretação

| Dado | Representação no Wokwi | Papel e limite |
|---|---|---|
| Temperatura | DS18B20 simulado, em °C | Acompanha a temperatura no ponto monitorado. Isoladamente, não comprova contaminação. |
| Sinal óptico | Potenciômetro convertido em índice relativo de 0 a 100 | É a entrada usada para classificar alterações na versão atual. Não representa NTU, concentração de contaminantes ou resultado sanitário. |
| Vazão | Potenciômetro convertido em 0–20 L/min | Indica circulação e condiciona a avaliação da amostra. Não comprova a qualidade da água. |
| Pressão | Potenciômetro convertido em 0–6 bar | Registra o contexto do abastecimento. Não é usada para estimar o nível da caixa nem para classificar potabilidade. |

**O Vigia d’Água não filtra a água, não identifica todos os contaminantes e não substitui análises laboratoriais de potabilidade.** A leitura representa o ponto monitorado, sem garantir as condições de todas as torneiras, do reservatório ou da rede pública.

Filtragem, dosagem de produtos, fechamento automático do registro e emissão de laudos ficam fora do MVP.

## Estudos que orientam o projeto

A etapa atual é um protótipo simulado. Usamos pesquisas publicadas para escolher os parâmetros, conhecer limitações dos sensores e planejar os testes do software. Não realizamos ensaios com passagem de água, calibração de sondas físicas ou comparação com um instrumento de referência.

| Trabalho | Resultado publicado | O que aproveitamos no Vigia d’Água |
|---|---|---|
| [Camargo et al. (2023)](https://doi.org/10.3390/s23094424), revisão sistemática | Dos **142 trabalhos** selecionados, apenas **18** compararam os sensores com instrumentos de referência; os outros 124 não incluíram essa comparação ou não discutiram a validação. | Distinguir funcionamento do sistema de precisão da medição e registrar o que foi efetivamente testado. |
| [Gillett e Marchiori (2019)](https://doi.org/10.3390/s19143039), monitor contínuo de turbidez | Os autores relataram acurácia de aproximadamente **1 NTU na faixa de 0–100 NTU** para o instrumento que construíram, em laboratório. Um ensaio de **38 dias** identificou ruído associado a luz ambiente e bolhas na tubulação. | Incluir ruído, picos e mudanças persistentes nos cenários de teste. Esse resultado pertence ao instrumento do estudo, não ao nosso protótipo. |
| [Xavier et al. (2022)](https://sol.sbc.org.br/index.php/sbesc_estendido/article/download/22842/22665), UTFPR e UNIOESTE | Sensores de baixo custo produziram leituras próximas às referências em condições controladas, mas apresentaram dificuldades adicionais em ambiente real. | Tratar a instalação e a validação física como etapas próprias; o Wokwi não reproduz essas condições. |
| [Naloufi et al. (2024)](https://doi.org/10.3390/w16121708), estabilidade de sensores | O sensor de temperatura apresentou bom desempenho. O modelo de turbidez avaliado mostrou instabilidade e sensibilidade à luz externa. Os autores também compararam métodos de remoção de valores discrepantes. | Comparar filtros e acompanhar a qualidade de cada canal. Os resultados dependem do sensor, da instalação e dos dados usados no estudo. |
| [Silva (2023)](https://repositorio.ifpb.edu.br/handle/177683/3156), TCC do IFPB | Desenvolveu uma plataforma de monitoramento com sensores, ESP32, MQTT e visualização, discutindo calibração e ameaças à validade dos resultados. | Usar como referência de integração e documentação acadêmica, mantendo a decisão do nosso aparelho no ESP32. |

Esses trabalhos não fornecem uma calibração pronta para o nosso aparelho. Uma equação ou um erro de medição publicado só pode ser entendido junto ao sensor, à montagem e às condições do experimento. Também não adotamos limites sanitários de rios ou aquários como critérios para uma residência.

### Dados públicos para avaliar o algoritmo

Selecionamos o [GECCO Industrial Challenge 2019, versão 2](https://doi.org/10.5281/zenodo.4304080) como base para uma avaliação futura. O conjunto reúne dados reais fornecidos pela Thüringer Fernwasserversorgung e pelo projeto IMProvT para uma competição de detecção de eventos na qualidade da água. Os arquivos incluem temperatura, turbidez e uma coluna de eventos, além de outros parâmetros.

A base oferece conjuntos de treino, teste e validação em ordem cronológica. Isso permite ajustar o algoritmo em uma parte e avaliá-lo em outra. **Ainda não executamos o algoritmo do Vigia d’Água nessa base.** O uso deverá respeitar as unidades e os intervalos originais; nem todo evento envolve os parâmetros acompanhados pelo nosso dispositivo. Vazão e pressão continuarão sendo cenários simulados quando não houver dados compatíveis.

### Avaliação da interpretação dos dados

| Ensaio | O que observar |
|---|---|
| Sinal estável e ruído | Quantos alertas surgem sem uma mudança persistente. |
| Pico isolado e mudança breve | Se o filtro reduz interferências sem esconder eventos que queremos registrar. |
| Mudança persistente e retorno | Tempo de resposta, confirmação do estado e comportamento da histerese. |
| Falha ou dado antigo por canal | Se o aparelho identifica a limitação sem apresentar ausência de leitura como normalidade. |
| Parada e retomada de circulação | Como a avaliação é suspensa e retomada, incluindo observações durante a renovação da amostra. |
| Séries públicas compatíveis | Desempenho fora dos cenários usados para ajustar o algoritmo. |

A revisão 2 comparou média e mediana no mesmo motor C++, com cenários sintéticos. Em uma referência estável de 20 seguida de uma amostra 100, a média de cinco pontos chegou a 36 e a mediana permaneceu em 20. Com a nova regra que exige apoio da amostra bruta em cada confirmação, ambos os modos retornaram sem alerta confirmado após esse pico. A configuração do aparelho passou a usar **mediana de cinco amostras e três confirmações novas**, apoiadas pelo sinal bruto e pelo filtrado.

Uma mudança ainda não confirmada fica identificada como observação provisória, inclusive durante a retomada do fluxo. Falhas de temperatura e pressão são informadas por canal, preservando os outros valores; a classificação óptica exige óptica e vazão utilizáveis. Os ensaios interativos da revisão 2 verificaram esses comportamentos, além de dado antigo, recuperação, silêncio e reset. A avaliação com a base pública e a ampliação dos cenários de ruído continuam pendentes.

Os limites **30 e 60** do índice óptico são parâmetros didáticos de demonstração; não foram estabelecidos pelos artigos e não correspondem a valores em NTU. A comparação controlada dos filtros não demonstra superioridade em toda condição de uso.

A avaliação desta etapa cobre o comportamento do software em condições declaradas. A precisão das sondas físicas e o desempenho em uma instalação hidráulica ficam para uma etapa posterior.

## Estados do aparelho

| Estado | Mensagem de apoio | Indicação no protótipo |
|---|---|---|
| Sem alteração detectada | Nos parâmetros acompanhados | Palavra, símbolo de confirmação e verde. |
| Atenção | Mudança em observação | Palavra, triângulo e amarelo. |
| Verifique | Mudança persistente | Palavra, exclamação e vermelho. |
| Leitura indisponível | Motivo da suspensão ou confirmação em andamento | Palavra, símbolo e indicação neutra intermitente. |

Os significados também serão mantidos na plataforma. Texto e símbolo acompanham a cor. Falta de leitura não aparece como normalidade. Durante a confirmação de uma mudança, o aparelho distingue o sinal provisório do último estado confirmado.

## Arquitetura e integração

```text
Sensores ou entradas simuladas
              |
              v
 ESP32: validação, suavização e decisão
        |                      |
        v                      v
   Aviso local          Backend IoT / API
                               |
                               v
                       Plataforma web
                 histórico e acompanhamentos
```

| Parte | Responsabilidade | Tecnologia prevista |
|---|---|---|
| Dispositivo | Ler entradas, validar, suavizar, classificar e sinalizar localmente. | ESP32 e simulação no Wokwi. |
| Backend IoT | Receber e disponibilizar as leituras do dispositivo. | Python 3.12, FastAPI, Pydantic, SQLAlchemy tipado e Postgres. Entrada HTTPS; MQTT e FIWARE configuráveis. |
| API de acompanhamentos | Persistir e disponibilizar operações de CRUD. | API deste backend, com token administrativo separado. |
| Plataforma web | Apresentar o ponto, o histórico, os eventos e as providências. | React, TypeScript, Vite e Tailwind CSS, em `dashboard/`. |
| Aplicação Python | Processar dados, organizar consultas e persistir registros de demonstração. | Python e arquivos JSON. |
| Modelagem matemática | Estudar comportamento, taxas de variação e grandezas acumuladas nos cenários simulados. | Python ou GeoGebra, conforme a avaliação de DPS. |

A validação, a interpretação e os avisos do aparelho são locais. O backend recebe os valores e o estado já calculado pelo ESP32; sua indisponibilidade não deve interromper a avaliação. A aplicação Python fará análises separadas da decisão em tempo real.

O protótipo está disponível no [Wokwi](https://wokwi.com/projects/480416382701050881), e o circuito e o firmware estão espelhados em [firmware/wokwi/](firmware/wokwi/README.md). A API fica em [backend/](backend/README.md), sem arquivos de interface. O [dashboard](dashboard/README.md) fica em sua própria pasta na raiz, com as fontes e a paleta dos slides, consulta de histórico e CRUD de acompanhamentos. A entrega acadêmica de Front-end/Web ainda exige a conferência dos demais requisitos, documentação, acesso e histórico mínimo de commits.

## Estado do projeto

| Frente | Situação registrada em 17/11/2026 |
|---|---|
| Identidade da companhia e do produto | Definida. |
| Pesquisa exploratória | Primeiras 18 respostas analisadas; coleta em andamento. |
| Software & Total Experience Design | Apresentação de 16 slides preparada e revisada fora deste repositório. |
| Storytelling e Inspiração Empreendedora | A desenvolver. |
| Edge Computing & Computer Systems | Firmware com 69 autotestes aprovados na nova compilação de 17/11; transporte HTTPS ao Render confirmado. API Python tipada, Postgres e dashboard separado publicados. A demonstração FIWARE/IoT Cloud, o vídeo e a avaliação com dados públicos continuam pendentes. |
| Computational Thinking with Python | A implementar. |
| Differentiated Problem Solving | Modelagem e relatório a desenvolver conforme as questões da avaliação. |
| Front-end Design e Web Development | Dashboard React/TypeScript com histórico e CRUD implementado. Conferir os requisitos restantes e preparar as entregas separadas. |

## Reprodução e validação

O [guia do firmware](firmware/wokwi/README.md) explica os nove arquivos necessários, as bibliotecas, o circuito e como reconstruir o projeto no Wokwi. O [roteiro de testes](firmware/wokwi/TESTES.md) detalha os cenários e diferencia resultados esperados dos resultados registrados anteriormente.

Em **17/11/2026**, a versão com HTTPS compilou no Wokwi e repetiu os **69 autotestes, sem falhas**. Leituras produzidas pelo ESP32 simulado foram aceitas pela API publicada e persistidas no Postgres. O [registro de validação](docs/backend-validation.md) distingue essa integração dos testes anteriores e dos cenários ainda pendentes.

O firmware envia `telemetry` e `event` por HTTPS à API Python do Render, com filas, retentativas e confirmação de aceitação. A decisão e o alarme funcionam localmente mesmo sem internet. O [guia de backend para Linux](docs/gsBackendLinux.md) explica o transporte e as integrações adicionais.

## Próximas etapas

1. Dividir as frentes de trabalho e combinar formato de dados, unidades, identificadores e estados.
2. Preparar o documento de marca, os slides e o videopitch de Storytelling.
3. Ampliar os cenários de ruído e mudanças breves, mantendo os resultados de cada revisão do firmware.
4. Usar o caminho ESP32 → API Render → Postgres → painel já validado como base para o vídeo; demonstrar a camada FIWARE/IoT Cloud escolhida conforme o enunciado de Edge.
5. Conferir a aplicação web, complementar os requisitos acadêmicos e manter um histórico real de commits significativos.
6. Desenvolver o programa Python e a análise matemática de DPS em paralelo.
7. Avaliar as séries públicas compatíveis, consolidar os registros e finalizar as entregas de cada disciplina.

Testes de compreensão com moradores, ensaios com sensores físicos e seleção de um local para piloto fazem parte da continuidade planejada. Não são apresentados como atividades já realizadas.

## Referências

1. CAMARGO, E. T. et al. **Low-Cost Water Quality Sensors for IoT: A Systematic Review**. *Sensors*, v. 23, n. 9, artigo 4424, 2023. [DOI: 10.3390/s23094424](https://doi.org/10.3390/s23094424).
2. GILLETT, D.; MARCHIORI, A. **A Low-Cost Continuous Turbidity Monitor**. *Sensors*, v. 19, n. 14, artigo 3039, 2019. [DOI: 10.3390/s19143039](https://doi.org/10.3390/s19143039).
3. XAVIER, F. N. C. et al. **Evaluation of low-cost sensors for real-time water quality monitoring**. *Anais Estendidos do XII Simpósio Brasileiro de Engenharia de Sistemas Computacionais*, p. 56–61, 2022. [Texto no portal da Sociedade Brasileira de Computação](https://sol.sbc.org.br/index.php/sbesc_estendido/article/download/22842/22665).
4. NALOUFI, M. et al. **Long-Term Stability of Low-Cost IoT System for Monitoring Water Quality in Urban Rivers**. *Water*, v. 16, n. 12, artigo 1708, 2024. [DOI: 10.3390/w16121708](https://doi.org/10.3390/w16121708).
5. SILVA, M. P. B. da. **Monitoramento de parâmetros de qualidade da água com plataforma desenvolvida com Arduino**. Trabalho de Conclusão de Curso (Tecnologia em Telemática), Instituto Federal da Paraíba, Campina Grande, 2023. [Repositório do IFPB](https://repositorio.ifpb.edu.br/handle/177683/3156).
6. MORITZ, S.; REHBACH, F.; BARTZ-BEIELSTEIN, T. **GECCO Industrial Challenge 2019 Dataset**. Versão 2. Zenodo, 2019. [DOI: 10.5281/zenodo.4304080](https://doi.org/10.5281/zenodo.4304080).

Referências consultadas em 12/11/2026.

## Equipe

| Integrante | RM |
|---|---|
| Thiago Gomes Nascimento | RM569436 |
| Rafael De Sousa | RM568777 |
| Vitor Assis Florenzano | RM572192 |
| Raul Moreira Andrade | RM571042 |

## Documentação do uso de IA

Usamos o Codex para ajudar na organização das ideias, na pesquisa de referências, na documentação e na implementação do firmware, do backend e dos testes. Cabe à nossa equipe conferir o conteúdo, entender o código e revisar as escolhas feitas. O registro desse apoio acompanha a documentação de cada matéria.
