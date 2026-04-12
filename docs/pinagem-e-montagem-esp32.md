# Pinagem e Montagem — ESP32-WROOM-32D

Documento de referência para a montagem física do sistema de controle do aquário.
Cobre o **Sistema 1 (Luminária)** e o **Sistema 2 (Termômetro + Ventoinha)** em detalhe.

---

## ⚠️ Aviso de Segurança — Tensão de Rede (AC)

> O SSR40DA comuta a **tensão da rede elétrica (127V/220V AC)**.
> O lado de carga do SSR **nunca deve ser tocado** com o sistema energizado.
> Todo o cabeamento AC deve ser feito com cabo apropriado para a corrente da luminária,
> conectores isolados, e o circuito de baixa tensão (ESP32 + módulos) deve estar
> **eletricamente separado** do circuito AC por gabinete ou canaleta.

---

## Visão Geral

| Sistema | Componentes | Status |
|---|---|---|
| **1 — Luminária** | ESP32, DS3231SN, SSR40DA, Push button | ✅ Implementado |
| **2 — Temperatura/Ventoinha** | DS18B20, Ventoinha 4 pinos 12V, Potenciômetro B10K | ✅ Implementado |

Alimentação de todo o sistema de baixa tensão: **USB-C → 5V → LDO da placa dev → 3,3V**

---

## Lista de Componentes

### Sistema 1 — Luminária

| Componente | Quantidade | Observação |
|---|---|---|
| ESP32 DevKit (WROOM-32D) | 1 | Alimentado via USB-C |
| Módulo DS3231SN | 1 | Inclui pull-ups I2C e slot CR2032 |
| Bateria CR2032 | 1 | Backup do RTC — verificar se vem com o módulo |
| SSR40DA | 1 | Relé de estado sólido para controle AC |
| Push button N.O. | 1 | Botão momentâneo, normalmente aberto |
| Resistor 220Ω | 1 | Limitador de corrente do controle do SSR |
| Resistor 10kΩ | 1 | Pulldown em GPIO23 (segurança no boot) |
| Capacitor 100nF | 2 | Desacoplamento de alimentação |
| 1N4007 | — | **Não necessário** neste sistema (SSR é estado sólido, sem bobina) |

### Sistema 2 — Temperatura + Ventoinha

