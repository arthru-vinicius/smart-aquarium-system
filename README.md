# Smart Aquarium System

Controle inteligente de aquário com ESP32, interface web/PWA e backend proxy em PHP + MQTT TLS (porta 8883).

O projeto foi evoluído para operação contínua e segura:
- automação por RTC (DS3231)
- controle de temperatura e ventoinha com histerese/failsafe
- ventoinha 12V 2 fios via MOSFET com boost de partida
- OTA protegido por credenciais
- API com token
- thresholds/horários ajustáveis pela interface (persistência em NVS)
- frontend instalável no Android (PWA)
- recuperação de Wi-Fi com Access Point de configuração

## Estado atual do projeto

### Firmware (`firmware/aquarium`)
- Arquitetura modular em C++ (Arduino/ESP32).
- Loop principal não bloqueante (funcionalidades locais continuam mesmo sem rede).
- Módulos:
  - `light.*`: luminária com botão físico e automação por horário
  - `temperature.*`: DS18B20 com leitura não bloqueante
  - `fan.*`: modos AUTO/MANUAL, histerese/cooldown e partida assistida para fan 2 fios
  - `rtc_manager.*`: DS3231 + sincronização NTP (com retentativas)
  - `wifi_manager.*`: reconexão automática + AP de recuperação + persistência de credenciais em NVS
  - `web_server.*`: API REST local + OTA + portal `/wifi-setup`

### Pinagem efetiva
- `GPIO18`: push button
- `GPIO21/22`: I2C do DS3231
- `GPIO23`: SSR da luminária
- `GPIO19`: DS18B20
- `GPIO34`: potenciômetro (ADC1)
- `GPIO17`: gate do MOSFET da ventoinha 2 fios

### Servidor (`server`)
- Endpoint oculto via rewrite: `cache-<secret>.js`.
- Proxy entre navegador e broker MQTT (HiveMQ), sem expor IP local do ESP32 no frontend.
- Frontend em `app.html` (SPA leve).
- PWA dinâmico:
  - `?manifest=1`
  - `?sw=1`
- Estratégia offline: shell cacheado; chamadas de API seguem network-first.

## Estrutura do repositório

```text
smart-aquarium-system/
├─ firmware/
│  └─ aquarium/
│     ├─ aquarium.ino
│     ├─ config.example.h
│     ├─ wifi_manager.*
│     ├─ web_server.*
│     ├─ fan.*
│     ├─ temperature.*
│     ├─ light.*
│     └─ rtc_manager.*
├─ server/
│  ├─ .htaccess
│  ├─ index.php
│  ├─ app.html
│  ├─ config.example.php
│  └─ assets/
└─ docs/
   ├─ clock-module-schema.avif
   ├─ pinagem-e-montagem-esp32.md
   └─ deploy-servidor.md
```

## Fluxo de funcionamento

1. O usuário acessa `https://.../cache-<secret>.js`.
2. O `.htaccess` reescreve para `server/index.php?key=<secret>`.
3. `index.php` valida o secret.
4. Sem `?api=`, entrega `app.html`.
5. Com `?api=...`, publica/lê via MQTT no broker e retorna JSON.
6. Se `?manifest=1` ou `?sw=1`, entrega recursos PWA dinâmicos.

## Endpoints

### No servidor (`server/index.php`)
- `GET /cache-<secret>.js` -> app HTML
- `GET /cache-<secret>.js?api=status`
- `GET /cache-<secret>.js?api=toggle`
- `GET /cache-<secret>.js?api=temperature`
- `GET /cache-<secret>.js?api=fan_toggle`
- `GET /cache-<secret>.js?api=fan_speed&value=0..100`
- `GET /cache-<secret>.js?api=rtc_sync`
- `GET /cache-<secret>.js?api=fan_config&trigger=<float>&off=<float>`
- `GET /cache-<secret>.js?api=light_schedule&on=HH:MM&off=HH:MM`
- `GET /cache-<secret>.js?manifest=1`
- `GET /cache-<secret>.js?sw=1`

### No ESP32 (`web_server.cpp`)
- `GET /status`
- `GET /toggle`
- `GET /temperature`
- `GET /fan_toggle`
- `GET /fan_speed?value=0..100`
- `GET /update` (OTA)
- `GET /wifi-setup` (portal de recuperação de Wi-Fi, Basic Auth)
- `POST /wifi-setup/save` (salva SSID/senha em NVS e reconecta)

## Segurança atual

- URL protegida por secret (`cache-<secret>.js`).
- API do ESP32 protegida por token (`API_AUTH_TOKEN` / `X-Api-Token`).
- OTA protegido por `OTA_USERNAME`/`OTA_PASSWORD`.
- Portal Wi-Fi (`/wifi-setup`) reutiliza as credenciais OTA.
- Bloqueio 403 para rotas não permitidas no `.htaccess`.
- CORS restrito por origem configurável (`CORS_ALLOWED_ORIGIN`).

## Recuperação de Wi-Fi

