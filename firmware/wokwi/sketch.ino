/*
 * Prototipo SIMULADO. Indice optico 0..100 nao e NTU nem potabilidade.
 * Regras locais continuam sem rede. Backend MQTT ainda nao configurado.
 * Apoio de IA: documentado em README.md.
 */
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "edge_core.h"
#include "self_tests.h"
#include "network.h"

using namespace vigia;

OneWire oneWire(Config::PIN_TEMP);
DallasTemperature temperatureSensor(&oneWire);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
EdgeController controller;
AlarmLatch waterAlarm;

Inputs lastInput = {0, 0, 0, 0, false, false, false, false};
char bootId[16] = {0};
uint32_t observationSeq = 0;
uint32_t lastSampleAt = 0;
uint32_t lastUiAt = 0;
uint32_t conversionStartedAt = 0;
uint32_t temperatureFreshAt = 0;
float temperatureC = NAN;
bool temperaturePending = false;
bool temperatureReady = false;
bool selfTestsPassed = false;
uint16_t testsPassed = 0, testsFailed = 0;

// As injecoes sao controles de TESTE declarados; nunca fingem ser sensor real.
bool injectStale = false;
uint8_t injectedFaultMask = 0, lastOverrideMask = 0;
uint8_t pulseRemaining = 0;
float pulseValue = 100;
uint32_t channelSampleAt[4] = {0, 0, 0, 0};
bool opticalOverrideOn = false, flowOverrideOn = false;
float opticalOverride = 10.0f, flowOverride = 6.0f;
bool muted = false;
bool buttonRaw = HIGH, buttonStable = HIGH;
uint32_t buttonChangedAt = 0;
bool viewObserved = false;
AssessmentState previousState = AssessmentState::UNAVAILABLE;
Reason previousReason = Reason::INITIALIZING;
FlowContext previousFlow = FlowContext::UNKNOWN;
WaterState previousWater = WaterState::UNKNOWN;
Observation previousObservation = Observation::NONE;
Quality previousQuality[4] = {Quality::NOT_READY, Quality::NOT_READY,
                             Quality::NOT_READY, Quality::NOT_READY};

enum class SoundPattern : uint8_t { NONE, FAULT };
SoundPattern soundPattern = SoundPattern::NONE;
uint32_t soundStartedAt = 0;
bool piezoOn = false;
char commandBuffer[96];
size_t commandLength = 0;
bool commandOverflow = false;

bool elapsed(uint32_t now, uint32_t then, uint32_t interval) {
  return static_cast<uint32_t>(now - then) >= interval;
}

bool inTestMode() {
  return injectStale || injectedFaultMask || opticalOverrideOn || flowOverrideOn ||
    pulseRemaining || lastOverrideMask;
}

void reportSelfTest(const char* name, bool passed) {
  Serial.printf("[TEST] %s %s\n", passed ? "PASS" : "FAIL", name);
}

void setPiezo(bool on, unsigned frequency = 1800) {
  if (on == piezoOn) return;
  piezoOn = on;
  if (on) tone(Config::PIN_BUZZER, frequency);
  else noTone(Config::PIN_BUZZER);
}

void startSound(SoundPattern pattern, uint32_t now) {
  setPiezo(false);
  soundPattern = pattern;
  soundStartedAt = now;
}

void silenceSound() {
  muted = true;
  waterAlarm.silence();
  soundPattern = SoundPattern::NONE;
  setPiezo(false);
  Serial.println("[UI] Som silenciado; estado e historico preservados.");
}

void resumeSound() {
  muted = false;
  soundPattern = SoundPattern::NONE;
  const View v = controller.view();
  const bool confirmedAlert = v.state == AssessmentState::ATTENTION ||
    v.state == AssessmentState::CHECK;
  if (waterAlarm.resume(confirmedAlert))
    Serial.println("[UI] Som reativado; alerta confirmado continua ativo.");
  else
    Serial.println("[UI] Som reativado para os proximos avisos.");
}

