#include "network.h"
#include "config.h"
#include "https_ca.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// HTTPS com certificado verificado. A chave de ingestao existe somente em RAM,
// carregada pelo Serial; nunca fica no projeto publico. Rede roda em task propria.

namespace net {
namespace {

constexpr uint32_t kMaxRetryMs = 30000;
constexpr uint32_t kLoopDelayMs = 20;
constexpr uint32_t kEventIntervalMs = 200;
constexpr UBaseType_t kEventQueueLength = 16;
constexpr size_t kPayloadCapacity = 3072;
constexpr uint32_t kTaskStackBytes = 16384;

struct SnapshotEnvelope {
  Snapshot value;
  uint32_t changeGeneration;
};

struct PendingEvent {
  Snapshot value;
  uint32_t eventSequence;
};

static_assert(std::is_trivially_copyable<Snapshot>::value,
              "Snapshots enviados pelas filas precisam aceitar copia por valor");

QueueHandle_t snapshotQueue = nullptr;
QueueHandle_t eventQueue = nullptr;
std::atomic<bool> started(false);
std::atomic<bool> enabled(true);
std::atomic<bool> backendOnline(false);
std::atomic<bool> credentialChanged(false);
portMUX_TYPE tokenMutex = portMUX_INITIALIZER_UNLOCKED;
char ingestToken[129] = {};
std::atomic<uint32_t> changeGeneration(0);
std::atomic<uint32_t> dropped(0);
std::atomic<uint32_t> sent(0);

bool elapsed(uint32_t now, uint32_t previous, uint32_t interval) {
  return static_cast<uint32_t>(now - previous) >= interval;
}

uint32_t baseRetry() {
  const uint32_t configured = Config::NETWORK_RETRY_MS;
  return configured == 0 ? 1000 :
         (configured > kMaxRetryMs ? kMaxRetryMs : configured);
}

uint32_t nextRetry(uint32_t current) {
  return current >= kMaxRetryMs / 2 ? kMaxRetryMs : current * 2;
}

void terminateStrings(Snapshot& snapshot) {
  snapshot.deviceId[sizeof(snapshot.deviceId) - 1] = '\0';
  snapshot.bootId[sizeof(snapshot.bootId) - 1] = '\0';
  snapshot.state[sizeof(snapshot.state) - 1] = '\0';
  snapshot.reason[sizeof(snapshot.reason) - 1] = '\0';
  snapshot.flowContext[sizeof(snapshot.flowContext) - 1] = '\0';
  snapshot.lastAlertStatus[sizeof(snapshot.lastAlertStatus) - 1] = '\0';
  snapshot.lastAlertState[sizeof(snapshot.lastAlertState) - 1] = '\0';
  snapshot.observation[sizeof(snapshot.observation) - 1] = '\0';
  if (snapshot.deviceId[0] == '\0') {
    snprintf(snapshot.deviceId, sizeof(snapshot.deviceId), "%s", Config::DEVICE_ID);
  }
}

float channelValue(const Snapshot& snapshot, uint8_t channel) {
  const float values[] = {snapshot.temperatureC, snapshot.opticalRaw,
                         snapshot.flowLpm, snapshot.pressureBar};
  return values[channel];
}

vigia::Quality effectiveQuality(const Snapshot& snapshot, uint8_t channel) {
  const float value = channelValue(snapshot, channel);
  const float low[] = {-55, 0, 0, 0}, high[] = {125, 100, 20, 6};
  if (snapshot.quality[channel] != vigia::Quality::VALID) return snapshot.quality[channel];
  if (!std::isfinite(value)) return vigia::Quality::INVALID;
  return value >= low[channel] && value <= high[channel] ?
    vigia::Quality::VALID : vigia::Quality::OUT_OF_RANGE;
}

bool channelValid(const Snapshot& snapshot, uint8_t channel) {
  return effectiveQuality(snapshot, channel) == vigia::Quality::VALID;
}

bool finiteReadings(const Snapshot& snapshot) {
  return snapshot.readingsValid && channelValid(snapshot, 0) &&
    channelValid(snapshot, 1) && channelValid(snapshot, 2) && channelValid(snapshot, 3);
}

bool usableFilter(const Snapshot& snapshot) {
  return channelValid(snapshot, 1) && channelValid(snapshot, 2) &&
    snapshot.filteredValid && std::isfinite(snapshot.opticalFiltered) &&
    snapshot.opticalFiltered >= 0 && snapshot.opticalFiltered <= 100;
}

template <typename TDocument>
void addChannels(TDocument& document, const Snapshot& snapshot, bool event) {
  JsonObject channels = document[event ? "event_channels" : "channels"].template to<JsonObject>();
  const char* names[] = {"temperature", "optical", "flow", "pressure"};
  const char* units[] = {"degC", "relative_index", "L/min", "bar"};
  const char* fields[] = {"temperature_c", "optical_index_raw", "flow_l_min", "pressure_bar"};
  const char* eventFields[] = {"event_temperature_c", "event_optical_index_raw",
                             "event_flow_l_min", "event_pressure_bar"};
  for (uint8_t i = 0; i < 4; ++i) {
    JsonObject item = channels[names[i]].to<JsonObject>();
    const bool valid = channelValid(snapshot, i);
    item["quality"] = vigia::qualityName(effectiveQuality(snapshot, i));
    item["valid"] = valid;
    item["unit"] = units[i];
    item["source"] = (snapshot.overrideMask & (1u << i)) ? "override" : "simulator";
    if (snapshot.quality[i] != vigia::Quality::NOT_READY) item["age_ms"] = snapshot.ageMs[i];
    if (valid) {
      item["value"] = channelValue(snapshot, i);
      document[event ? eventFields[i] : fields[i]] = channelValue(snapshot, i);
    }
  }
}

template <typename TDocument>
size_t finishJson(TDocument& document, char* payload, size_t capacity) {
  const size_t required = measureJson(document);
  if (document.overflowed() || capacity == 0 || required >= capacity) return 0;
  const size_t written = serializeJson(document, payload, capacity);
  if (written != required) return 0;
  payload[written] = '\0';
  return written;
}

size_t encodeSnapshot(const Snapshot& snapshot, char* payload, size_t capacity) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument document;
#else
  DynamicJsonDocument document(4096);
#endif
  const bool valid = finiteReadings(snapshot);
  const bool filterValid = usableFilter(snapshot);
  document["schema_version"] = 2;
  document["rule_version"] = Config::RULE_VERSION;
  document["message_kind"] = "telemetry";
  document["device_id"] = snapshot.deviceId;
  document["boot_id"] = snapshot.bootId;
  document["seq"] = snapshot.seq;
  document["uptime_ms"] = snapshot.uptimeMs;
  document["sent_uptime_ms"] = static_cast<uint32_t>(millis());
  document["time_synced"] = false;
  document["simulated"] = true;
  document["test_override"] = snapshot.testOverride;
  document["assessment_available"] = snapshot.assessmentAvailable &&
    channelValid(snapshot, 1) && channelValid(snapshot, 2);
  document["assessment_scope"] = "relative_optical_change";
  document["optical_observation"] = snapshot.observation;
  document["observation_provisional"] = std::strcmp(snapshot.observation, "NONE") != 0 &&
    snapshot.observation[0] != '\0';
  document["readings_valid"] = valid;
  document["filtered_valid"] = filterValid;
  document["assessment_state"] = snapshot.state;
  document["reason"] = snapshot.reason;
  document["flow_context"] = snapshot.flowContext;
  document["last_alert_status"] = snapshot.lastAlertStatus;
  document["last_alert_state"] = snapshot.lastAlertState;
  document["dropped_events"] = dropped.load();
  addChannels(document, snapshot, false);
  if (filterValid) document["optical_index_filtered"] = snapshot.opticalFiltered;
  // Sem UTC confiável: o backend deverá atribuir received_at. uptime NÃO é ts.
  // Omitir valores inválidos exige que o futuro painel respeite *_valid;
  // ausência de chave não deve transformar o último número guardado em atual.
  return finishJson(document, payload, capacity);
}

size_t encodeEvent(const PendingEvent& event, char* payload, size_t capacity) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument document;
#else
  DynamicJsonDocument document(4096);
#endif
  const Snapshot& snapshot = event.value;
  const bool valid = finiteReadings(snapshot);
  const bool filterValid = usableFilter(snapshot);
  char eventId[72];
  snprintf(eventId, sizeof(eventId), "%s:%s:%lu", snapshot.deviceId,
           snapshot.bootId, static_cast<unsigned long>(event.eventSequence));
  document["schema_version"] = 2;
  document["event_rule_version"] = Config::RULE_VERSION;
  document["message_kind"] = "event";
  document["device_id"] = snapshot.deviceId;
  document["boot_id"] = snapshot.bootId;
  document["time_synced"] = false;
  document["simulated"] = true;
  document["event_test_override"] = snapshot.testOverride;
  document["event_assessment_available"] = snapshot.assessmentAvailable &&
    channelValid(snapshot, 1) && channelValid(snapshot, 2);
  document["event_assessment_scope"] = "relative_optical_change";
  document["event_optical_observation"] = snapshot.observation;
  document["event_observation_provisional"] = std::strcmp(snapshot.observation, "NONE") != 0 &&
    snapshot.observation[0] != '\0';
  document["event_id"] = eventId;
  document["event_seq"] = event.eventSequence;
  document["event_sample_seq"] = snapshot.seq;
  document["event_uptime_ms"] = snapshot.uptimeMs;
  document["sent_uptime_ms"] = static_cast<uint32_t>(millis());
  document["event_state"] = snapshot.state;
  document["event_reason"] = snapshot.reason;
  document["event_flow_context"] = snapshot.flowContext;
  document["event_last_alert_status"] = snapshot.lastAlertStatus;
  document["event_last_alert_state"] = snapshot.lastAlertState;
  document["event_readings_valid"] = valid;
  document["event_filtered_valid"] = filterValid;
  document["dropped_events"] = dropped.load();
  addChannels(document, snapshot, true);
  if (filterValid) {
    document["event_optical_index_filtered"] = snapshot.opticalFiltered;
  }
  // Reenvios usam o MESMO event_id. O backend futuro precisa reconhecer
  // duplicatas. Campos event_* não sobrescrevem o estado atual do dispositivo.
  return finishJson(document, payload, capacity);
}


