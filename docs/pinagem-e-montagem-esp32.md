# Pinagem e Montagem — ESP32-WROOM-32D

Documento de referência da montagem física do sistema na versão atual:

- `DS18B20` no `GPIO19`
- ventoinha `12V 2 fios (Molex)` controlada por `MOSFET IRLB8721` no `GPIO17`
- sem tacômetro (RPM não medido nesta versão)

---

## Aviso de segurança (AC)

O `SSR40DA` comuta tensão de rede (`127V/220V AC`).

- Não tocar no lado AC energizado.
- Usar isolamento físico entre baixa tensão (ESP32/sensores) e rede AC.
- Interromper apenas a fase da luminária no SSR.

---

## Pinagem final (firmware atual)

| GPIO | Função | Observação |
|---|---|---|
| 18 | Push button | `INPUT_PULLUP`, botão para GND |
| 21 | SDA (DS3231) | I2C |
| 22 | SCL (DS3231) | I2C |
| 23 | SSR40DA entrada + | Em série com resistor `220R` |
| 19 | DS18B20 DATA | OneWire |
| 34 | Potenciômetro (wiper) | ADC1, seguro com Wi-Fi |
| 17 | Gate do IRLB8721 | PWM LEDC para fan 2 fios |

Sem uso nesta versão:

- `GPIO35` (não há tacômetro)

---

## Componentes do sistema

### Sistema 1 — Luminária

- ESP32 DevKit (WROOM-32D)
- DS3231SN
- CR2032
- SSR40DA
- Push button N.O.
- resistor `220R` (GPIO23 -> SSR+)
- resistor `10k` pulldown (GPIO23 -> GND)
- capacitores `100nF` para desacoplamento

### Sistema 2 — Temperatura + Fan

- DS18B20 (3 fios)
- potenciômetro B10K
- ventoinha 12V 2 fios (Molex)
- MOSFET `IRLB8721` (chaveamento low-side)
- diodo `1N4007` (flyback da fan)
- resistor de gate `100R` a `220R` (recomendado `220R`)
- resistor pulldown de gate `10k`
- capacitor `100nF` no ADC do potenciômetro (filtro)
- capacitor `100nF` próximo ao DS18B20 (desacoplamento)
- capacitor `10uF` no conector da fan (recomendado, opcional)
- fonte 12V para fan (GND comum com ESP32)

---

## Ligações

### 1) Alimentação e desacoplamento

Base:

- `3V3` do ESP32 alimenta DS3231, DS18B20 e potenciômetro
- `GND` comum para todos os módulos de baixa tensão

Desacoplamento com `100nF`:

- um capacitor entre `3V3` e `GND` próximo ao ESP32
- um capacitor entre `3V3` e `GND` próximo ao DS3231

Observação:

- capacitor de `100nF` cerâmico (ex.: `104`) não tem polaridade

### 2) Push button (GPIO18)

Ligações:

- `GPIO18` -> terminal A do botão
- terminal B do botão -> `GND`
- capacitor `100nF` entre `GPIO18` e `GND` (opcional, debounce por hardware)

Diagrama:

```text
GPIO18 ----+---- botão ---- GND
           |
         100nF
           |
          GND
```

### 3) DS3231 (RTC)

Ligações:

- `GPIO21` -> `SDA`
- `GPIO22` -> `SCL`
- `3V3` -> `VCC`
- `GND` -> `GND`

Observação:

- módulos DS3231 normalmente já têm pull-up I2C onboard

### 4) SSR40DA (luminária)

Lado de controle (baixa tensão):

- `GPIO23` -> resistor `220R` -> `SSR +`
- `GND ESP32` -> `SSR -`
- resistor `10k` entre `GPIO23` e `GND` (pulldown)

Diagrama:

```text
GPIO23 ----+---- 220R ---- SSR +
           |
          10k
           |
GND -------+-------------- SSR -
```

Lado AC:

