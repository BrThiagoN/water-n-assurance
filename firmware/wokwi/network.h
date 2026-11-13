#pragma once

#include <stdint.h>
#include "edge_core.h"

// Transporte preparado para um broker futuro. Não há servidor/backend incluído.
// Toda a API abaixo é rápida e não acessa sockets no loop de aquisição.
namespace net {

struct Snapshot {
  char deviceId[24];
  char bootId[16];
  uint32_t seq;
  uint32_t uptimeMs;
  float temperatureC;
  float opticalRaw;
  float opticalFiltered;
  float flowLpm;
  float pressureBar;
  bool readingsValid;
  bool filteredValid;
  bool assessmentAvailable;
  bool testOverride;
  uint8_t overrideMask;
  vigia::Quality quality[4];
  uint32_t ageMs[4];
  char observation[12];
  char state[20];
  char reason[24];
  char flowContext[16];
  char lastAlertStatus[12];
  char lastAlertState[16];
};

// Chamar uma vez no setup, antes de offer(). Se faltar memória para a task/fila,
// registra o erro e conserva o dispositivo local funcionando, offline.
void begin();

// Copia o snapshot: o chamador pode reutilizar sua estrutura imediatamente.
// stateChanged também enfileira um evento identificado por boot+sequência.
// Snapshot: última versão vence. Eventos: 16 na fila + no máximo 1 em envio;
// lotação descarta o mais antigo ainda na fila e incrementa droppedEvents().
void offer(const Snapshot& snapshot, bool stateChanged);

// Liga/desliga somente o transporte; não altera estados, sensores ou alertas.
// A operação de socket já em curso pode terminar na task antes da desconexão.
void setEnabled(bool enabled);
bool online();
uint32_t droppedEvents();

// Número de mensagens confirmadas por PUBACK MQTT (telemetria + eventos).
// Não significa que um backend persistiu dados ou que um painel os mostrou.
uint32_t sentCount();

// Exercita os encoders reais e relê seu JSON, sem iniciar Wi-Fi/MQTT.
// Acrescenta resultados aos contadores recebidos; não os reinicializa.
// Chamar na bateria de testes, antes de begin(). Retorna o resultado DESTA
// bateria, independentemente de falhas anteriores nos contadores globais.
bool runSelfTests(void (*report)(const char*, bool),
                  uint16_t& passed, uint16_t& failed);

}  // namespace net

