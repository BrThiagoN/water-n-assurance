# Vigia d’Água — protótipo ESP32 no Wokwi

Este diretório espelha os arquivos do [protótipo público no Wokwi](https://wokwi.com/projects/480416382701050881), revisão 3. Ele permite revisar o sketch, o diagrama, as dependências e os testes junto com a documentação do projeto.

## O que o dispositivo simula

Um ESP32 recebe valores simulados, valida as leituras, suaviza o índice óptico e decide localmente se há uma mudança para acompanhar. O display OLED, o LED RGB e o buzzer mostram a resposta sem depender de servidor.

| Componente no diagrama | Pino | Função nesta revisão |
|---|---:|---|
| DS18B20 | GPIO 4 | Temperatura em °C, como contexto da leitura. |
| Potenciômetro “Óptica” | GPIO 32 | Índice óptico relativo de 0 a 100; é a entrada usada para classificar mudanças. Não é NTU. |
| Potenciômetro “Vazão” | GPIO 33 | Entrada simulada em L/min; indica se há circulação e se a avaliação pode avançar. |
| Potenciômetro “Pressão” | GPIO 34 | Entrada simulada em bar, registrada como contexto. Não estima nível da caixa nem classifica potabilidade. |
| OLED SSD1306 | I²C: SDA 21, SCL 22 | Mensagem textual, estado, disponibilidade e instrução do botão. |
| LED RGB | GPIO 16, 17 e 18 | Indicação visual associada ao estado, junto com palavra e símbolo. |
| Buzzer piezo | GPIO 25 | Alarme sonoro contínuo para uma alteração confirmada. |
| Botão “Silenciar” | GPIO 27 | Reconhece e silencia; outro clique reativa o som se a mesma condição continuar confirmada. |
| Chave “Falha simulada” | GPIO 26 | Exercita o tratamento local de uma falha de entrada. |

Os três potenciômetros e a chave de falha são controles do simulador, não sondas físicas. As unidades de vazão e pressão são valores de cenário; não há calibração de sensores reais.

## Interpretação local

O ESP32 mantém quatro resultados compreensíveis: **Sem alteração detectada**, **Atenção**, **Verifique** e **Leitura indisponível**. O índice óptico passa por uma mediana de cinco amostras. Os limites didáticos da revisão atual são 30 e 60 para subida e 25 e 55 para retorno, com confirmações novas apoiadas pelo valor bruto e pelo filtrado. Eles servem para demonstrar persistência e histerese, não são limites sanitários.

Leitura inválida, ausente ou antiga não vira normalidade. Temperatura e pressão dão contexto; falhas nesses canais são informadas sem apagar automaticamente uma avaliação óptica ainda válida. A classificação óptica depende de sinal óptico e circulação utilizáveis.

Quando Atenção ou Verifique é confirmado, o buzzer permanece ligado até reconhecimento. O primeiro clique silencia. Se a condição ainda estiver confirmada, o próximo clique liga o buzzer novamente. Se a leitura já normalizou, o clique prepara os avisos seguintes; não ressuscita um alarme antigo. Uma nova ocorrência ou escalada rearma o alarme.

## Arquivos e execução

Os oito arquivos de circuito, código e dependências precisam permanecer no mesmo projeto Wokwi:

- diagram.json — circuito e ligações;
- sketch.ino — aquisição, interface e ciclo principal;
- config.h — pinos, intervalos e configuração;
- edge_core.h — validação, filtragem, estados e política do alarme;
- self_tests.h — autotestes executados no início;
- network.h e network.cpp — telemetria preparada para configuração posterior;
- libraries.txt — dependências do Wokwi.

Abra o projeto público, inicie a simulação e confira os resultados no Serial Monitor. O projeto está salvo no Wokwi; estes arquivos no GitHub são o espelho versionado para leitura e revisão. TESTES.md resume os testes executados e aponta as evidências desta revisão.

A rede não é necessária para a leitura, classificação ou alarme local. O host MQTT está como localhost:1883, aguardando configuração: no ESP32, localhost é o próprio dispositivo e ainda não existe um broker conectado. Não há credenciais reais no firmware.

## Limites e entrega acadêmica

Este é um protótipo simulado. Não foi validado com água, sensores físicos, instrumento de referência ou medição acústica; não determina potabilidade e não substitui análise laboratorial. O backend, o painel web e a integração de nuvem ainda estão pendentes.

O briefing da disciplina pede um projeto Wokwi público que abra e rode, além de um vídeo de equipe de até sete minutos mostrando o circuito, as decisões no ESP32 e os estados do protótipo. O vídeo ainda será produzido. Este espelho no GitHub facilita a revisão e o versionamento, mas não substitui os links de entrega da disciplina.
