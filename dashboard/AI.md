# Apoio de IA no dashboard

Usamos o Codex para criar a interface a pedido da equipe. A referência visual foram os slides de Software & Total Experience Design e a paleta enviada por Thiago: Merino, Rock Blue e Venice Blue. As fontes do PDF foram conferidas antes da implementação: DM Serif Display e Source Sans 3.

O apoio incluiu componentes React, TypeScript estrito, estilos, cliente HTTP, schemas Zod, consulta periódica, gráfico sem preenchimento de lacunas, CRUD de acompanhamentos e testes de apresentação. A interface foi separada em `dashboard/`, mantendo a API Python em `backend/`.

A validação inclui build, testes automatizados e conferência no navegador com a API e Postgres locais, incluindo criação, edição, conclusão e exclusão de uma anotação de teste. O layout foi conferido também em largura de celular. Isso não equivale a teste de compreensão com moradores, medição física da água ou prova de integração de um Wokwi real.

A equipe precisa revisar os textos, entender os hooks e o contrato da API e conferir os requisitos de cada disciplina antes de apresentar o trabalho.
