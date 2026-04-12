#pragma once

// =============================================================================
// CONFIGURAÇÃO — copie este arquivo para config.h e preencha os valores reais
// config.h está no .gitignore e nunca deve ser versionado
// =============================================================================

// --- Wi-Fi -------------------------------------------------------------------
#define WIFI_SSID      "YOUR_NETWORK_HERE"
#define WIFI_PASSWORD  "YOUR_PASSWORD_HERE"
#define WIFI_RECONNECT_INTERVAL_MS  10000UL   // tentativa de reconexão (não-bloqueante)
#define WIFI_RECOVERY_TIMEOUT_MS    180000UL  // após esse tempo offline, habilita AP de recuperação
#define WIFI_RECOVERY_MAX_ATTEMPTS  12        // ou após esse número de tentativas sem sucesso
#define WIFI_RECOVERY_AP_SSID_PREFIX "Aquarium-Setup"

// --- Rede (IP estático) ------------------------------------------------------
#define NET_LOCAL_IP   "10.141.68.50"
#define NET_GATEWAY    "10.141.68.29"
#define NET_SUBNET     "255.255.255.0"

// --- Pinos de hardware -------------------------------------------------------
#define PIN_SSR        23   // Relé SSR40DA — controle da luminária LED
#define PIN_DS18B20     4   // DS18B20 — sensor de temperatura (OneWire)
#define PIN_BUTTON     18   // Push button — liga/desliga luminária (INPUT_PULLUP)
#define PIN_POT        34   // Potenciômetro B10K — controle de velocidade da ventoinha (ADC)
#define PIN_FAN        25   // Saída PWM da ventoinha (LEDC — evitar pinos ADC2 com Wi-Fi ativo)

// DS3231SN usa I2C padrão do ESP32: SDA = GPIO 21, SCL = GPIO 22

// --- Horário automático da luminária (DS3231SN) ------------------------------
// A automação é sempre ativa: o botão físico/web apenas sobrepõe o estado
// atual e a rotina volta a agir na próxima transição de horário.
#define RTC_ON_HOUR    10   // hora de ligar  (0–23)
#define RTC_ON_MIN      0   // minuto de ligar (0–59)
#define RTC_OFF_HOUR   17   // hora de desligar (0–23)
#define RTC_OFF_MIN     0   // minuto de desligar (0–59)

// --- NTP (sincronização de horário ao inicializar com Wi-Fi) -----------------
// Fuso: Brasília = UTC-3 (sem horário de verão desde 2019)
#define NTP_SERVER1      "pool.ntp.org"
#define NTP_SERVER2      "time.google.com"
#define NTP_UTC_OFFSET   (-3 * 3600)   // UTC-3 em segundos

// --- Controle automático da ventoinha ----------------------------------------
// Histérese: liga acima de TRIGGER, inicia cooldown abaixo de OFF.
// O intervalo entre os dois valores evita liga/desliga rápido (hunting).
#define TEMP_FAN_TRIGGER    29.0f   // °C — temperatura para ligar a ventoinha
#define TEMP_FAN_OFF        27.5f   // °C — temperatura para iniciar cooldown (< TRIGGER)
#define FAN_COOLDOWN_MIN    30      // minutos de funcionamento após atingir TEMP_FAN_OFF

// Velocidades escalonadas por Δ = temperatura – TEMP_FAN_TRIGGER (%)
#define FAN_SPEED_LOW       30      // Δ ≤ 1,0°C — levemente acima do limiar
#define FAN_SPEED_MED       55      // Δ ≤ 2,0°C — aquecimento moderado
#define FAN_SPEED_HIGH      80      // Δ ≤ 3,0°C — aquecimento significativo
#define FAN_SPEED_MAX       100     // Δ >  3,0°C — situação crítica
#define FAN_FAILSAFE_SPEED  30      // velocidade usada no AUTO se temperatura ficar indisponível

// Dados de temperatura são considerados stale após este tempo sem leitura válida
#define TEMP_MAX_STALE_MS   5000UL

// --- API local ESP32 (controle remoto) ---------------------------------------
// Se vazio, desativa autenticação da API (NÃO recomendado em produção).
#define API_AUTH_TOKEN      "REPLACE_WITH_LONG_RANDOM_TOKEN"

// Origem permitida para CORS (frontend HTTPS). Deixe vazio para desabilitar CORS.
// Ex.: "https://aquarium.seudominio.com"
#define CORS_ALLOWED_ORIGIN ""

// --- OTA (atualização de firmware via rede) ----------------------------------
// Interface acessível em: http://<IP_DO_ESP32>/update
// Também protege o portal de recuperação Wi-Fi: /wifi-setup
#define OTA_USERNAME   "admin"
#define OTA_PASSWORD   "aquarium"