| Componente | Quantidade | Observação |
|---|---|---|
| DS18B20 (à prova d'água) | 1 | Sensor OneWire — conector P4 ou solda direta |
| Resistor 4,7kΩ | 0 ou 1 | Pull-up OneWire (usar **somente** se o módulo/sensor não tiver pull-up onboard) |
| Potenciômetro B10K | 1 | Controle manual de velocidade da ventoinha |
| Capacitor 100nF | 1 | Filtro no ADC do potenciômetro (GPIO34 → GND) |
| Capacitor 100nF | 1 | Desacoplamento local do DS18B20 (VCC → GND, próximo ao sensor) |
| Capacitor 10µF | 1 | Desacoplamento no conector da ventoinha 12V (12V → GND) |
| Ventoinha 4 pinos 12V PWM | 1 | Pinos: GND, 12V, Tach (opcional), PWM |
| Resistor 10kΩ | 1 | Pull-up 3,3V para o fio Tach (opcional) |
| Fonte 12V DC | 1 | Exclusiva para a ventoinha; GND comum com o ESP32 |

---

## Tabela de Pinagem Completa

### Pinos utilizados

| GPIO | Módulo/Componente | Direção | Resistor/Cap associado |
|---|---|---|---|
| GPIO18 | Push button | Entrada | Pull-up interno 45kΩ + 100nF para GND |
| GPIO21 | DS3231SN — SDA | I/O | Pull-up 4,7kΩ no próprio módulo |
| GPIO22 | DS3231SN — SCL | Saída | Pull-up 4,7kΩ no próprio módulo |
| GPIO23 | SSR40DA — controle (+) | Saída | 220Ω em série + 10kΩ para GND |
| GPIO4 | DS18B20 — DATA | I/O | Pull-up OneWire (normalmente onboard no módulo) + 100nF VCC/GND recomendado |
| GPIO25 | Ventoinha — PWM | Saída | LEDC canal 0 (25 kHz) |
| GPIO34 | Potenciômetro — wiper | Entrada (só) | 100nF para GND (filtro ADC) |
| GPIO35 | Ventoinha — Tachômetro | Entrada (só) | 10kΩ pull-up para 3,3V (opcional) |

### Pinos não usar

| GPIO | Motivo |
|---|---|
| GPIO6 – GPIO11 | Flash SPI interno — inutilizáveis |
| GPIO0 | Strapping (boot mode) |
| GPIO2 | Strapping (download mode) |
| GPIO5 | Strapping |
| GPIO12 (MTDI) | Strapping — define tensão do flash |
| GPIO15 (MTDO) | Strapping |
| GPIO1 (TX0) | UART — monitor serial / programação |
| GPIO3 (RX0) | UART — programação |

---

## Circuitos de Montagem — Sistema 1 (Luminária)

### 1 · Alimentação

```
Computador / Fonte USB
        │ USB-C
        ▼
  ┌──────────────────┐
  │  ESP32 DevKit    │   5V → regulador LDO interno → 3,3V
  │                  │
  │  3V3 ○──────────────── alimenta todos os módulos de baixa tensão
  │  GND ○──────────────── referência comum
  └──────────────────┘

Desacoplamento (colocar o mais próximo possível dos pinos de alimentação):
  3V3 ──┬── 100nF ── GND   (próximo ao ESP32)
        └── 100nF ── GND   (próximo ao DS3231SN)
```

---

### 2 · Módulo DS3231SN (RTC — I2C)

```
  ESP32 DevKit          DS3231SN Module
  ┌──────────┐          ┌──────────────┐
  │          │          │              │
  │  3V3 ────┼──────────┼─ VCC         │
  │  GND ────┼──────────┼─ GND         │
  │          │          │              │
  │ GPIO21 ──┼──────────┼─ SDA  [4,7kΩ pull-up já no módulo]
  │ (SDA)    │          │              │
  │ GPIO22 ──┼──────────┼─ SCL  [4,7kΩ pull-up já no módulo]
  │ (SCL)    │          │              │
  └──────────┘          │  [CR2032]    │  ← bateria de backup
                        └──────────────┘

Obs.: se o módulo não tiver pull-ups integrados, adicionar
      4,7kΩ de SDA → 3,3V e SCL → 3,3V.
```

---

### 3 · SSR40DA (controle da luminária AC)

```
                         ┌──────────────────────────────────────────┐
  ESP32                  │           SSR40DA                        │
  ┌──────────┐           │  ┌──────────┐   ┌──────────────────┐    │
  │          │           │  │  ENTRADA │   │  SAÍDA (AC carga)│    │
  │ GPIO23 ──┼──[220Ω]───┼──┤ (+)  (−)├───┤  1           2   │    │
  │          │     │     │  └──────────┘   └──────────────────┘    │
  │  GND  ───┼─────┴─────┼─ (−) e [10kΩ] ── GND                    │
  │          │   [10kΩ]  └──────────────────────────────────────────┘
  │          │     │
  └──────────┘    GND

  Corrente GPIO23 (HIGH): (3,3V − 1,2V) / 220Ω + 3,3V / 10kΩ ≈ 9,8 mA  ✓ (< 28 mA máx)
  Pulldown 10kΩ: mantém SSR desligado enquanto GPIO23 está flutuante (boot do ESP32)

  ═══════════════════════════════════════════════════════════
  ATENÇÃO — LADO AC (alta tensão):

  Tomada/Quadro ──── FASE ────[SSR terminal 1]
                   [SSR terminal 2]──── FASE DA LUMINÁRIA ──── Luminária LED
  Tomada/Quadro ──── NEUTRO ──────────────────────────────── Luminária LED
  ═══════════════════════════════════════════════════════════

  O SSR40DA possui zero-crossing detector interno (reduz interferência).
  Interromper SOMENTE o fio de FASE através do SSR.
```

---

### 4 · Push Button

```
  ESP32
  ┌──────────────┐
  │              │
  │ GPIO18 ──────┼──────── [BTN terminal A]
  │              │              │
  │  GND  ───────┼──────── [BTN terminal B]   ← botão N.O. (normalmente aberto)
  │       │      │
  │       └──[100nF]── GPIO18   (opcional: filtro hardware de debounce)
  │              │
  └──────────────┘

  Pullup interno ativado em firmware: pinMode(PIN_BUTTON, INPUT_PULLUP)
  Pullup interno: ~45 kΩ  (suficiente, resistor externo não necessário)
  Software debounce: 50 ms já implementado em light.cpp
```

---

## Diagrama de Blocos — Sistema 1 Completo

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                       USB-C (5V)                                │
  │                           │                                     │
  │                    ┌──────▼──────┐                              │
  │                    │  ESP32 Dev  │                              │
  │                    │  WROOM-32D  │                              │
  │                    └─────────────┘                              │
  │                    │  3V3  │ GND │                              │
  │       ┌────────────┘       └─────────────┐                      │
  │       │                                  │                      │
  │  ┌────▼────────┐   ┌──────────┐   ┌──────▼──────┐              │
  │  │  DS3231SN   │   │ SSR40DA  │   │ Push Button  │             │
  │  │   (I2C)     │   │ (GPIO23) │   │  (GPIO18)    │             │
  │  │ SDA=GPIO21  │   │  220Ω    │   │  INPUT_PULLUP│             │
  │  │ SCL=GPIO22  │   │  10kΩ↓  │   │  100nF opt.  │             │
  │  └─────────────┘   └────┬─────┘   └──────────────┘             │
  │                         │ AC                                    │
  │                    ┌────▼──────────────────┐                   │
  │                    │   LUMINÁRIA LED (AC)   │                   │
  │                    └───────────────────────┘                   │
  └─────────────────────────────────────────────────────────────────┘
```

---

## Circuitos de Montagem — Sistema 2 (Termômetro + Ventoinha)

### 5 · DS18B20 (sensor de temperatura — OneWire)

```
  ESP32 DevKit          DS18B20 (3 fios)
  ┌──────────┐          ┌─────────────────┐
  │          │          │                 │
  │ GPIO4 ───┼──────────┼─ DATA (amarelo) │  ← pull-up normalmente já no módulo
  │          │          │                 │
  │  3V3 ────┼──────────┼─ VCC  (vermelho)│  ← modo alimentação normal (não parasita)
  │   │      │          │                 │
  │ [100nF]  │          │                 │  ← capacitor de desacoplamento VCC↔GND
  │   │      │          │                 │
  │  GND ────┼──────────┼─ GND  (preto)   │
  └──────────┘          └─────────────────┘

  Obs.: cores padrão do DS18B20 à prova d'água. Conferir datasheet do cabo.
  Se você estiver usando módulo DS18B20 com pull-up onboard, não adicione outro resistor.
  Se for sensor "cru" (sem módulo), adicionar 4,7kΩ entre DATA e 3,3V é obrigatório.
  Recomenda-se 100nF entre VCC e GND próximo ao sensor para reduzir ruído.

  GPIO4 pertence ao ADC2, mas é usado como GPIO digital (OneWire) — sem conflito com WiFi.
```

---

### 6 · Potenciômetro B10K (controle manual de velocidade)

```
  ESP32
  ┌──────────┐
  │          │          Potenciômetro B10K
  │  3V3 ────┼──────────── pino 1 (extremo)
  │          │          │
  │ GPIO34 ──┼──────────── pino 2 (cursor / wiper)
  │       │  │          │
  │       └──[100nF]──GND  ← filtro de ruído no ADC
  │          │
  │  GND ────┼──────────── pino 3 (extremo)
  └──────────┘

  GPIO34 é input-only e pertence ao ADC1 — seguro com WiFi ativo.
  Tensão no cursor: 0 a 3,3V (mapeada para velocidade 0–100%).
  Posição no mínimo (cursor próximo ao GND) = ventoinha desligada.

  Comportamento de calibração pós-reset (RTC_ON):
    ① Sistema ignora posição atual do pot
    ② Usuário gira ao mínimo  → pot reconhecido como "viu o zero"
    ③ Usuário aumenta         → pot entra em modo ativo, controla velocidade
    ④ Usuário volta ao mínimo → MANUAL_OFF (persiste até próximo RTC_ON)
```

---

### 7 · Ventoinha 4 pinos 12V PWM

```
  ┌────────────────────────────────────────────────────────────────┐
  │  Conector da ventoinha (padrão Intel 4 pinos)                  │
  │                                                                │
  │  Pino 1 — GND ────────────────────────────── GND comum        │
  │  Pino 2 — 12V ────────────────────────────── Fonte 12V DC     │
  │  Pino 3 — Tach ──[10kΩ]── 3,3V   GPIO35 (opcional)           │
  │  Pino 4 — PWM ────────────────────────────── GPIO25 (ESP32)   │
  └────────────────────────────────────────────────────────────────┘

  ESP32                         Fonte 12V DC
  ┌──────────┐                  ┌──────────┐
  │          │                  │          │
  │ GPIO25 ──┼──────────────────┼─ PWM     │  Ventoinha
  │          │                  │          │
  │  GND  ───┼──────────────────┼─ GND ────┼─ Pino 1 ─┐
  └──────────┘                  │  12V ────┼─ Pino 2  │
                                └──────────┘          │
                  GPIO35 ──[10kΩ pull-up 3,3V]── Pino 3 (Tach, opcional)

  IMPORTANTE: O GND da fonte 12V deve ser ligado ao GND do ESP32 (referência comum).
  A fonte 12V NÃO compartilha o +12V com o ESP32 — apenas o GND.

  Recomendação para máxima compatibilidade: usar driver open-drain no PWM
  (ex.: NPN 2N3904/BC337 com resistor de base 1kΩ), pois muitos fans 4 pinos
  seguem a especificação Intel com linha PWM em coletor aberto.
  Ligação direta 3,3V costuma funcionar em vários modelos, mas é menos robusta.

  Adicionar 10µF + 100nF entre 12V e GND próximo ao conector da ventoinha
  reduz ruído e melhora estabilidade em transientes de partida.

  GPIO25 pertence ao ADC2, mas é usado como saída digital PWM (LEDC) — sem conflito com WiFi.
```

---

### 8 · Tachômetro (pino 3 da ventoinha — opcional)

```
  Ventoinha                ESP32
  ┌─────────┐              ┌──────────┐
  │         │              │          │
  │  Tach ──┼──────────────┼── GPIO35 │
  │         │   [10kΩ]     │          │
  │         │     │        └──────────┘
  │         │    3,3V
  └─────────┘

  GPIO35 é input-only: adequado para leitura de pulsos do tachômetro.
  O sinal Tach é open-drain; o pull-up 10kΩ para 3,3V é obrigatório.
  Firmware atual: pino reservado, leitura de RPM não implementada nesta versão.
```

---

## Diagrama de Blocos — Sistema Completo (1 + 2)

```
  ┌───────────────────────────────────────────────────────────────────┐
  │                    USB-C (5V)        Fonte 12V DC                 │
  │                        │                  │                       │
  │                 ┌──────▼──────┐           │                       │
  │                 │  ESP32 Dev  │           │                       │
  │                 │  WROOM-32D  │           │                       │
  │                 └─────────────┘           │                       │
  │            3V3 ─┤             ├─ GND ─────┤                       │
  │                 │             │           │                       │
  │  ┌──────────────┼─────────────┼──────┐   │                       │
  │  │              │             │      │   │                       │
  │  │  DS3231SN    │  SSR40DA    │  DS18B20  │                       │
  │  │  (I2C)       │  (GPIO23)   │  (GPIO4)  │                       │
  │  │  SDA=21      │  220Ω       │  4,7kΩ↑  │                       │
  │  │  SCL=22      │  10kΩ↓     │           │                       │
  │  └──────────────┘  ──┬──      └───────────┘                       │
  │                      │ AC                                         │
  │               ┌──────▼──────────────┐                            │
  │               │   LUMINÁRIA (AC)    │                            │
  │               └─────────────────────┘                            │
  │                                                                   │
  │  Push Button   Potenciômetro B10K    Ventoinha 4 pinos 12V        │
  │  (GPIO18)      (GPIO34, ADC1)        (GPIO25 PWM, GPIO35 Tach)    │
  │  INPUT_PULLUP  100nF filtro          GND comum + 12V exclusivo    │
  └───────────────────────────────────────────────────────────────────┘
```

---

## Lógica de Controle da Ventoinha

### Modo AUTO (padrão — ativo a cada RTC_ON)

A ventoinha responde automaticamente à temperatura com **histérese** para evitar
liga/desliga rápido quando a temperatura oscila em torno do limiar:

```
Constantes configuráveis em config.h:
  TEMP_FAN_TRIGGER  28,0°C  → temperatura para ligar a ventoinha
  TEMP_FAN_OFF      26,5°C  → temperatura para iniciar cooldown (1,5°C de histérese)
  FAN_COOLDOWN_MIN  30 min  → funcionamento após atingir TEMP_FAN_OFF

Velocidade proporcional ao delta (Δ = temperatura − TEMP_FAN_TRIGGER):
  Δ ≤ 1,0°C  →  FAN_SPEED_LOW  (30%) — levemente acima do limiar
  Δ ≤ 2,0°C  →  FAN_SPEED_MED  (55%) — aquecimento moderado
  Δ ≤ 3,0°C  →  FAN_SPEED_HIGH (80%) — aquecimento significativo
  Δ > 3,0°C  →  FAN_SPEED_MAX (100%) — situação crítica

Fluxo de estados AUTO:
  IDLE ──(temp > TRIGGER)──► RUNNING ──(temp < OFF)──► COOLDOWN ──(timer)──► IDLE
                                 ▲                         │
                                 └────(temp > TRIGGER)─────┘  (cancela cooldown)
```

### Overrides manuais

| Fonte | Ação | Efeito | Duração |
|---|---|---|---|
| Potenciômetro calibrado | Aumentar acima do mínimo | MANUAL_SPEED na posição do pot | Até RTC_ON |
| Potenciômetro calibrado | Colocar no mínimo | MANUAL_OFF | Até RTC_ON |
| Interface web (toggle) | Desligar | MANUAL_OFF | Até RTC_ON |
| Interface web (toggle) | Ligar | MANUAL_SPEED na última velocidade | Até RTC_ON |
| Interface web (slider) | Ajustar velocidade | MANUAL_SPEED no valor definido | Até RTC_ON |

### Calibração do potenciômetro após RTC_ON

Após qualquer reset para modo AUTO (RTC_ON), o potenciômetro é **ignorado** até que
o usuário execute a sequência de calibração:

```
① Girar ao mínimo  (ADC < 80)  → sistema reconhece o zero
② Aumentar a partir do mínimo  → potenciômetro ativo, controla velocidade
③ Girar ao mínimo novamente    → MANUAL_OFF (persiste até próximo RTC_ON)
```

Isso evita que a posição residual do pot interfira no modo automático após um reset.

---

## Notas Técnicas

### Corrente máxima dos GPIOs
Trate os GPIOs do ESP32 como **sinais lógicos**, não como fonte de potência:
- projeto conservador: manter corrente por pino em faixa baixa (ideal <= 8-12 mA)
- não alimentar cargas diretamente por GPIO (usar transistor/MOSFET/driver quando necessário)
- somatório de corrente depende de limites internos e térmicos; evitar dimensionar pelo "máximo teórico"

### ADC e WiFi
O ESP32 possui dois blocos de ADC:
- **ADC1** (GPIO32–39): **seguro com WiFi ativo** → usar para leituras analógicas
- **ADC2** (GPIO0, 2, 4, 12–15, 25–27): bloqueado pelo WiFi quando rádio está ligado → **não usar como ADC enquanto WiFi estiver ativo**

GPIO4 (DS18B20) e GPIO25 (Fan PWM) pertencem ao ADC2, mas são usados como **GPIO digital** (OneWire e PWM), portanto não há conflito.
GPIO34 (Potenciômetro) é ADC1 — seguro com WiFi ativo.

### Pull-ups I2C
O protocolo I2C exige resistores de pull-up em SDA e SCL.
O módulo DS3231SN normalmente os inclui (4,7kΩ). Verificar antes de montar.
Se necessário, adicionar externamente: **4,7kΩ de cada linha para 3,3V**.

### OneWire e DS18B20
O barramento OneWire exige pull-up na linha DATA:
- se o módulo DS18B20 já tiver pull-up onboard, não duplicar resistor
- se for sensor sem módulo, usar **4,7kΩ entre DATA e 3,3V**
- manter modo de alimentação normal (3 fios), não parasita, para maior confiabilidade
- adicionar 100nF entre VCC/GND próximo ao sensor melhora imunidade a ruído

A conversão de temperatura a 12 bits demora ~750 ms. O firmware usa conversão
não-bloqueante (`setWaitForConversion(false)`), portanto o loop não trava.

### PWM da ventoinha
Frequência: 25 kHz (acima da faixa audível — sem chiado).
Para robustez elétrica, preferir estágio open-drain/coletor-aberto no sinal PWM.
Ligação direta 3,3V pode funcionar, mas depende do fan e tende a ser menos previsível.
Verificar no datasheet da ventoinha os níveis lógicos aceitos.

### Strapping pins no boot
O ESP32 amostra GPIO0, GPIO2, GPIO5, GPIO12 e GPIO15 durante a inicialização para definir modo de boot e tensão do flash. Manter esses pinos sem cargas que alterem seu estado padrão.

### Pinos do flash interno
**GPIO6 a GPIO11 estão conectados ao flash SPI interno do módulo e nunca devem ser usados.**
