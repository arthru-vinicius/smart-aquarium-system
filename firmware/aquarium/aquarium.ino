#include "config.h"
#include "wifi_manager.h"
#include "light.h"
#include "temperature.h"
#include "fan.h"
#include "rtc_manager.h"
#include "web_server.h"

static bool          _ntp_synced         = false;
static unsigned long _last_ntp_attempt   = 0;
static const unsigned long NTP_RETRY_MS  = 60000UL;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== AQUARIUM CONTROLLER STARTING ===");

  light_init();
  temperature_init();
  fan_init();
  rtc_init();
  wifi_connect();
  _ntp_synced = rtc_sync_ntp();  // tenta sincronizar via NTP sem bloquear startup
  if (!_ntp_synced) _last_ntp_attempt = millis();
  webserver_init();  // sobe API REST + ElegantOTA

  Serial.println("=== SYSTEM READY ===");
}

void loop() {
  wifi_check_reconnect();  // reconecta automaticamente se Wi-Fi cair

  // Tenta sincronizar RTC quando a rede voltar (sem impactar operação principal)
  if (!_ntp_synced && wifi_is_connected()) {
    unsigned long now = millis();
    if (_last_ntp_attempt == 0 || (now - _last_ntp_attempt) >= NTP_RETRY_MS) {
      _last_ntp_attempt = now;
      _ntp_synced = rtc_sync_ntp();
    }
  }

  light_check_button();    // verifica push button físico com debounce
  temperature_update();    // gerencia conversão não-bloqueante do DS18B20 (~750 ms)
  fan_update();            // controle completo da ventoinha (pot + modo auto + histérese)
  rtc_check_automation();  // aciona luminária/ventoinha na transição de horário (DS3231SN)
  webserver_loop();        // manutenção do ElegantOTA
  delay(200);
}