bool copyToken(char* destination, size_t capacity) {
  portENTER_CRITICAL(&tokenMutex);
  snprintf(destination, capacity, "%s", ingestToken);
  portEXIT_CRITICAL(&tokenMutex);
  return destination[0] != '\0';
}

class HttpsTransport {
 public:
  bool publish(char* payload, size_t length, const char* token) {
    if (!length || !token[0] || WiFi.status() != WL_CONNECTED) return false;
    if (!begun) {
      client.setCACert(VIGIA_HTTPS_ROOT_CA);
      client.setHandshakeTimeout(30);  // Segundos; certificado sempre verificado.
      http.setConnectTimeout(30000);
      http.setTimeout(15000);
      http.setReuse(true);
      if (!http.begin(client, Config::HTTPS_INGEST_URL)) return false;
      begun = true;
    }
    http.addHeader("Content-Type", "application/json", false, true);
    http.addHeader("Authorization", String("Bearer ") + token, false, true);
    const int status = http.POST(reinterpret_cast<uint8_t*>(payload), length);
    bool accepted = false;
    if ((status == 200 || status == 201) && http.getSize() <= 2048) {
      const String body = http.getString();  // Drena a resposta antes de reutilizar TCP.
#if ARDUINOJSON_VERSION_MAJOR >= 7
      JsonDocument acknowledgment;
#else
      DynamicJsonDocument acknowledgment(512);
#endif
      accepted = !deserializeJson(acknowledgment, body) &&
        acknowledgment["accepted"].is<bool>() && acknowledgment["accepted"].as<bool>();
    }
    backendOnline.store(accepted);
    if (accepted) {
      sent.fetch_add(1);
      Serial.printf("[NET] HTTPS %d: persistencia confirmada pelo backend Python.\n", status);
    } else {
      char tlsMessage[128] = {};
      const int tlsError = client.lastError(tlsMessage, sizeof(tlsMessage));
      Serial.printf("[NET] HTTPS %d (%s), TLS %d (%s), epoch=%lld; retry agendado.\n",
                    status, HTTPClient::errorToString(status).c_str(), tlsError,
                    tlsMessage, static_cast<long long>(time(nullptr)));
      disconnect();
    }
    return accepted;
  }

