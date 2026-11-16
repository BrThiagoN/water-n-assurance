# Dashboard — water n' assurance

Interface do **Vigia d’Água**, separada da [API Python](../backend/README.md). React, TypeScript estrito, Vite e Tailwind CSS. Usa DM Serif Display e Source Sans 3, as mesmas fontes dos slides, e a paleta Merino `#F5EEDD`, Rock Blue `#84B3CE` e Venice Blue `#16587B`. As fontes são servidas pela própria aplicação.

## Executar

Primeiro inicie a API seguindo [`backend/README.md`](../backend/README.md). Depois, com Node.js 24.15+ da linha 24:

```bash
cd dashboard
npm ci
npm run dev
```

Abra `http://localhost:5173`. A interface consulta `http://localhost:8000` por padrão. Para outra API, crie `.env.local` nesta pasta:

```dotenv
VITE_API_URL=https://SEU-BACKEND.onrender.com
```

Inclua a origem da interface em `ALLOWED_ORIGINS` do backend. Se um `.env` antigo tiver `ALLOWED_ORIGINS=` vazio, configure `http://localhost:5173` para desenvolvimento. Reinicie a API depois de mudar esse arquivo e o Vite depois de mudar suas variáveis.

Variáveis `VITE_` são públicas no JavaScript gerado. **Não inclua tokens nelas.** A chave administrativa dos acompanhamentos é digitada pela pessoa responsável e fica apenas em memória até sair ou recarregar a página.

## Funcionalidades

- Consulta automática do ponto, último estado, validade dos quatro canais e idade da comunicação.
- Histórico de até 200 leituras, com seleção de período e exportação CSV pela API.
- Linha do gráfico interrompida em leituras inválidas; nenhum preenchimento de dados ausentes.
- Eventos com data/hora de recebimento em Brasília e identificação de entradas de teste.
- CRUD de acompanhamentos: criar, listar, editar, concluir/reabrir e excluir a anotação, preservando o evento.
- Layout responsivo, navegação por teclado, rótulos nos campos e cor acompanhada de texto.

`useMonitor` concentra a consulta e atualização de dados; `useFollowups` gerencia o CRUD. Os componentes ficam em `src/components`, o cliente e os schemas de validação em `src/api.ts` e as regras de apresentação em `src/presentation.ts`.

Se não houver leituras ou a API estiver indisponível, a interface explica a situação. Quando uma leitura fica antiga, os valores deixam de aparecer como atuais e o estado é identificado como o último recebido. Registros históricos incompletos da versão anterior são identificados, sem inventar números.

O índice óptico é relativo, de 0 a 100; não é NTU nem uma escala de potabilidade. O protótipo não filtra a água nem substitui análises laboratoriais.

## Conferir antes de publicar

```bash
npm test
npm run build
npm run preview
```

O build inclui a checagem TypeScript. Os testes verificam leituras antigas/inválidas, lacunas no gráfico e estados desconhecidos. O workflow do GitHub repete teste e build.

O [Blueprint Render](../render.yaml) configura esta pasta como **Static Site**, publicado a partir de `dist`. Ele é separado da API Python. Se os endereços atribuídos forem diferentes, ajuste `VITE_API_URL` na interface e `ALLOWED_ORIGINS` na API. Uma mudança de `VITE_API_URL` exige reconstruir o dashboard.

Esta implementação ajuda a demonstrar o projeto; a entrega de Front-end/Web ainda precisa da revisão dos requisitos acadêmicos, inclusive os commits significativos, arquivos e acesso dos professores. Não apresenta testes com moradores ou validação física como atividades concluídas.

## Equipe

| Integrante | RM |
|---|---|
| Thiago Gomes Nascimento | RM569436 |
| Rafael De Sousa | RM568777 |
| Vitor Assis Florenzano | RM572192 |
| Raul Moreira Andrade | RM571042 |

Uso de IA: [AI.md](AI.md).
