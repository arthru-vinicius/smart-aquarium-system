# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Padrão de código

- **Código em inglês**: nomes de arquivos, variáveis, funções, classes, constantes, chaves JSON e endpoints
- **Documentação e comentários em português**: docblocks (Doxygen/PHPDoc/JSDoc), comentários inline e mensagens de log Serial
- **C/C++**: Doxygen (`@brief`, `@param`, `@return`)
- **PHP**: PHPDoc (`@file`, `@brief`, `@param`, `@return`)
- **JavaScript**: JSDoc (`@param`, `@returns`)

## Compilação e Upload (Firmware)

Este projeto usa o **Arduino IDE** para compilar e gravar o firmware no ESP32.

1. Instale o pacote da placa ESP32 via Gerenciador de Placas (Espressif Systems)
2. Instale as bibliotecas necessárias via Gerenciador de Bibliotecas:
   - `ESPAsyncWebServer` (by lacamera ou me-no-dev)
   - `ArduinoJson` (v6 ou v7, by Benoit Blanchon)
   - `OneWire` e `DallasTemperature` (para o sensor DS18B20)
   - `RTClib` (para o módulo DS3231SN)
   - `ElegantOTA` (by ayushsharma82) — OTA de firmware
   - `WiFi.h` — incluída no pacote ESP32
3. **Abra a pasta `firmware/aquarium/`** no Arduino IDE (o IDE exige que o `.ino` esteja em pasta com o mesmo nome)
4. Copie `firmware/aquarium/config.example.h` para `firmware/aquarium/config.h` e preencha com as credenciais reais
5. Selecione a placa: **ESP32-WROOM-DA Module** (ou ESP32 Dev Module)
6. Selecione a porta COM correta e faça o upload via **Sketch → Carregar**
7. Monitore a saída serial em **115200 baud**

## Servidor PHP (Deploy)

O servidor fica em `server/` e é hospedado dentro de um WordPress existente.

1. Copie `server/config.example.php` para `server/config.php` e defina `secret` + credenciais MQTT
2. Suba os arquivos de `server/` para `/wp-content/uploads/.cache-api/` no servidor via FTP/SSH
3. A interface fica acessível em: `https://boasementestore.com.br/wp-content/uploads/.cache-api/cache-<SECRET>.js`
4. Para testar localmente: `php -S localhost:8080 -t server/` e acessar `http://localhost:8080/index.php?key=<SECRET>`

## Hardware

| Componente | Modelo | Pino ESP32 |
|---|---|---|
| Placa de desenvolvimento | ESP32-WROOM-32D | — |
| Relé SSR (luminária) | SSR40DA | GPIO 23 |
| Termômetro | DS18B20 (à prova d'água) | GPIO 19 (definido em `config.h`) |
| Módulo RTC | DS3231SN | I2C (SDA=21, SCL=22) |
| Push button (luminária) | — | GPIO 18 (definido em `config.h`) |
| Potenciômetro (ventoinha) | B10K | GPIO 34 ADC (definido em `config.h`) |
| PWM control da fan (pino 4) | — | GPIO 17 (definido em `config.h`) |
| Tacômetro da fan (pino 3) | — | GPIO 25 (definido em `config.h`) |

Datasheet do ESP32-WROOM-32D: `docs/esp32-wroom-32d_datasheet_en.pdf`

## Arquitetura

### Estrutura de diretórios

```
firmware/aquarium/          ← Pasta do sketch Arduino (nome = nome do .ino)
  aquarium.ino              ← Entry point: apenas setup() e loop()
  config.h                  ← GITIGNORED — credenciais e pinos reais
  config.example.h          ← Template versionado
  wifi_manager.{h,cpp}      ← Conexão e reconexão Wi-Fi
  light.{h,cpp}             ← Relé SSR40DA + push button + debounce
  temperature.{h,cpp}       ← Leitura DS18B20 (DallasTemperature)
  fan.{h,cpp}               ← ADC potenciômetro + PWM LEDC direto na fan 4 pinos + tacômetro
  rtc_manager.{h,cpp}       ← DS3231SN via RTClib + NTP sync + automação por horário
  web_server.{h,cpp}        ← ESPAsyncWebServer + ArduinoJson + ElegantOTA

server/                     ← Deploy no WordPress via FTP/SSH
  index.php                 ← Proxy PHP: serve app.html e integra com MQTT
  app.html                  ← Interface web (SPA)
  .htaccess                 ← Rewrite: cache-<SECRET>.js → index.php?key=SECRET
  config.php                ← GITIGNORED — secret e credenciais MQTT
  config.example.php        ← Template versionado
```

### Fluxo de comunicação

```
Browser → https://boasementestore.com.br/.../cache-<SECRET>.js
              ↓ .htaccess rewrite
          index.php?key=<SECRET>
              ↓ sem ?api         → serve app.html
              ↓ com ?api=status  → lê aquarium/status no broker MQTT
              ↓ com ?api=toggle  → publica comando em aquarium/cmd/light
```

O browser nunca acessa o ESP32 diretamente — toda comunicação passa pelo proxy PHP + MQTT.

### Endpoints do ESP32

| Endpoint | Ação |
|---|---|
| `GET /status` | Retorna JSON completo do sistema sem alterar estado |
| `GET /toggle` | Alterna luminária (a automação por horário permanece ativa) |
| `GET /temperature` | Retorna apenas leitura de temperatura |
| `GET /fan_toggle` | Alterna ventoinha (override manual até próximo RTC_ON) |
| `GET /fan_speed?value=0..100` | Ajusta velocidade manual da ventoinha |
| `GET /update` | Interface ElegantOTA para upload de firmware (auth: config.h) |

### Comportamento da automação

A automação por horário é **sempre ativa** — não há botão para desligá-la.

- A luminária liga/desliga nos horários `RTC_ON_*` / `RTC_OFF_*` de `config.h`
- O botão físico e o botão web apenas **sobrepõem** o estado dentro do período atual
- Na próxima transição de horário, a automação volta a agir normalmente
- Exemplo: se desligada manualmente às 12h (período ligado 10h–17h), volta a ligar às 10h do dia seguinte

### Formato JSON de resposta

```json
{
  "light":       { "on": true },
  "temperature": { "celsius": 26.5, "available": true },
  "fan":         { "on": false, "speed_percent": 0, "rpm": 0 },
  "rtc":         { "time": "14:32", "available": true,
                   "on_time": "10:00", "off_time": "17:00" }
}
```

## Objetivo de Evolução do Sistema

- ✅ Controle da luminária via web e botão físico (SSR40DA)
- ✅ Automação da luminária por horário via DS3231SN + sincronização NTP
- ✅ OTA de firmware via ElegantOTA
- ✅ Leitura de temperatura via DS18B20
- ✅ Controle da ventoinha 4 pinos (CPU fan) via PWM direto (on/off e velocidade via potenciômetro/web)
- ✅ Tacômetro da ventoinha — RPM medido via GPIO25 e exibido na interface
- ✅ Interface web exibindo temperatura, ventoinha e RPM