  void disconnect() {
    http.end();
    client.stop();
    begun = false;
    backendOnline.store(false);
  }

 private:
  WiFiClientSecure client;
  HTTPClient http;
  bool begun = false;
};

void networkTask(void*) {
  HttpsTransport transport;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  SnapshotEnvelope latest = {};
  PendingEvent pending = {};
  bool haveSnapshot = false, havePending = false;
  bool wasEnabled = true, forceLatest = true, clockConfigured = false;
  bool missingTokenReported = false, clockWaitReported = false;
  uint32_t lastSentGeneration = 0, lastTelemetryMs = 0, lastEventMs = 0;
  uint32_t wifiRetryMs = baseRetry(), httpRetryMs = baseRetry();
  uint32_t lastWifiAttemptMs = static_cast<uint32_t>(millis()) - wifiRetryMs;
  uint32_t lastHttpFailureMs = 0;
  bool retryPending = false;
  char payload[kPayloadCapacity];
  char token[129];
  Serial.printf("[NET] Destino HTTPS: %s\n", Config::HTTPS_INGEST_URL);
  Serial.println("[NET] Use NET TOKEN <chave> no Serial. A chave nao sera impressa nem salva.");

  for (;;) {
    const uint32_t now = millis();
    SnapshotEnvelope update;
    if (xQueueReceive(snapshotQueue, &update, 0) == pdTRUE) {
      latest = update; haveSnapshot = true;
    }
    if (credentialChanged.exchange(false)) {
      transport.disconnect();
      forceLatest = true; retryPending = false; missingTokenReported = false;
      httpRetryMs = baseRetry(); backendOnline.store(false);
    }
    if (!enabled.load()) {
      if (wasEnabled) {
        transport.disconnect();
        WiFi.disconnect();
        Serial.println("[NET] Transporte desativado; avaliacao local continua.");
      }
      backendOnline.store(false); wasEnabled = false;
      vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs)); continue;
    }
    if (!wasEnabled) {
      wifiRetryMs = httpRetryMs = baseRetry();
      lastWifiAttemptMs = now - wifiRetryMs;
      forceLatest = true; wasEnabled = true; retryPending = false;
    }
    if (!copyToken(token, sizeof(token))) {
      backendOnline.store(false);
      if (!missingTokenReported) {
        Serial.println("[NET] Aguardando chave de demonstracao via NET TOKEN; decisao local ativa.");
        missingTokenReported = true;
      }
      vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs)); continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      backendOnline.store(false); forceLatest = true;
      if (elapsed(now, lastWifiAttemptMs, wifiRetryMs)) {
        lastWifiAttemptMs = now;
        WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD, 6);
        wifiRetryMs = nextRetry(wifiRetryMs);
      }
      vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs)); continue;
    }
    wifiRetryMs = baseRetry();
    if (!clockConfigured) {
      configTime(0, 0, "time.google.com", "pool.ntp.org");
      clockConfigured = true;
    }
    // O relogio serve a verificacao TLS. O contrato continua sem UTC da medicao.
    if (time(nullptr) < 1700000000) {
      if (!clockWaitReported) {
        Serial.println("[NET] Sincronizando relogio para validar certificado TLS.");
        clockWaitReported = true;
      }
      vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs)); continue;
    }
    if (retryPending && !elapsed(now, lastHttpFailureMs, httpRetryMs)) {
      vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs)); continue;
    }
    const bool urgent = latest.changeGeneration != lastSentGeneration;
    if (haveSnapshot && (forceLatest || urgent ||
        elapsed(now, lastTelemetryMs, Config::TELEMETRY_INTERVAL_MS))) {
      const size_t size = encodeSnapshot(latest.value, payload, sizeof(payload));
      if (!size) {
        Serial.println("[NET] Snapshot excedeu capacidade JSON; nao enviado.");
        lastTelemetryMs = now; forceLatest = false;
        lastSentGeneration = latest.changeGeneration;
      } else if (transport.publish(payload, size, token)) {
        lastTelemetryMs = millis(); lastSentGeneration = latest.changeGeneration;
        forceLatest = false; retryPending = false; httpRetryMs = baseRetry();
      } else {
        retryPending = true; lastHttpFailureMs = millis();
        httpRetryMs = nextRetry(httpRetryMs);
        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs)); continue;
      }
    }
    // Eventos antigos nunca substituem o snapshot atual no backend.
    if (enabled.load() && elapsed(millis(), lastEventMs, kEventIntervalMs)) {
      if (!havePending) havePending = xQueueReceive(eventQueue, &pending, 0) == pdTRUE;
      if (havePending) {
        const size_t size = encodeEvent(pending, payload, sizeof(payload));
        if (!size) {
          dropped.fetch_add(1); havePending = false;
          Serial.println("[NET] Evento excedeu capacidade JSON; descarte contado.");
        } else if (transport.publish(payload, size, token)) {
          havePending = false; lastEventMs = millis();
          retryPending = false; httpRetryMs = baseRetry();
        } else {
          retryPending = true; lastHttpFailureMs = millis();
          httpRetryMs = nextRetry(httpRetryMs);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
  }
}

}  // namespace

