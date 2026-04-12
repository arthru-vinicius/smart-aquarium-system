#pragma once

/**
 * @brief Inicializa o canal LEDC (PWM) para controle da ventoinha.
 *        Estado inicial: desligada. Modo inicial: AUTO.
 */
void fan_init();

/**
 * @brief Gerencia toda a lógica de controle da ventoinha.
 *        Lê o potenciômetro, monitora a temperatura (via cache do DS18B20) e
 *        executa a máquina de estados do controle automático com histérese.
 *        Se a temperatura ficar indisponível/stale no modo AUTO, aplica
 *        FAN_FAILSAFE_SPEED como fallback de segurança.
 *        Deve ser chamado no loop(), após temperature_update().
 */
void fan_update();

/**
 * @brief Reseta a ventoinha para o modo AUTO e requer re-calibração do
 *        potenciômetro. Chamado pelo RTC na transição para o período ON
 *        (RTC_ON_HOUR:RTC_ON_MIN), sincronizado com o acendimento da luminária.
 */
void fan_on_rtc_reset();

/**
 * @brief Define a velocidade manualmente via interface web.
 *        Congela o controle automático até o próximo RTC_ON.
 *        Se percent == 0, equivale a fan_toggle_web() no estado ligado.
 * @param percent velocidade desejada (0–100)
 */
void fan_set_speed_web(int percent);

/**
 * @brief Alterna o estado da ventoinha via interface web.
 *        Se desligada: liga na última velocidade manual usada (padrão: 50%).
 *        Se ligada: desliga em modo MANUAL_OFF e reseta calibração do pot.
 *        Congela o controle automático até o próximo RTC_ON.
 */
void fan_toggle_web();

/**
 * @brief Retorna a velocidade atual da ventoinha como percentual (0–100).
 */
int fan_get_speed_percent();

/**
 * @brief Retorna true se a ventoinha estiver ligada (velocidade > 0).
 */
bool fan_is_on();