void toggleAlarmSound() {
  if (muted) {
    resumeSound();
  } else if (waterAlarm.active() || piezoOn || soundPattern != SoundPattern::NONE) {
    silenceSound();
  } else {
    Serial.println("[UI] Sem aviso sonoro; som continua habilitado.");
  }
}

void updateSound(uint32_t now) {
  if (muted) {
    setPiezo(false);
    return;
  }
  if (waterAlarm.active()) {
    setPiezo(true);  // Continuous tone, no timeout; only acknowledgement/reset clears it.
    return;
  }
  const bool faultTone = soundPattern == SoundPattern::FAULT &&
    static_cast<uint32_t>(now - soundStartedAt) < 600;
  setPiezo(faultTone);
  if (!faultTone) soundPattern = SoundPattern::NONE;
}

const char* reasonText(Reason reason) {
  switch (reason) {
    case Reason::INITIALIZING: return "Inicializando";
    case Reason::SENSOR_INVALID: return "Sensor inválido";
    case Reason::SENSOR_STALE: return "Dado antigo";
    case Reason::NO_FLOW: return "Sem circulação";
    case Reason::RENEWING: return "Renovando amostra";
    case Reason::FILTER_WARMUP: return "Reunindo leituras";
    case Reason::CONFIRMING: return "Confirmando leitura";
    default: return "Parâmetros observados";
  }
}

void textAt(uint8_t x, uint8_t y, const char* text) {
  oled.setCursor(x, y);
  oled.print(text);
}

void drawStateIcon(AssessmentState state) {
  switch (state) {
    case AssessmentState::NO_CHANGE:
      oled.drawLine(112, 23, 116, 27); oled.drawLine(116, 27, 124, 17); break;
    case AssessmentState::ATTENTION:
      oled.drawTriangle(118, 16, 111, 29, 125, 29);
      oled.drawVLine(118, 20, 4); oled.drawPixel(118, 26); break;
    case AssessmentState::CHECK:
      oled.drawFrame(112, 16, 12, 14);
      oled.drawVLine(118, 19, 6); oled.drawPixel(118, 27); break;
    default:
      oled.drawLine(113, 18, 123, 28); oled.drawLine(123, 18, 113, 28); break;
  }
}

