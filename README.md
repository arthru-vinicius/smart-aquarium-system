# Smart Aquarium System

Controle inteligente de aquário com ESP32, interface web/PWA e backend proxy em PHP.

O projeto foi evoluído para operação contínua e segura:
- automação por RTC (DS3231)
- controle de temperatura e ventoinha com histerese/failsafe
- OTA protegido por credenciais
- API com token
- frontend instalável no Android (PWA)
- recuperação de Wi-Fi com Access Point de configuração

## Estado atual do projeto

### Firmware (`firmware/aquarium`)
- Arquitetura modular em C++ (Arduino/ESP32).
- Loop principal não bloqueante (funcionalidades locais continuam mesmo sem rede).
- Módulos:
  - `light.*`: luminária com botão físico e automação por horário
  - `temperature.*`: DS18B20 com leitura não bloqueante
  - `fan.*`: modos AUTO/MANUAL, histerese e cooldown
  - `rtc_manager.*`: DS3231 + sincronização NTP (com retentativas)
  - `wifi_manager.*`: reconexão automática + AP de recuperação + persistência de credenciais em NVS
  - `web_server.*`: API REST local + OTA + portal `/wifi-setup`

### Servidor (`server`)
- Endpoint oculto via rewrite: `cache-<secret>.js`.
- Proxy entre navegador e ESP32 (evita expor IP local no frontend).
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
   ├─ pinagem-e-montagem-esp32.md
   └─ deploy-servidor.md
```

## Fluxo de funcionamento

1. O usuário acessa `https://.../cache-<secret>.js`.
2. O `.htaccess` reescreve para `server/index.php?key=<secret>`.
3. `index.php` valida o secret.
4. Sem `?api=`, entrega `app.html`.
5. Com `?api=...`, faz proxy HTTP para o ESP32 e retorna JSON.
6. Se `?manifest=1` ou `?sw=1`, entrega recursos PWA dinâmicos.

## Endpoints

### No servidor (`server/index.php`)
- `GET /cache-<secret>.js` -> app HTML
- `GET /cache-<secret>.js?api=status`
- `GET /cache-<secret>.js?api=toggle`
- `GET /cache-<secret>.js?api=temperature`
- `GET /cache-<secret>.js?api=fan_toggle`
- `GET /cache-<secret>.js?api=fan_speed&value=0..100`
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

## PWA (Android)

- `app.html` inclui manifest e registro do service worker.
- Instalação via fluxo nativo do Chrome no Android.
- `start_url`/`id` usam a URL protegida atual (`cache-<secret>.js`).
- Em offline, o shell abre; ações de API dependem de rede local com ESP32.

## Configuração

### Firmware
1. Copie `firmware/aquarium/config.example.h` para `firmware/aquarium/config.h`.
2. Preencha Wi-Fi, horários, token da API e credenciais OTA.
3. Faça upload do firmware no ESP32.

### Servidor
1. Copie `server/config.example.php` para `server/config.php`.
2. Configure:
   - `secret`
   - `esp32_ip`
   - `esp32_port`
   - `esp32_api_token` (igual ao `API_AUTH_TOKEN` do firmware)
3. Garanta `mod_rewrite` e leitura do `.htaccess`.
4. Faça deploy em HTTPS (necessário para PWA instalável no Android).

## CORS: como preencher `CORS_ALLOWED_ORIGIN`

Use apenas a origem (`scheme + host + porta`), sem caminho e sem barra final.

Exemplos:
- correto: `https://boasementestore.com.br`
- incorreto: `https://boasementestore.com.br/`
- incorreto: `https://boasementestore.com.br/wp-content/uploads/.cache-api`

Se você acessar o app por outro host (ex.: `https://www.boasementestore.com.br`), a origem muda e precisa bater exatamente com o valor configurado.

## Hardware e montagem

A documentação elétrica e de pinagem está em:
- `docs/pinagem-e-montagem-esp32.md`

Pontos importantes:
- SSR controla AC: manter isolamento físico e boas práticas de segurança.
- Ventoinha 12V com GND comum ao ESP32.
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
