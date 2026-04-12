#include "fan.h"
#include "config.h"
#include "temperature.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Configuração do PWM (LEDC)
// ---------------------------------------------------------------------------
static const int LEDC_CHANNEL    = 0;
static const int LEDC_FREQ_HZ    = 25000;   // 25 kHz — frequência típica para ventoinhas PWM 4 pinos
static const int LEDC_RESOLUTION = 8;       // 8 bits → valores 0–255

#ifndef FAN_FAILSAFE_SPEED
#define FAN_FAILSAFE_SPEED FAN_SPEED_LOW
#endif

// ---------------------------------------------------------------------------
// Thresholds do potenciômetro
// ---------------------------------------------------------------------------
// ADC abaixo deste valor é interpretado como "mínimo" (ventoinha off via pot)
static const int POT_MIN_ADC    = 80;    // em unidades de 0–4095
// Histerese para sair da região de "mínimo" e evitar comutações por ruído
static const int POT_MIN_EXIT_ADC = POT_MIN_ADC + 40;
// Variação mínima no ADC para considerar que o usuário moveu o pot intencionalmente
static const int POT_CHANGE_ADC = 60;

// ---------------------------------------------------------------------------
// Modos de operação
// ---------------------------------------------------------------------------
/**
 * Modo de operação da ventoinha.
 * FAN_MODE_AUTO:          Controle automático por temperatura (histérese + cooldown).
 * FAN_MODE_MANUAL_OFF:    Desligada manualmente (pot ou web) — aguarda próximo RTC_ON.
 * FAN_MODE_MANUAL_SPEED:  Velocidade fixada manualmente (pot ou web) — aguarda próximo RTC_ON.
 */
enum FanMode {
  FAN_MODE_AUTO,
  FAN_MODE_MANUAL_OFF,
  FAN_MODE_MANUAL_SPEED,
};

/**
 * Sub-estado interno do modo AUTO.
 * FAN_AUTO_IDLE:      Temperatura abaixo do trigger — ventoinha desligada.
 * FAN_AUTO_RUNNING:   Temperatura acima do trigger — ventoinha controlando.
 * FAN_AUTO_COOLDOWN:  Temperatura abaixo de TEMP_FAN_OFF — aguardando cooldown.
 */
enum FanAutoState {
  FAN_AUTO_IDLE,
  FAN_AUTO_RUNNING,
  FAN_AUTO_COOLDOWN,
};

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------
static FanMode      _mode      = FAN_MODE_AUTO;
static FanAutoState _auto_st   = FAN_AUTO_IDLE;
static int          _pwm_val   = 0;   // 0–255
static int          _speed_pct = 0;   // 0–100

// Cooldown: instante de início e duração calculada a partir de FAN_COOLDOWN_MIN
static unsigned long _cooldown_start_ms  = 0;
static const unsigned long COOLDOWN_MS   = (unsigned long)FAN_COOLDOWN_MIN * 60UL * 1000UL;

// Velocidade manual lembrada para o toggle (liga na última velocidade usada)
static int _last_manual_pct = 50;

// ---------------------------------------------------------------------------
// Calibração do potenciômetro
// ---------------------------------------------------------------------------
// Após um reset para AUTO (RTC_ON), o pot é ignorado até que o usuário:
//   1. Leve o pot ao mínimo (_pot_seen_min = true)
//   2. Aumente a partir do mínimo (_pot_calibrated = true)
// Isso evita que a posição residual do pot interfira no modo automático.
static bool _pot_seen_min    = false;
static bool _pot_calibrated  = false;
static int  _pot_adc_prev    = -1;
static int  _pot_filtered_adc = -1;
static bool _pot_min_latched  = false;

// Modo de segurança quando a temperatura fica indisponível/stale.
static bool _temp_failsafe_active = false;

// ---------------------------------------------------------------------------
// Helpers privados
// ---------------------------------------------------------------------------

/** @brief Aplica percentual (0–100) como duty PWM e atualiza variáveis de estado. */
static void _apply_speed(int pct) {
  pct        = constrain(pct, 0, 100);
  _pwm_val   = (int)((long)pct * 255L / 100L);
  _speed_pct = pct;
  ledcWrite(LEDC_CHANNEL, _pwm_val);
}

/**
 * @brief Retorna a velocidade alvo para o modo AUTO com base no delta de temperatura.
 * @param delta  temperatura_atual − TEMP_FAN_TRIGGER (sempre > 0 quando chamado)
 * @return percentual de velocidade (FAN_SPEED_LOW … FAN_SPEED_MAX)
 */