void refreshUi(uint32_t now) {
  View v = controller.view();
  uint8_t r = 0, g = 0, b = 0;
  if (!selfTestsPassed) r = (now / 250) % 2 ? 255 : 0;
  else if (v.state == AssessmentState::NO_CHANGE) g = 180;
  else if (v.state == AssessmentState::ATTENTION) r = g = 180;
  else if (v.state == AssessmentState::CHECK) r = 220;
  else r = g = b = (now / 500) % 2 ? 70 : 0;
  analogWrite(Config::PIN_RED, r);
  analogWrite(Config::PIN_GREEN, g);
  analogWrite(Config::PIN_BLUE, b);

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  textAt(0, 10, inTestMode() ? "Vigia [TESTE]" : "Vigia d'Água");
  textAt(108, 10, net::online() ? "ON" : "OFF");
  oled.drawHLine(0, 12, 128);
  if (!selfTestsPassed) {
    textAt(0, 27, "Falha nos testes");
    textAt(0, 43, "Confira o Serial");
    oled.sendBuffer();
    return;
  }
  drawStateIcon(v.state);
  switch (v.state) {
    case AssessmentState::NO_CHANGE:
      textAt(0, 26, "Sem alteração"); textAt(0, 38, "detectada"); break;
    case AssessmentState::ATTENTION:
      textAt(0, 26, "Atenção"); textAt(0, 38, "Mudança em observação"); break;
    case AssessmentState::CHECK:
      textAt(0, 26, "Verifique"); textAt(0, 38, "Mudança persistente"); break;
    default:
      textAt(0, 26, "Leitura"); textAt(0, 38, "indisponível"); break;
  }
  if (v.state == AssessmentState::UNAVAILABLE) {
    const char* reason = reasonText(v.reason);
    if (v.reason == Reason::SENSOR_INVALID)
      reason = v.quality[1] != Quality::VALID ? "Falha na óptica" : "Falha na vazão";
    textAt(0, 50, reason);
    if (v.observation != Observation::NONE && (now / 2000) % 2 == 0)
      textAt(0, 62, v.observation == Observation::ELEVATED ? "Sinal alto (parcial)" : "Mudança a confirmar");
    else if (v.waterState == WaterState::CHECK) textAt(0, 62, "Último: Verifique");
    else if (v.waterState == WaterState::ATTENTION) textAt(0, 62, "Último: Atenção");
    else if (v.waterState == WaterState::NO_CHANGE) textAt(0, 62, "Último: sem alteração");
    else textAt(0, 62, "Sem avaliação atual");
  } else if (waterAlarm.active() && v.state == AssessmentState::NO_CHANGE) {
    textAt(0, 50, "Aviso pendente"); textAt(0, 62, "Pressione Silenciar");
  } else if (v.state == AssessmentState::ATTENTION || v.state == AssessmentState::CHECK) {
    textAt(0, 50, muted ? "Som silenciado" : "Alarme sonoro ativo");
    textAt(0, 62, muted ? "Toque p/ reativar" : "Toque p/ silenciar");
  } else if (v.quality[0] != Quality::VALID || v.quality[3] != Quality::VALID) {
    textAt(0, 50, "Óptica avaliada");
    const bool showTemp = v.quality[0] != Quality::VALID &&
      (v.quality[3] == Quality::VALID || (now / 2000) % 2 == 0);
    textAt(0, 62, showTemp ? "Temp. indisponível" : "Pressão indisponível");
  } else if (v.state == AssessmentState::NO_CHANGE && (now / 4000) % 2 == 0) {
    textAt(0, 50, "Nos parâmetros"); textAt(0, 62, "acompanhados");
  } else {
    char row[32];
    snprintf(row, sizeof(row), "T:%.1fC  P:%.1fbar", lastInput.temperatureC, lastInput.pressureBar);
    textAt(0, 50, row);
    snprintf(row, sizeof(row), "Opt:%.0f Q:%.1fL/m", v.filteredOptical, lastInput.flowLpm);
    textAt(0, 62, muted && (now / 2000) % 2 == 0 ? "Som silenciado" : row);
  }
  oled.sendBuffer();
}

bool readingsAreValid(uint32_t now, const View& v) {
  (void)now;
  for (uint8_t i = 0; i < 4; ++i) if (v.quality[i] != Quality::VALID) return false;
  return true;
}

net::Snapshot makeSnapshot(uint32_t now) {
  net::Snapshot s = {};
  View v = controller.view();
  snprintf(s.deviceId, sizeof(s.deviceId), "%s", Config::DEVICE_ID);
  snprintf(s.bootId, sizeof(s.bootId), "%s", bootId);
  s.seq = observationSeq;
  s.uptimeMs = now;
  s.temperatureC = lastInput.temperatureC;
  s.opticalRaw = lastInput.optical;
  s.opticalFiltered = v.filteredOptical;
  s.flowLpm = lastInput.flowLpm;
  s.pressureBar = lastInput.pressureBar;
  s.readingsValid = readingsAreValid(now, v);
  s.filteredValid = v.filteredValid && v.state != AssessmentState::UNAVAILABLE;
  s.assessmentAvailable = v.state != AssessmentState::UNAVAILABLE;
  s.testOverride = inTestMode();
  s.overrideMask = lastOverrideMask;
  for (uint8_t i = 0; i < 4; ++i) {
    s.quality[i] = v.quality[i];
    s.ageMs[i] = now - channelSampleAt[i];
  }
  snprintf(s.observation, sizeof(s.observation), "%s", observationName(v.observation));
  snprintf(s.state, sizeof(s.state), "%s", assessmentName(v.state));
  snprintf(s.reason, sizeof(s.reason), "%s", reasonName(v.reason));
  snprintf(s.flowContext, sizeof(s.flowContext), "%s", flowName(v.flow));
  const char* status = "UNKNOWN";
  const char* alert = "UNKNOWN";
  if (v.waterState == WaterState::NO_CHANGE) { status = "CLEARED"; alert = "NONE"; }
  else if (v.waterState == WaterState::CHECK || v.waterState == WaterState::ATTENTION) {
    status = "ACTIVE"; alert = waterName(v.waterState);
  }
  snprintf(s.lastAlertStatus, sizeof(s.lastAlertStatus), "%s", status);
  snprintf(s.lastAlertState, sizeof(s.lastAlertState), "%s", alert);
  return s;
}

