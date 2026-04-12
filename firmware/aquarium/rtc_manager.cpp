#include "rtc_manager.h"
#include "config.h"
#include "light.h"
#include "fan.h"
#include "wifi_manager.h"
#include <RTClib.h>
#include <Wire.h>
#include <time.h>   // getLocalTime(), configTime() — ESP32 Arduino core

static RTC_DS3231 _rtc;
static bool       _available      = false;

// Horários configurados em config.h
static const int _on_hour    = RTC_ON_HOUR;
static const int _on_minute  = RTC_ON_MIN;
static const int _off_hour   = RTC_OFF_HOUR;
static const int _off_minute = RTC_OFF_MIN;

// Rastreia o último período (ligado/desligado) para só agir na transição.
// Isso garante que sobrescritas manuais dentro de um período sejam respeitadas
// até a próxima mudança de janela horária.
static bool _last_period_on = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static String _format_time(int h, int m) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  return String(buf);
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

void rtc_init() {
  Wire.begin();
  _available = _rtc.begin();

  if (_available) {
    if (_rtc.lostPower()) {
      // Bateria fraca: hora pode estar errada — será corrigida pelo NTP em seguida
      Serial.println("[RTC] Bateria fraca ou primeiro uso. Hora sera ajustada via NTP.");
    }
    Serial.print("[RTC] DS3231SN encontrado. Hora atual: ");
    Serial.println(rtc_get_time_str());
  } else {
    Serial.println("[RTC] DS3231SN NAO encontrado. Verifique conexao I2C (SDA=21, SCL=22).");
  }
}

bool rtc_sync_ntp() {
  if (!wifi_is_connected()) {
    Serial.println("[RTC] WiFi indisponivel. Pulando sincronizacao NTP sem bloquear o sistema.");
    return false;
  }

  // Configura SNTP com fuso horário de Brasília (UTC-3, sem horário de verão)
  configTime(NTP_UTC_OFFSET, 0, NTP_SERVER1, NTP_SERVER2);

  Serial.print("[RTC] Sincronizando com NTP (Brasilia UTC-3)");

  struct tm timeinfo;
  bool synced = false;
  for (int i = 0; i < 8; i++) {
    if (getLocalTime(&timeinfo)) {
      synced = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (!synced) {
    Serial.println("[RTC] Timeout NTP. Usando hora ja armazenada no modulo.");
    return false;
  }

  // Atualiza o DS3231SN com a hora obtida via NTP
  if (_available) {
    DateTime dt(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon  + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    );
    _rtc.adjust(dt);
    Serial.print("[RTC] Hora ajustada via NTP: ");
    Serial.println(rtc_get_time_str());
  } else {
    Serial.println("[RTC] NTP OK mas modulo RTC indisponivel — usando hora do sistema ESP32.");
  }

  return true;
}

String rtc_get_time_str() {
  if (!_available) return "--:--";
  DateTime now = _rtc.now();
  return _format_time(now.hour(), now.minute());
}

bool rtc_available() {
  return _available;
}

void rtc_check_automation() {
  if (!_available) return;

  DateTime now = _rtc.now();

  // Converte os horários para minutos desde meia-noite para facilitar a comparação
  int current = now.hour()   * 60 + now.minute();
  int on_at   = _on_hour     * 60 + _on_minute;
  int off_at  = _off_hour    * 60 + _off_minute;

  bool should_be_on;
  if (on_at < off_at) {
    // Caso normal: ex. 10:00 – 17:00
    should_be_on = (current >= on_at && current < off_at);
  } else {
    // Cruzando meia-noite: ex. 20:00 – 06:00
    should_be_on = (current >= on_at || current < off_at);
  }

  // Só age quando o período muda (on → off ou off → on).
  // Dentro do mesmo período, sobrescritas manuais são preservadas.
  if (should_be_on != _last_period_on) {
    _last_period_on = should_be_on;
    light_set(should_be_on);
    if (should_be_on) {
      // Na transição para o período ON: reseta a ventoinha para modo AUTO.
      // Overrides manuais anteriores são descartados; pot requer re-calibração.
      fan_on_rtc_reset();
    }
    Serial.printf("[RTC] Automacao: %s luminaria\n", should_be_on ? "ligando" : "desligando");
  }
}

String rtc_get_on_time() {
  return _format_time(_on_hour, _on_minute);
}

String rtc_get_off_time() {
  return _format_time(_off_hour, _off_minute);
}
