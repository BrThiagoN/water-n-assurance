# water n' assurance

**water-n-assurance é o repositório da companhia water n' assurance. Nosso primeiro produto é o Vigia d’Água, um monitor residencial que propõe avisar sobre alterações na água de forma compreensível no próprio aparelho.**

Projeto acadêmico desenvolvido para a **Global Solution CitizenTech 2026.2**, do curso de **Engenharia de Software da FIAP**, turma **1ESPV-2026**.

## Ficha da CitizenTech

| Campo | Definição |
|---|---|
| Companhia | **water n' assurance** |
| Produto | **Vigia d’Água** |
| Cidade de implantação inicial planejada | São Paulo — SP |
| Bairro de referência | A definir pela equipe; ainda não há piloto físico selecionado. |
| Cidadão beneficiado | Moradores de casas e apartamentos interessados em acompanhar a água da própria residência. |
| Problema | Dificuldade para perceber alterações na água e reunir informações compreensíveis para solicitar uma avaliação. |
| Como a solução funcionará | Um dispositivo próximo ao registro acompanhará alguns parâmetros, apresentará avisos locais e disponibilizará registros em uma plataforma. |
| Papel de cada parte | Sensores ou entradas simuladas fornecem dados; o ESP32 valida e sinaliza; o backend registra; a aplicação web apresenta o histórico e os acompanhamentos; Python processa dados e DPS explora os modelos matemáticos. |
| Indicador de impacto proposto | Compreensão dos avisos sem ajuda e capacidade de consultar um evento e obter seus registros. Ainda sem resultados de testes com usuários. |
| ODS principal | **ODS 6 — Água potável e saneamento**, com foco em acesso à informação e acompanhamento das condições da água. |
| Repositório | [BrThiagoN/water-n-assurance](https://github.com/BrThiagoN/water-n-assurance) |

## A companhia e seu primeiro produto

A **water n' assurance** tem como propósito tornar informações sobre a água mais acessíveis para que as pessoas possam acompanhar alterações e buscar providências com mais contexto.

O **Vigia d’Água** é o primeiro produto proposto pela companhia. Sua experiência principal acontece no próprio dispositivo: o morador deve conseguir olhar para o aparelho e reconhecer um aviso, mesmo sem conhecimento técnico e sem precisar abrir um celular.

A plataforma digital complementará essa experiência com detalhes das leituras, histórico do ponto monitorado, exportação de registros e acompanhamento das providências registradas pelo morador.

O primeiro público é individual: moradores que desejam acompanhar a água da própria casa ou apartamento. A expansão pretendida passa por condomínios e construtoras, com companhias de água como possibilidade posterior. Essa estratégia ainda precisa de validação; não há parcerias comerciais anunciadas.

## Problema e motivação

A ideia inclui um caso familiar relatado à equipe: uma moradora de Iguatu que utiliza água de poço já percebeu episódios de forte alteração de cor e sabor. A causa dessas alterações não foi confirmada por análises laboratoriais. O caso motivador é distinto da implantação inicial planejada em São Paulo.

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
- Registrar eventos com horário, ponto, valores e contexto de circulação.
- Consultar estado, histórico e detalhes na plataforma.
- Exportar registros por evento ou período.
- Criar, consultar, atualizar e excluir acompanhamentos de demonstração, preservando os eventos e as leituras originais.

### Parâmetros e limites de interpretação

| Dado planejado | Representação na demonstração | Limite |
|---|---|---|
| Temperatura | Leitura em °C | Isoladamente, não comprova contaminação. |
| Sinal óptico | Índice relativo simulado de 0 a 100 | Não representa NTU, concentração de contaminantes ou resultado sanitário. |
| Vazão ou presença de fluxo | Entrada simulada em L/min | Ajuda a distinguir circulação e água parada naquele trecho. |
| Pressão | Entrada simulada em bar | Contextualiza o abastecimento; não indica potabilidade. |

**O Vigia d’Água não filtra a água, não identifica todos os contaminantes e não substitui análises laboratoriais de potabilidade.** A leitura representa o ponto monitorado, sem garantir as condições de todas as torneiras, do reservatório ou da rede pública.

Filtragem, dosagem de produtos, fechamento automático do registro e emissão de laudos ficam fora do MVP.

## Estados propostos do aparelho

| Estado | Mensagem de apoio | Indicação planejada |
|---|---|---|
| Sem alteração detectada | Nos parâmetros acompanhados | Palavra, símbolo de confirmação e verde. |
| Atenção | Mudança em observação | Palavra, triângulo e amarelo. |
| Verifique | Mudança persistente | Palavra, exclamação e vermelho. |
| Leitura indisponível | Confira o aparelho | Palavra, símbolo de falha e cinza. |

Os significados também serão mantidos na plataforma. A cor será complementar: texto e símbolo precisam permitir a compreensão do aviso. Falta de leitura não deverá aparecer como normalidade.

## Arquitetura planejada

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
| Backend IoT | Receber e disponibilizar as leituras do dispositivo. | FIWARE ou IoT Cloud, a definir. |
| API de acompanhamentos | Persistir e disponibilizar operações de CRUD. | Serviço externo, a definir. |
| Plataforma web | Apresentar o ponto, o histórico, os eventos e as providências. | React com Vite ou Next.js e Tailwind CSS, a definir na implementação. |
| Aplicação Python | Processar dados, organizar consultas e persistir registros de demonstração. | Python e arquivos JSON. |
| Modelagem matemática | Estudar comportamento, taxas de variação e grandezas acumuladas nos cenários simulados. | Python ou GeoGebra, conforme a avaliação de DPS. |

Essa arquitetura descreve o planejamento. Ainda não há implementação funcional publicada neste repositório.

## Estado do projeto

| Frente | Estado em 11/11/2026 |
|---|---|
| Identidade da companhia e do produto | Definida. |
| Pesquisa exploratória | Primeiras 18 respostas analisadas; coleta em andamento. |
| Software & Total Experience Design | Apresentação de 16 slides preparada e revisada fora deste repositório. |
| Storytelling e Inspiração Empreendedora | A desenvolver. |
| Edge Computing & Computer Systems | A implementar. |
| Computational Thinking with Python | A implementar. |
| Differentiated Problem Solving | Modelagem e relatório a desenvolver conforme as questões da avaliação. |
| Front-end Design e Web Development | Aplicação compartilhada a implementar, com entregas acadêmicas separadas. |

## Acesso ao repositório

```bash
git clone https://github.com/BrThiagoN/water-n-assurance.git
cd water-n-assurance
```

O repositório está na etapa inicial de documentação. Ainda não há aplicação para instalar ou executar. As instruções de ambiente, dependências e execução de cada parte serão incluídas junto à respectiva implementação.

Links de Wokwi, deploy e vídeos serão publicados quando esses materiais existirem e tiverem sido verificados pela equipe.

## Próximas etapas

1. Dividir as frentes de trabalho e combinar formato de dados, unidades, identificadores e estados.
2. Preparar o documento de marca, os slides e o videopitch de Storytelling.
3. Implementar o circuito e testar cedo a comunicação entre ESP32, backend e painel.
4. Desenvolver a aplicação web com CRUD externo e histórico real de commits.
5. Desenvolver o programa Python e a análise matemática de DPS em paralelo.
6. Executar os cenários técnicos, documentar resultados e finalizar as entregas de cada disciplina.

Testes de compreensão com moradores, ensaios com sensores físicos e seleção de um local para piloto fazem parte da continuidade planejada. Não são apresentados como atividades já realizadas.

## Equipe

| Integrante | RM |
|---|---|
| Thiago Gomes Nascimento | RM569436 |
| Rafael De Sousa | RM568777 |
| Vitor Assis Florenzano | RM572192 |

## Documentação do uso de IA

Este README foi preparado com apoio de inteligência artificial a partir das decisões e materiais do projeto. A equipe é responsável por revisar as informações e compreender as escolhas apresentadas. O registro do apoio de IA em cada implementação deverá acompanhar a documentação exigida pela respectiva disciplina.