void printStatus(uint32_t now, const char* type = "sample") {
  View v = controller.view();
  net::Snapshot s = makeSnapshot(now);
  JsonDocument doc;
  doc["type"] = type;
  doc["schema_version"] = 2;
  doc["rule_version"] = Config::RULE_VERSION;
  doc["alarm_policy"] = Config::ALARM_POLICY;
  doc["simulated"] = true;
  doc["assessment_scope"] = "relative_optical_change";
  doc["seq"] = s.seq;
  doc["uptime_ms"] = now;
  doc["state"] = s.state;
  doc["reason"] = s.reason;
  doc["water_state"] = waterName(v.waterState);
  doc["flow_context"] = s.flowContext;
  doc["valid"] = s.readingsValid;
  doc["assessment_available"] = s.assessmentAvailable;
  doc["optical_observation"] = s.observation;
  doc["observation_provisional"] = v.observation != Observation::NONE;
  doc["age_ms"] = v.hasFresh ? static_cast<uint32_t>(now - v.lastFreshMs) : 0;
  doc["has_fresh"] = v.hasFresh;
  doc["window"] = v.windowSize;
  doc["assessment_seq"] = v.assessmentSeq;
  const char* names[] = {"temperature", "optical", "flow", "pressure"};
  const char* fields[] = {"temperature_c", "optical_raw", "flow_l_min", "pressure_bar"};
  const char* units[] = {"degC", "relative_index", "L/min", "bar"};
  const float values[] = {s.temperatureC, s.opticalRaw, s.flowLpm, s.pressureBar};
  for (uint8_t i = 0; i < 4; ++i) {
    JsonObject ch = doc["channels"][names[i]].to<JsonObject>();
    ch["quality"] = qualityName(s.quality[i]);
    ch["valid"] = s.quality[i] == Quality::VALID;
    ch["unit"] = units[i];
    ch["source"] = s.overrideMask & (1u << i) ? "override" : "simulator";
    if (s.quality[i] != Quality::NOT_READY) ch["age_ms"] = s.ageMs[i];
    if (s.quality[i] == Quality::VALID) {
      doc[fields[i]] = values[i];
      ch["value"] = values[i];
    }
  }
  if (s.filteredValid) doc["optical_filtered"] = s.opticalFiltered;
  doc["network"] = net::online() ? "ONLINE" : "OFFLINE";
  doc["test_override"] = inTestMode();
  doc["muted"] = muted;
  doc["alarm_latched"] = waterAlarm.active();
  doc["buzzer_on"] = piezoOn;
  doc["sent"] = net::sentCount();
  doc["dropped_events"] = net::droppedEvents();
  serializeJson(doc, Serial);
  Serial.println();
}