static int _auto_speed(float delta) {
  if (delta <= 1.0f) return FAN_SPEED_LOW;
  if (delta <= 2.0f) return FAN_SPEED_MED;
  if (delta <= 3.0f) return FAN_SPEED_HIGH;
  return FAN_SPEED_MAX;
}

/**
 * @brief Registra uma transição para MANUAL_OFF: desliga a ventoinha e
 *        invalida a calibração do potenciômetro, exigindo novo ciclo min→cima.
 */
static void _enter_manual_off() {
  _mode           = FAN_MODE_MANUAL_OFF;
  _pot_calibrated = false;
  _pot_seen_min   = false;
  _pot_adc_prev   = -1;
  _pot_filtered_adc = -1;
  _pot_min_latched  = false;
  _temp_failsafe_active = false;
  _apply_speed(0);
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

void fan_init() {
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ_HZ, LEDC_RESOLUTION);
  ledcAttachPin(PIN_FAN, LEDC_CHANNEL);
  _apply_speed(0);
  Serial.println("[Fan] Inicializada — desligada (modo AUTO)");
}

void fan_on_rtc_reset() {
  // Chamado pelo RTC quando o período ON da luminária começa.
  // Reseta para AUTO e exige re-calibração do pot para evitar ativação acidental.
  _mode     = FAN_MODE_AUTO;
  _auto_st  = FAN_AUTO_IDLE;
  _pot_seen_min    = false;
  _pot_calibrated  = false;
  _pot_adc_prev    = -1;
  _pot_filtered_adc = -1;
  _pot_min_latched  = false;
  _temp_failsafe_active = false;
  // Não altera _speed_pct/_pwm_val agora; fan_update() decidirá no próximo ciclo.
  Serial.println("[Fan] Modo AUTO restaurado (RTC_ON) — re-calibracao do potenciometro necessaria");
}

void fan_set_speed_web(int percent) {
  percent = constrain(percent, 0, 100);
  if (percent == 0) {
    _enter_manual_off();
    Serial.println("[Fan] Desligada via web (fan_set_speed 0)");
    return;
  }
  _last_manual_pct = percent;
  _mode = FAN_MODE_MANUAL_SPEED;
  _apply_speed(percent);
  Serial.printf("[Fan] Velocidade manual via web: %d%%\n", percent);
}

void fan_toggle_web() {
  if (_speed_pct == 0 || _mode == FAN_MODE_MANUAL_OFF) {
    // Estava desligada: liga na última velocidade manual
    _mode = FAN_MODE_MANUAL_SPEED;
    _apply_speed(_last_manual_pct);
    Serial.printf("[Fan] Ligada via web (%d%%)\n", _last_manual_pct);
  } else {
    // Estava ligada: desliga manualmente
    _last_manual_pct = _speed_pct;   // memoriza a velocidade atual para próximo toggle-on
    _enter_manual_off();
    Serial.println("[Fan] Desligada via web (MANUAL_OFF)");
  }
}

int fan_get_speed_percent() {
  return _speed_pct;
}

bool fan_is_on() {
  return _speed_pct > 0;
}