Se o ESP32 não encontrar/conectar na rede por tempo/tentativas configurados:
- ativa AP de recuperação (`WIFI_RECOVERY_AP_SSID_PREFIX-XXXXXX`)
- mantém tentativa de STA em paralelo (`WIFI_AP_STA`)
- expõe página de configuração em `/wifi-setup`
- persiste novas credenciais em NVS
- desativa AP automaticamente quando reconecta

Parâmetros relevantes (`config.h`):
- `WIFI_RECONNECT_INTERVAL_MS`
- `WIFI_RECOVERY_TIMEOUT_MS`
- `WIFI_RECOVERY_MAX_ATTEMPTS`
- `WIFI_RECOVERY_AP_SSID_PREFIX`
- `FAN_MIN_RUNNING_PCT`
- `FAN_STARTUP_BOOST_PCT`
- `FAN_STARTUP_BOOST_MS`

## PWA (Android)

- `app.html` inclui manifest e registro do service worker.
- Instalação via fluxo nativo do Chrome no Android.
- `start_url`/`id` usam a URL protegida atual (`cache-<secret>.js`).
- Em offline, o shell abre; ações de API dependem de rede local com ESP32.

## Configuração

## Dependências de Build (Firmware)

- Core ESP32 (Arduino): `3.3.7` (versão usada nos logs de compilação do projeto)
- `Adafruit BusIO`: `1.17.4`
- `ArduinoJson`: `7.4.3`
- `Async TCP`: `3.4.10`
- `DallasTemperature`: `4.0.6`
- `ElegantOTA`: `3.1.7`
- `ESP Async WebServer`: `3.10.3`
- `OneWire`: `2.3.8`
- `PubSubClient`: `2.8`
- `RTClib`: `2.1.4` (depende de `Adafruit BusIO`)

Como verificar a versão do core ESP32 no seu ambiente:

1. Arduino IDE -> `Tools` -> `Board` -> `Boards Manager` -> procure `esp32` (Espressif Systems) e veja a versão instalada.
2. Pelo log de compilação, procure um caminho como:
   - `.../packages/esp32/hardware/esp32/<VERSAO>/...`
3. Exemplo real já visto neste projeto:
   - `.../packages/esp32/hardware/esp32/3.3.7/...`

### Firmware
1. Copie `firmware/aquarium/config.example.h` para `firmware/aquarium/config.h`.
2. Preencha Wi-Fi, horários, token da API e credenciais OTA.
3. Faça upload do firmware no ESP32.

### Servidor
1. Copie `server/config.example.php` para `server/config.php`.
2. Configure:
   - `secret`
   - `mqtt_host`
   - `mqtt_port` (TLS MQTT: `8883`)
   - `mqtt_user`
   - `mqtt_password`
3. Garanta `mod_rewrite` e leitura do `.htaccess`.
4. Faça deploy em HTTPS (necessário para PWA instalável no Android).

## Atualização OTA (passo a passo)

1. Conecte o ESP32 na mesma rede do seu computador/celular.
2. Descubra o IP do ESP32:
   - pelo Serial Monitor (linha `Conectado! IP: ...`)
   - ou no roteador (lista de clientes DHCP)
3. Abra no navegador: `http://<IP_DO_ESP32>/update`
4. Faça login com `OTA_USERNAME` e `OTA_PASSWORD` do `config.h`.
5. Compile o firmware e gere o `.bin` (Arduino IDE/PlatformIO).
6. No painel OTA, selecione o arquivo `.bin` e envie.
7. Aguarde o progresso até 100%; o ESP32 reinicia automaticamente.
8. Pós-update:
   - verifique no Serial se o boot ocorreu sem erro
   - confirme `/status` respondendo
   - valide hora do RTC e comandos (luz/fan) normalmente.

## CORS: como preencher `CORS_ALLOWED_ORIGIN`

Use apenas a origem (`scheme + host + porta`), sem caminho e sem barra final.

Exemplos:
- correto: `https://dominio.com.br`
- incorreto: `https://dominio.com.br/`
- incorreto: `https://dominio.com.br/wp-content/uploads/.cache-api`

Se você acessar o app por outro host (ex.: `https://www.dominio.com.br`), a origem muda e precisa bater exatamente com o valor configurado.

## Hardware e montagem

A documentação elétrica e de pinagem está em:
- `docs/pinagem-e-montagem-esp32.md`
- `docs/clock-module-schema.avif` (diagrama visual auxiliar do módulo de relógio)

Pontos importantes:
- SSR controla AC: manter isolamento físico e boas práticas de segurança.
- Ventoinha 12V 2 fios (Molex) via MOSFET IRLB8721 em low-side.
- Diodo 1N4007 em paralelo com a fan para flyback.
- GND da fonte 12V em comum com o GND do ESP32.
- Recomendado desacoplamento local (100nF/10uF) para estabilidade.

## Notas de operação

- Ausência de Wi-Fi não deve interromper controle local de luminária/ventoinha.
- NTP falho não bloqueia boot: sistema retenta sincronização depois.
- APIs de ventilação validam faixa e formato de entrada.

## Roadmap sugerido

- Telemetria de saúde (RSSI, uptime, reset reason) no `/status`
- Captive portal completo (DNS) no modo AP de recuperação
- Suporte a múltiplas origens CORS (allowlist)
- Testes automatizados de contrato da API proxy