void observeTransitions(uint32_t now) {
  View v = controller.view();
  bool changed = !viewObserved || v.state != previousState || v.reason != previousReason ||
    v.flow != previousFlow || v.waterState != previousWater || v.observation != previousObservation;
  for (uint8_t i = 0; i < 4; ++i) changed = changed || v.quality[i] != previousQuality[i];
  if (!changed) return;
  const bool newWaterAlert = waterAlarm.observe(v.waterState);
  bool newFault = v.state == AssessmentState::UNAVAILABLE &&
    (v.reason == Reason::SENSOR_INVALID || v.reason == Reason::SENSOR_STALE) &&
    (!viewObserved || previousState != v.state || previousReason != v.reason);
  if (v.state == AssessmentState::UNAVAILABLE && !waterAlarm.active()) {
    soundPattern = SoundPattern::NONE;
    setPiezo(false);
  }
  if (newWaterAlert) {
    muted = false;
    soundPattern = SoundPattern::NONE;
  } else if (newFault && !waterAlarm.active()) {
    muted = false;
    startSound(SoundPattern::FAULT, now);
  } else if (v.state == AssessmentState::NO_CHANGE && !waterAlarm.active()) {
    muted = false;
    soundPattern = SoundPattern::NONE;
    setPiezo(false);
  }
  previousState = v.state; previousReason = v.reason;
  previousFlow = v.flow; previousWater = v.waterState;
  previousObservation = v.observation;
  for (uint8_t i = 0; i < 4; ++i) previousQuality[i] = v.quality[i];
  viewObserved = true;
  ++observationSeq;
  printStatus(now, "event");
  net::offer(makeSnapshot(now), true);
}

void updateButton(uint32_t now) {
  bool raw = digitalRead(Config::PIN_MUTE);
  if (raw != buttonRaw) { buttonRaw = raw; buttonChangedAt = now; }
  if (buttonStable != raw && elapsed(now, buttonChangedAt, Config::BUTTON_DEBOUNCE_MS)) {
    buttonStable = raw;
    if (buttonStable == LOW) toggleAlarmSound();
  }
}

void updateTemperature(uint32_t now) {
  if (temperaturePending && elapsed(now, conversionStartedAt, Config::TEMP_CONVERSION_MS)) {
    temperatureC = temperatureSensor.getTempCByIndex(0);
    temperatureFreshAt = now;
    temperatureReady = true;
    temperaturePending = false;
  }
  if (!temperaturePending && elapsed(now, conversionStartedAt, Config::SENSOR_INTERVAL_MS)) {
    temperatureSensor.requestTemperatures();
    conversionStartedAt = now;
    temperaturePending = true;
  }
}

void acquireInputs(uint32_t now) {
  if (injectStale) return;  // Nem valor nem timestamp do lote sao renovados.
  Inputs next = {};
  next.temperatureC = temperatureC;
  const bool pulsing = pulseRemaining > 0;
  next.optical = pulsing ? pulseValue :
    (opticalOverrideOn ? opticalOverride : analogRead(Config::PIN_OPTICAL) * (100.0f / 4095.0f));
  next.flowLpm = flowOverrideOn ? flowOverride : analogRead(Config::PIN_FLOW) * (20.0f / 4095.0f);
  next.pressureBar = analogRead(Config::PIN_PRESSURE) * (6.0f / 4095.0f);
  lastOverrideMask = ((pulsing || opticalOverrideOn) ? 2 : 0) | (flowOverrideOn ? 4 : 0);
  if (pulsing) --pulseRemaining;
  channelSampleAt[0] = temperatureFreshAt;
  for (uint8_t i = 1; i < 4; ++i) channelSampleAt[i] = now;
  next.tempValid = !(injectedFaultMask & 1) && temperatureReady && !elapsed(now, temperatureFreshAt, 2001) &&
    isfinite(temperatureC) && temperatureC >= -55.0f && temperatureC <= 125.0f &&
    temperatureC != DEVICE_DISCONNECTED_C;
  next.opticalValid = !(injectedFaultMask & 2) && digitalRead(Config::PIN_FAULT) == HIGH;
  next.flowValid = !(injectedFaultMask & 4);
  next.pressureValid = !(injectedFaultMask & 8);
  lastInput = next;
  controller.ingest(now, next);
}

void printHelp() {
  Serial.println("COMANDOS: HELP | STATUS | MUTE | RESET");
  Serial.println("TEST FAULT ON/OFF | TEST STALE ON/OFF | TEST NET ON/OFF");
  Serial.println("TEST FAULT TEMP/OPTICAL/FLOW/PRESSURE ON/OFF | TEST PULSE <0..100> <1..10 amostras>");
  Serial.println("TEST OPTICAL <0..100>/OFF | TEST FLOW <0..20>/OFF");
  Serial.println("Overrides sao TESTES; OFF volta ao potenciometro. NET ON habilita rede.");
}