- fase da rede -> terminal 1 do SSR
- terminal 2 do SSR -> fase da luminária
- neutro da rede -> neutro da luminária

### 5) DS18B20 (GPIO19)

Ligações:

- `GPIO19` -> `DATA`
- `3V3` -> `VCC`
- `GND` -> `GND`
- capacitor `100nF` entre `VCC` e `GND` próximo ao sensor

Se o sensor não tiver pull-up embutido:

- adicionar `4,7k` entre `DATA` e `3V3`

### 6) Potenciômetro B10K (GPIO34)

Ligações:

- lateral 1 -> `3V3`
- lateral 2 -> `GND`
- pino central (wiper) -> `GPIO34`
- capacitor `100nF` entre `GPIO34` (wiper) e `GND`

Diagrama:

```text
3V3 ----[lateral]   B10K   [lateral]---- GND
                 \         /
                  \ wiper /
                   GPIO34
                     |
                   100nF
                     |
                    GND
```

Sentido de rotação:

- para aumentar no sentido horário: deixar `3V3` no lado direito (olhando o eixo de frente)
- se inverter, troque apenas os dois pinos laterais

### 7) Ventoinha 12V 2 fios via IRLB8721 (GPIO17)

Topologia: chaveamento low-side.

Ligações:

- `+12V fonte` -> positivo da fan
- negativo da fan -> `DRAIN` do IRLB8721
- `SOURCE` do IRLB8721 -> `GND` da fonte 12V
- `GPIO17` -> resistor `220R` -> `GATE` do IRLB8721
- `GATE` -> resistor `10k` -> `GND`
- `GND` da fonte 12V conectado ao `GND` do ESP32 (referência comum)

Flyback (`1N4007`) em paralelo com a fan:

- catodo (lado com faixa) -> `+12V` da fan
- anodo -> lado negativo da fan (`DRAIN`)

Desacoplamento da linha 12V:

- recomendado: `10uF` + `100nF` entre `+12V` e `GND` próximo ao conector da fan
- se você só tiver `100nF`, pode montar agora com ele e adicionar `10uF` depois

Diagrama:

```text
                +12V fonte
                   |
                   +-------------------+----- (+) FAN
                   |                   |
                 [100nF]             |>| 1N4007
                   |                   |
GND fonte ----------+--------+---------+----- (-) FAN ---- DRAIN (IRLB8721)
                             |
                           SOURCE
                             |
                           GND comum ---- GND ESP32

GPIO17 ---- 220R ---- GATE
                    |
                   10k
                    |
                   GND
```

---

## Notas de firmware (fan 2 fios)

A API permanece:

- `GET /fan_toggle`
- `GET /fan_speed?value=0..100`

O firmware usa:

- duty mínimo quando `value > 0` (`FAN_MIN_RUNNING_PCT`)
- boost curto de partida (`FAN_STARTUP_BOOST_PCT` / `FAN_STARTUP_BOOST_MS`)

Isso melhora a partida da ventoinha em valores baixos (`1..20`).

---

## Notas técnicas importantes

### ADC com Wi-Fi

- usar `ADC1` para sinais analógicos com Wi-Fi ativo (`GPIO32..39`)
- por isso o potenciômetro fica no `GPIO34`

### DS18B20 no GPIO19

- `GPIO19` é I/O digital válido para OneWire
- não há dependência de ADC nesse caso

### Pinos a evitar

- `GPIO6..GPIO11` (flash interno)
- pinos de strapping para cargas externas durante boot: `0`, `2`, `5`, `12`, `15`

---

## Checklist rápido de bancada

1. Testar boot com fan desligada.
2. Confirmar leitura do DS18B20 no serial (`GPIO19`).
3. Confirmar RTC no I2C (`GPIO21/22`).
4. Confirmar botão no `GPIO18`.
5. Confirmar comando `fan_speed=0` desliga fan.
6. Confirmar `fan_speed=10` parte (boost + duty mínimo).
7. Só depois energizar lado AC da luminária.