void begin() {
  if (started.exchange(true)) return;
  snapshotQueue = xQueueCreate(1, sizeof(SnapshotEnvelope));
  eventQueue = xQueueCreate(kEventQueueLength, sizeof(PendingEvent));
  if (snapshotQueue == nullptr || eventQueue == nullptr) {
    if (snapshotQueue != nullptr) vQueueDelete(snapshotQueue);
    if (eventQueue != nullptr) vQueueDelete(eventQueue);
    snapshotQueue = eventQueue = nullptr;
    started.store(false);
    Serial.println("[NET] Sem memoria para filas; continua somente local.");
    return;
  }
  if (xTaskCreate(networkTask, "vigia-net", kTaskStackBytes, nullptr, 1, nullptr)
      != pdPASS) {
    vQueueDelete(snapshotQueue);
    vQueueDelete(eventQueue);
    snapshotQueue = eventQueue = nullptr;
    started.store(false);
    Serial.println("[NET] Sem memoria para tarefa; continua somente local.");
  }
}

void offer(const Snapshot& snapshot, bool stateChanged) {
  Snapshot copy = snapshot;
  terminateStrings(copy);
  const uint32_t generation = stateChanged ? changeGeneration.fetch_add(1) + 1 :
                                            changeGeneration.load();
  if (snapshotQueue != nullptr) {
    SnapshotEnvelope envelope = {copy, generation};
    xQueueOverwrite(snapshotQueue, &envelope);
  }
  if (!stateChanged) return;
  if (eventQueue == nullptr) {
    dropped.fetch_add(1);
    return;
  }
  const PendingEvent event = {copy, generation};
  if (xQueueSendToBack(eventQueue, &event, 0) == pdTRUE) return;
  PendingEvent discarded;
  if (xQueueReceive(eventQueue, &discarded, 0) == pdTRUE) dropped.fetch_add(1);
  // O consumidor pode liberar um slot ao mesmo tempo; todas as operações têm
  // espera zero. Se o segundo envio falhar, contar também o evento novo.
  if (xQueueSendToBack(eventQueue, &event, 0) != pdTRUE) dropped.fetch_add(1);
}