bool parseValue(const String& text, float maximum, float& result) {
  if (text.length() == 0) return false;
  char* end = nullptr;
  result = strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0' && isfinite(result) && result >= 0 && result <= maximum;
}

void processCommand(char* input) {
  String cmd(input);
  cmd.trim(); cmd.toUpperCase();
  uint32_t now = millis();
  if (cmd.length() == 0) return;
  if (cmd == "HELP") printHelp();
  else if (cmd == "STATUS") printStatus(now, "status");
  else if (cmd == "MUTE") silenceSound();
  else if (cmd == "RESET") {
    injectStale = opticalOverrideOn = flowOverrideOn = false;
    injectedFaultMask = lastOverrideMask = pulseRemaining = 0;
    muted = false; soundPattern = SoundPattern::NONE; setPiezo(false);
    waterAlarm.reset();
    controller.begin(now); viewObserved = false;
    net::setEnabled(Config::NETWORK_ENABLED);
    Serial.println("[TEST] RESET logico: filtros/contexto limpos; boot_id preservado; rede habilitada.");
  } else if (cmd == "TEST STALE ON") injectStale = true;
  else if (cmd == "TEST STALE OFF") injectStale = false;
  else if (cmd == "TEST FAULT ON") injectedFaultMask |= 2;
  else if (cmd == "TEST FAULT OFF") injectedFaultMask &= ~2u;
  else if (cmd.startsWith("TEST FAULT ")) {
    String args = cmd.substring(11);
    const int sep = args.indexOf(' ');
    const String channel = sep < 0 ? "" : args.substring(0, sep);
    const String action = sep < 0 ? "" : args.substring(sep + 1);
    const uint8_t mask = channel == "TEMP" ? 1 : channel == "OPTICAL" ? 2 :
      channel == "FLOW" ? 4 : channel == "PRESSURE" ? 8 : 0;
    if (!mask || (action != "ON" && action != "OFF")) {
      Serial.println("[CMD] ERRO: FAULT TEMP/OPTICAL/FLOW/PRESSURE ON/OFF."); return;
    }
    if (action == "ON") injectedFaultMask |= mask; else injectedFaultMask &= ~mask;
  }
  else if (cmd.startsWith("TEST PULSE ")) {
    String args = cmd.substring(11);
    const int sep = args.indexOf(' ');
    float value = 0, count = 0;
    if (sep < 0 || !parseValue(args.substring(0, sep), 100, value) ||
        !parseValue(args.substring(sep + 1), 10, count) || count < 1 || floorf(count) != count) {
      Serial.println("[CMD] ERRO: PULSE <0..100> <1..10 amostras inteiras>."); return;
    }
    pulseValue = value; pulseRemaining = static_cast<uint8_t>(count);
  }
  else if (cmd == "TEST NET ON") net::setEnabled(true);
  else if (cmd == "TEST NET OFF") net::setEnabled(false);
  else if (cmd == "TEST OPTICAL OFF") opticalOverrideOn = false;
  else if (cmd == "TEST FLOW OFF") flowOverrideOn = false;
  else if (cmd.startsWith("TEST OPTICAL ")) {
    float parsed = 0;
    if (!parseValue(cmd.substring(13), 100.0f, parsed)) {
      Serial.println("[CMD] ERRO: OPTICAL precisa estar em 0..100."); return;
    }
    opticalOverride = parsed;
    opticalOverrideOn = true;
  } else if (cmd.startsWith("TEST FLOW ")) {
    float parsed = 0;
    if (!parseValue(cmd.substring(10), 20.0f, parsed)) {
      Serial.println("[CMD] ERRO: FLOW precisa estar em 0..20."); return;
    }
    flowOverride = parsed;
    flowOverrideOn = true;
  } else { Serial.println("[CMD] Comando desconhecido. Digite HELP."); return; }
  Serial.printf("[CMD] OK %s\n", cmd.c_str());
}

