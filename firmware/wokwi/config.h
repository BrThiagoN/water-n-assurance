#pragma once
#include <stdint.h>

// water n' assurance / Vigia d'Agua — todas as faixas sao didaticas.
namespace Config {
constexpr uint8_t PIN_TEMP = 4;
constexpr uint8_t PIN_OPTICAL = 32;
constexpr uint8_t PIN_FLOW = 33;
constexpr uint8_t PIN_PRESSURE = 34;
constexpr uint8_t PIN_SDA = 21;
constexpr uint8_t PIN_SCL = 22;
constexpr uint8_t PIN_RED = 16;
constexpr uint8_t PIN_GREEN = 17;
constexpr uint8_t PIN_BLUE = 18;
constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_FAULT = 26;
constexpr uint8_t PIN_MUTE = 27;
constexpr uint8_t OLED_ADDRESS = 0x3c;
constexpr uint32_t SENSOR_INTERVAL_MS = 1000;
constexpr uint32_t TEMP_CONVERSION_MS = 750;
constexpr uint32_t UI_INTERVAL_MS = 200;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 35;
constexpr bool RUN_SELF_TESTS = true;
constexpr bool NETWORK_ENABLED = true;
constexpr char RULE_VERSION[] = "median5-raw3-v2";
constexpr char ALARM_POLICY[] = "continuous-until-ack";

constexpr char DEVICE_ID[] = "vigia-sim-01";
constexpr char WIFI_SSID[] = "Wokwi-GUEST";
constexpr char WIFI_PASSWORD[] = "";
// Placeholder solicitado pelo usuario. No ESP32, localhost e o proprio ESP32,
// nao o computador. Trocar pelo endereco alcancavel do broker quando existir.
constexpr char NETWORK_HOST[] = "localhost";
constexpr uint16_t MQTT_PORT = 1883;
constexpr char MQTT_USERNAME[] = "";
constexpr char MQTT_PASSWORD[] = "";
constexpr uint32_t TELEMETRY_INTERVAL_MS = 5000;
constexpr uint32_t NETWORK_RETRY_MS = 5000;
// Sem TLS somente para o placeholder local/sintetico. Backend real exigira
// configurar transporte, autenticacao e certificado conforme o servidor.
}  // namespace Config