bool setIngestToken(const char* value) {
  const size_t length = value ? strlen(value) : 0;
  if (length && (length < 32 || length > 128)) return false;
  for (size_t i = 0; i < length; ++i)
    if (value[i] < '!' || value[i] > '~') return false;
  portENTER_CRITICAL(&tokenMutex);
  snprintf(ingestToken, sizeof(ingestToken), "%s", value ? value : "");
  portEXIT_CRITICAL(&tokenMutex);
  credentialChanged.store(true);
  return true;
}

void setEnabled(bool value) {
  enabled.store(value);
  if (!value) backendOnline.store(false);
}

bool online() { return backendOnline.load() && enabled.load(); }
uint32_t droppedEvents() { return dropped.load(); }
uint32_t sentCount() { return sent.load(); }

bool runSelfTests(void (*report)(const char*, bool),
                  uint16_t& passed, uint16_t& failed) {
  // JsonDocument de leitura usa heap para não somar outra arena de 2 KiB ao
  // stack do setup enquanto o encoder está ativo (compatível com AJ6/AJ7).
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument parsed;
#else
  DynamicJsonDocument parsed(4096);
#endif
  char payload[kPayloadCapacity];
  bool allPassed = true;
  const auto check = [&](const char* name, bool success) {
    if (success) ++passed;
    else ++failed;
    allPassed = allPassed && success;
    if (report != nullptr) report(name, success);
  };
  const auto decode = [&]() -> bool {
    parsed.clear();
    // const input evita o parsing zero-copy alterar o payload; assim o teste
    // de escaping também inspeciona exatamente os bytes enviados pelo encoder.
    return deserializeJson(parsed, static_cast<const char*>(payload)) ==
           DeserializationError::Ok;
  };

  Snapshot reference = {};
  snprintf(reference.deviceId, sizeof(reference.deviceId), "net-test-01");
  snprintf(reference.bootId, sizeof(reference.bootId), "boot-test");
  reference.seq = 42;
  reference.uptimeMs = 46000;
  reference.temperatureC = 25.0f;
  reference.opticalRaw = 80.0f;
  reference.opticalFiltered = 76.0f;
  reference.flowLpm = 6.0f;
  reference.pressureBar = 2.0f;
  reference.readingsValid = true;
  reference.filteredValid = true;
  reference.assessmentAvailable = true;
  for (uint8_t i = 0; i < 4; ++i) reference.quality[i] = vigia::Quality::VALID;
  reference.ageMs[0] = 250;
  snprintf(reference.observation, sizeof(reference.observation), "NONE");
  snprintf(reference.state, sizeof(reference.state), "CHECK");
  snprintf(reference.reason, sizeof(reference.reason), "OPTICAL_PERSISTENT_HIGH");
  snprintf(reference.flowContext, sizeof(reference.flowContext), "FLOWING");
  snprintf(reference.lastAlertStatus, sizeof(reference.lastAlertStatus), "OPEN");
  snprintf(reference.lastAlertState, sizeof(reference.lastAlertState), "CHECK");

  // 1. Caminho normal, incluindo contrato de horário: uptime não vira UTC.
  size_t length = encodeSnapshot(reference, payload, sizeof(payload));
  bool success = length > 0 && decode();
  success = success && std::strcmp(parsed["assessment_state"] | "", "CHECK") == 0 &&
      parsed["seq"].as<uint32_t>() == reference.seq &&
      parsed["uptime_ms"].as<uint32_t>() == reference.uptimeMs &&
      parsed["time_synced"].is<bool>() && !parsed["time_synced"].as<bool>() &&
      !parsed.containsKey("ts") && parsed["readings_valid"].as<bool>() &&
      parsed["schema_version"].as<int>() == 2 && parsed["assessment_available"].as<bool>() &&
      parsed["simulated"].as<bool>() &&
      std::strcmp(parsed["channels"]["optical"]["unit"] | "", "relative_index") == 0 &&
      parsed["filtered_valid"].as<bool>() &&
      std::fabs(parsed["temperature_c"].as<float>() - 25.0f) < 0.001f &&
      std::fabs(parsed["optical_index_filtered"].as<float>() - 76.0f) < 0.001f;
  check("NET JSON: snapshot valido e uptime sem UTC", success);

  // 2. Uma falha contextual não apaga os outros canais ou a avaliação óptica.
  Snapshot invalid = reference;
  invalid.temperatureC = std::numeric_limits<float>::quiet_NaN();
  length = encodeSnapshot(invalid, payload, sizeof(payload));
  success = length > 0 && decode();
  success = success && parsed["readings_valid"].is<bool>() &&
      !parsed["readings_valid"].as<bool>() && parsed["filtered_valid"].as<bool>() &&
      parsed["assessment_available"].as<bool>() &&
      !parsed.containsKey("temperature_c") &&
      !parsed["channels"]["temperature"].containsKey("value") &&
      std::strcmp(parsed["channels"]["temperature"]["quality"] | "", "INVALID") == 0 &&
      parsed.containsKey("optical_index_raw") &&
      parsed.containsKey("flow_l_min") && parsed.containsKey("pressure_bar") &&
      parsed.containsKey("optical_index_filtered");
  check("NET JSON: temperatura NaN preserva canais validos e avaliacao optica", success);

  // 3. Média inválida não apaga as medidas brutas válidas.
  invalid = reference;
  invalid.opticalFiltered = std::numeric_limits<float>::quiet_NaN();
  length = encodeSnapshot(invalid, payload, sizeof(payload));
  success = length > 0 && decode();
  success = success && parsed["readings_valid"].as<bool>() &&
      !parsed["filtered_valid"].as<bool>() &&
      parsed.containsKey("temperature_c") && parsed.containsKey("optical_index_raw") &&
      parsed.containsKey("flow_l_min") && parsed.containsKey("pressure_bar") &&
      !parsed.containsKey("optical_index_filtered");
  check("NET JSON: filtro NaN omite somente filtrado", success);

  // 4. Escaping efetivamente produzido por serializeJson e lido de volta.
  Snapshot escaped = reference;
  snprintf(escaped.reason, sizeof(escaped.reason), "aspas \"x\"\nlinha");
  length = encodeSnapshot(escaped, payload, sizeof(payload));
  success = length > 0 && decode();
  success = success && std::strcmp(parsed["reason"] | "", escaped.reason) == 0 &&
      std::strstr(payload, "\\\"x\\\"") != nullptr &&
      std::strstr(payload, "\\nlinha") != nullptr;
  check("NET JSON: aspas e newline fazem round-trip", success);

  // 5. Reenvio de um evento antigo não deve escrever assessment_state/reason
  // correntes. O event_id é estável e inclui dispositivo, boot e event_seq.
  const PendingEvent event = {reference, 7};
  length = encodeEvent(event, payload, sizeof(payload));
  success = length > 0 && decode();
  success = success && !parsed.containsKey("assessment_state") &&
      !parsed.containsKey("reason") && !parsed.containsKey("flow_context") &&
      std::strcmp(parsed["event_state"] | "", reference.state) == 0 &&
      std::strcmp(parsed["event_reason"] | "", reference.reason) == 0 &&
      std::strcmp(parsed["event_id"] | "", "net-test-01:boot-test:7") == 0 &&
      parsed["event_seq"].as<uint32_t>() == 7 &&
      parsed["event_sample_seq"].as<uint32_t>() == reference.seq &&
      parsed["event_uptime_ms"].as<uint32_t>() == reference.uptimeMs &&
      !parsed.containsKey("ts") && !parsed["time_synced"].as<bool>();
  check("NET JSON: evento tem prefixos e ID estavel", success);

  // 6. Verifica a barreira real contra truncamento nos dois encoders.
  char tooSmall[8] = {};
  success = encodeSnapshot(reference, tooSmall, sizeof(tooSmall)) == 0 &&
      encodeEvent(event, tooSmall, sizeof(tooSmall)) == 0;
  check("NET JSON: buffer insuficiente rejeita pacote", success);

  invalid = reference;
  invalid.quality[3] = vigia::Quality::INVALID;
  invalid.readingsValid = false;
  length = encodeSnapshot(invalid, payload, sizeof(payload));
  success = length > 0 && decode() && !parsed.containsKey("pressure_bar") &&
    parsed.containsKey("temperature_c") && parsed.containsKey("optical_index_filtered") &&
    parsed["assessment_available"].as<bool>() &&
    !parsed["channels"]["pressure"]["valid"].as<bool>();
  check("NET JSON: falha de pressao omite somente pressao", success);

  invalid = reference;
  invalid.quality[1] = vigia::Quality::STALE;
  invalid.ageMs[1] = 2000;
  length = encodeSnapshot(invalid, payload, sizeof(payload));
  success = length > 0 && decode() && !parsed.containsKey("optical_index_raw") &&
    !parsed.containsKey("optical_index_filtered") &&
    !parsed["assessment_available"].as<bool>() &&
    parsed["channels"]["optical"]["age_ms"].as<uint32_t>() == 2000 &&
    parsed.containsKey("temperature_c");
  check("NET JSON: optica antiga suspende avaliacao e preserva temperatura", success);

  invalid = reference;
  invalid.pressureBar = 7;
  length = encodeSnapshot(invalid, payload, sizeof(payload));
  success = length > 0 && decode() && !parsed.containsKey("pressure_bar") &&
    std::strcmp(parsed["channels"]["pressure"]["quality"] | "", "OUT_OF_RANGE") == 0;
  check("NET JSON: encoder rejeita numero fora da faixa mesmo com flag valida", success);

  Snapshot pulse = reference;
  pulse.testOverride = true; pulse.overrideMask = 2;
  pulse.assessmentAvailable = false; pulse.filteredValid = false;
  snprintf(pulse.state, sizeof(pulse.state), "UNAVAILABLE");
  snprintf(pulse.reason, sizeof(pulse.reason), "RENEWING");
  snprintf(pulse.observation, sizeof(pulse.observation), "HIGH");
  length = encodeSnapshot(pulse, payload, sizeof(payload));
  success = length > 0 && decode() && parsed["test_override"].as<bool>() &&
    parsed["observation_provisional"].as<bool>() &&
    !parsed["assessment_available"].as<bool>() &&
    std::strcmp(parsed["channels"]["optical"]["source"] | "", "override") == 0 &&
    std::strcmp(parsed["optical_observation"] | "", "HIGH") == 0;
  check("NET JSON: pulso tem origem e observacao provisoria declaradas", success);

  const PendingEvent pulseEvent = {pulse, 8};
  length = encodeEvent(pulseEvent, payload, sizeof(payload));
  success = length > 0 && decode() && parsed["event_test_override"].as<bool>() &&
    parsed["event_observation_provisional"].as<bool>() &&
    parsed.containsKey("event_channels") && !parsed.containsKey("channels") &&
    !parsed.containsKey("assessment_state") && !parsed.containsKey("optical_observation");
  check("NET JSON: evento provisorio nao sobrescreve campos correntes", success);
  return allPassed;
}

}  // namespace net