void pollSerial() {
  uint8_t budget = 48;
  while (Serial.available() && budget--) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (commandOverflow) Serial.println("[CMD] ERRO: comando muito longo.");
      else { commandBuffer[commandLength] = '\0'; processCommand(commandBuffer); }
      commandLength = 0; commandOverflow = false;
    } else if (commandLength < sizeof(commandBuffer) - 1) commandBuffer[commandLength++] = ch;
    else commandOverflow = true;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(Config::PIN_RED, OUTPUT); pinMode(Config::PIN_GREEN, OUTPUT);
  pinMode(Config::PIN_BLUE, OUTPUT); pinMode(Config::PIN_BUZZER, OUTPUT);
  pinMode(Config::PIN_FAULT, INPUT_PULLUP); pinMode(Config::PIN_MUTE, INPUT_PULLUP);
  analogReadResolution(12);
  Wire.begin(Config::PIN_SDA, Config::PIN_SCL);
  oled.setI2CAddress(Config::OLED_ADDRESS << 1);
  oled.begin(); oled.enableUTF8Print();
  Serial.println("\nwater n' assurance / Vigia d'Agua / 1ESPV-2026");
  Serial.println("Thiago Gomes Nascimento RM569436 | Rafael De Sousa RM568777 | Vitor Assis Florenzano RM572192");
  Serial.println("SIMULACAO: indice optico relativo; nao comprova potabilidade.");
  Serial.printf("[RULE] %s; mediana5 + 3 confirmacoes apoiadas por amostras novas.\n", Config::RULE_VERSION);
  selfTestsPassed = true;
  if (Config::RUN_SELF_TESTS) {
    selfTestsPassed = vigia::runSelfTests(reportSelfTest, testsPassed, testsFailed);
    selfTestsPassed = net::runSelfTests(reportSelfTest, testsPassed, testsFailed) && selfTestsPassed;
  }
  Serial.printf("[TEST] SUMMARY passed=%u failed=%u result=%s\n", testsPassed, testsFailed, selfTestsPassed ? "PASS" : "FAIL");
  snprintf(bootId, sizeof(bootId), "%08lx", static_cast<unsigned long>(esp_random()));
  controller.begin(millis());
  if (!selfTestsPassed) { refreshUi(millis()); return; }
  temperatureSensor.begin(); temperatureSensor.setResolution(12);
  temperatureSensor.setWaitForConversion(false);
  temperatureSensor.requestTemperatures();
  conversionStartedAt = millis(); temperaturePending = true;
  lastSampleAt = millis();
  buttonRaw = buttonStable = digitalRead(Config::PIN_MUTE);
  net::setEnabled(Config::NETWORK_ENABLED);
  net::begin();
  Serial.printf("[NET] Backend pendente: MQTT %s:%u; decisao local independente.\n", Config::NETWORK_HOST, Config::MQTT_PORT);
  printHelp();
  refreshUi(millis());
}

void loop() {
  pollSerial();
  // RESET pode atualizar o tempo-base do motor; capturar agora DEPOIS dele.
  uint32_t now = millis();
  updateButton(now);
  if (selfTestsPassed) {
    updateTemperature(now);
    if (elapsed(now, lastSampleAt, Config::SENSOR_INTERVAL_MS)) {
      lastSampleAt = now;
      acquireInputs(now);
      ++observationSeq;
      printStatus(now);
      net::offer(makeSnapshot(now), false);
    }
    controller.tick(now);
    observeTransitions(now);
    updateSound(now);
  }
  if (elapsed(now, lastUiAt, Config::UI_INTERVAL_MS)) {
    lastUiAt = now; refreshUi(now);
  }
  // Cede um tick ao escalonador, sem esperas de sensor/rede/som neste loop.
  // Os intervalos de trabalho continuam controlados por millis().
  vTaskDelay(1);
}