void fan_update() {
  // ── 1. Potenciômetro ─────────────────────────────────────────────────────
  int pot_raw = analogRead(PIN_POT);
  if (_pot_filtered_adc < 0) {
    _pot_filtered_adc = pot_raw;
  } else {
    // Filtro exponencial simples para reduzir ruído no ADC.
    _pot_filtered_adc = (_pot_filtered_adc * 3 + pot_raw) / 4;
  }

  int pot_adc = _pot_filtered_adc;
  if (!_pot_min_latched) {
    if (pot_adc <= POT_MIN_ADC) {
      _pot_min_latched = true;
    }
  } else if (pot_adc >= POT_MIN_EXIT_ADC) {
    _pot_min_latched = false;
  }
  bool pot_at_min = _pot_min_latched;

  if (!_pot_calibrated) {
    // Fase de calibração: aguarda o usuário ir ao mínimo e depois subir
    if (pot_at_min) {
      _pot_seen_min = true;
    } else if (_pot_seen_min) {
      // Saiu do mínimo após ter passado pelo mínimo: pot pronto para uso
      _pot_calibrated = true;
      _pot_adc_prev   = pot_adc;
      Serial.println("[Fan] Potenciometro calibrado");
    }
  } else {
    // Pot calibrado: interpreta posição
    if (pot_at_min) {
      // Usuário colocou no mínimo intencionalmente → MANUAL_OFF
      if (_mode != FAN_MODE_MANUAL_OFF) {
        _enter_manual_off();
        Serial.println("[Fan] Desligada via potenciometro (MANUAL_OFF)");
      }
    } else {
      // Verifica se houve mudança significativa (filtra ruído do ADC)
      if (_pot_adc_prev < 0 || abs(pot_adc - _pot_adc_prev) > POT_CHANGE_ADC) {
        _pot_adc_prev   = pot_adc;
        int new_pct     = (int)map(pot_adc, POT_MIN_ADC, 4095, 1, 100);
        new_pct         = constrain(new_pct, 1, 100);
        _last_manual_pct = new_pct;
        _mode            = FAN_MODE_MANUAL_SPEED;
        _apply_speed(new_pct);
      }
    }
  }

  // ── 2. Modo manual: nada mais a fazer ────────────────────────────────────
  if (_mode != FAN_MODE_AUTO) return;

  // ── 3. Modo AUTO: controle por temperatura com histérese ─────────────────
  if (!temperature_is_fresh()) {
    int failsafe_pct = constrain(FAN_FAILSAFE_SPEED, 0, 100);
    if (!_temp_failsafe_active || _speed_pct != failsafe_pct) {
      _apply_speed(failsafe_pct);
      _temp_failsafe_active = true;
      Serial.printf("[Fan] AUTO em seguranca: temperatura indisponivel/stale, mantendo %d%%\n",
                    failsafe_pct);
    }
    return;
  }

  _temp_failsafe_active = false;
  float temp = temperature_read();

  switch (_auto_st) {

    case FAN_AUTO_IDLE:
      // Aguardando temperatura subir acima do trigger
      if (temp > TEMP_FAN_TRIGGER) {
        float delta = temp - TEMP_FAN_TRIGGER;
        int   speed = _auto_speed(delta);
        _apply_speed(speed);
        _auto_st = FAN_AUTO_RUNNING;
        Serial.printf("[Fan] AUTO IDLE→RUNNING: %.1f°C (Δ%.1f°C) → %d%%\n",
                      temp, delta, speed);
      }
      break;

    case FAN_AUTO_RUNNING:
      if (temp < TEMP_FAN_OFF) {
        // Temperatura abaixo do limiar de desligamento: inicia cooldown com velocidade reduzida
        _cooldown_start_ms = millis();
        _apply_speed(FAN_SPEED_LOW);
        _auto_st = FAN_AUTO_COOLDOWN;
        Serial.printf("[Fan] AUTO RUNNING→COOLDOWN: %.1f°C — %d min, %d%%\n",
                      temp, FAN_COOLDOWN_MIN, FAN_SPEED_LOW);
      } else if (temp >= TEMP_FAN_TRIGGER) {
        // Ainda acima do trigger: ajusta velocidade conforme delta atual
        float delta    = temp - TEMP_FAN_TRIGGER;
        int   speed    = _auto_speed(delta);
        if (speed != _speed_pct) {
          _apply_speed(speed);
          Serial.printf("[Fan] AUTO velocidade ajustada: %.1f°C (Δ%.1f°C) → %d%%\n",
                        temp, delta, speed);
        }
      }
      // Banda de histérese (TEMP_FAN_OFF ≤ temp < TEMP_FAN_TRIGGER): mantém velocidade atual
      break;

    case FAN_AUTO_COOLDOWN:
      if (temp > TEMP_FAN_TRIGGER) {
        // Temperatura subiu novamente durante o cooldown: cancela e retoma controle
        float delta = temp - TEMP_FAN_TRIGGER;
        int   speed = _auto_speed(delta);
        _apply_speed(speed);
        _auto_st = FAN_AUTO_RUNNING;
        Serial.printf("[Fan] AUTO COOLDOWN→RUNNING: temperatura voltou (%.1f°C) → %d%%\n",
                      temp, speed);
      } else if (millis() - _cooldown_start_ms >= COOLDOWN_MS) {
        // Cooldown expirado: desliga
        _apply_speed(0);
        _auto_st = FAN_AUTO_IDLE;
        Serial.println("[Fan] AUTO COOLDOWN→IDLE: cooldown encerrado, desligada");
      }
      break;
  }
}
