<?php
/**
 * @file index.php
 * @brief Proxy PHP entre o browser e o ESP32.
 *
 * Dois modos de operação:
 *   - ?manifest=1 → retorna o manifest PWA dinâmico
 *   - ?sw=1       → retorna o service worker dinâmico
 *   - Sem ?api    → serve a interface web (app.html)
 *   - Com ?api    → publica/le via MQTT TLS e retorna JSON
 *
 * O secret é injetado pela reescrita do .htaccess: cache-<SECRET>.js → ?key=<SECRET>
 */

ini_set('display_errors', 0);
ini_set('log_errors', '1');
error_reporting(E_ALL);

// ---------------------------------------------------------------------------
// Carrega configuração
// ---------------------------------------------------------------------------
$config      = require __DIR__ . '/config.php';
$SECRET      = $config['secret'];
$MQTT_HOST    = $config['mqtt_host'];
$MQTT_PORT    = (int)$config['mqtt_port'];
$MQTT_USER    = $config['mqtt_user'];
$MQTT_PASS    = $config['mqtt_password'];

// ---------------------------------------------------------------------------
// Valida secret (enviado pelo .htaccess via rewrite: ?key=<SECRET>)
// ---------------------------------------------------------------------------
$key = $_GET['key'] ?? '';
if ($key !== $SECRET) {
    http_response_code(403);
    exit('Forbidden');
}

// ---------------------------------------------------------------------------
// Contexto da URL pública (necessário para manifest/shell do PWA)
// ---------------------------------------------------------------------------
$requestUri  = $_SERVER['REQUEST_URI'] ?? '';
$requestPath = parse_url($requestUri, PHP_URL_PATH) ?: '/';

$isHttps = (
    (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
    || (isset($_SERVER['SERVER_PORT']) && (int) $_SERVER['SERVER_PORT'] === 443)
);

$forwardedProto = $_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '';
if (!empty($forwardedProto)) {
    $firstProto = strtolower(trim(explode(',', $forwardedProto)[0]));
    $isHttps = ($firstProto === 'https');
}

$scheme = $isHttps ? 'https' : 'http';
$host   = $_SERVER['HTTP_HOST'] ?? 'localhost';
$origin = $scheme . '://' . $host;
$appUrl = $origin . $requestPath;

$basePath = rtrim(str_replace('\\', '/', dirname($requestPath)), '/');
if ($basePath === '') {
    $basePath = '/';
}

$assetsPrefix = ($basePath === '/') ? '' : $basePath;
$icon192Path  = $assetsPrefix . '/assets/betta-icon-192.png';
$icon512Path  = $assetsPrefix . '/assets/betta-icon-512.png';
$icon192Url   = $origin . $icon192Path;
$icon512Url   = $origin . $icon512Path;
$manifestUrl  = $appUrl . '?manifest=1';

// ---------------------------------------------------------------------------
// Modo PWA: manifest dinâmico
// ---------------------------------------------------------------------------
if (isset($_GET['manifest']) && $_GET['manifest'] === '1') {
    $manifest = [
        'name'             => 'Betta Care',
        'short_name'       => 'Betta',
        'display'          => 'standalone',
        'orientation'      => 'portrait',
        'theme_color'      => '#070d1a',
        'background_color' => '#070d1a',
        'start_url'        => $appUrl,
        'id'               => $appUrl,
        'icons'            => [
            [
                'src'   => $icon192Url,
                'sizes' => '192x192',
                'type'  => 'image/png',
            ],
            [
                'src'   => $icon512Url,
                'sizes' => '512x512',
                'type'  => 'image/png',
            ],
        ],
    ];

    header('Content-Type: application/manifest+json; charset=utf-8');
    header('Cache-Control: no-cache, must-revalidate');
    echo json_encode($manifest, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

// ---------------------------------------------------------------------------
// Modo PWA: service worker dinâmico
// ---------------------------------------------------------------------------
if (isset($_GET['sw']) && $_GET['sw'] === '1') {
    $cacheSeed = implode('|', [
        $SECRET,
        (string) @filemtime(__DIR__ . '/app.html'),
        (string) @filemtime(__DIR__ . '/assets/betta-icon-192.png'),
        (string) @filemtime(__DIR__ . '/assets/betta-icon-512.png'),
    ]);
    $cacheName = 'smart-aquarium-pwa-' . substr(hash('sha256', $cacheSeed), 0, 12);

    $cacheNameJson = json_encode($cacheName, JSON_UNESCAPED_SLASHES);
    $appUrlJson    = json_encode($appUrl, JSON_UNESCAPED_SLASHES);
    $icon192Json   = json_encode($icon192Url, JSON_UNESCAPED_SLASHES);
    $precacheJson  = json_encode(
        [$appUrl, $manifestUrl, $icon192Url, $icon512Url],
        JSON_UNESCAPED_SLASHES
    );

    header('Content-Type: application/javascript; charset=utf-8');
    header('Cache-Control: no-cache, must-revalidate');

    echo <<<JS
const CACHE_NAME = {$cacheNameJson};
const APP_URL = {$appUrlJson};
const IMAGE_FALLBACK_URL = {$icon192Json};
const PRECACHE_URLS = {$precacheJson};

self.addEventListener('install', (event) => {
  event.waitUntil((async () => {
    const cache = await caches.open(CACHE_NAME);
    await cache.addAll(PRECACHE_URLS);
    await self.skipWaiting();
  })());
});

self.addEventListener('activate', (event) => {
  event.waitUntil((async () => {
    const keys = await caches.keys();
    await Promise.all(
      keys
        .filter((key) => key !== CACHE_NAME)
        .map((key) => caches.delete(key))
    );
    await self.clients.claim();
  })());
});

self.addEventListener('fetch', (event) => {
  const { request } = event;
  if (request.method !== 'GET') {
    return;
  }

  const url = new URL(request.url);
  if (url.origin !== self.location.origin) {
    return;
  }

  // Endpoints da API permanecem network-first (sem fallback offline)
  if (url.searchParams.has('api')) {
    event.respondWith(fetch(request));
    return;
  }

  // Navegação: tenta rede e cai para shell em cache no offline
  if (request.mode === 'navigate') {
    event.respondWith((async () => {
      try {
        const fresh = await fetch(request);
        const cache = await caches.open(CACHE_NAME);
        await cache.put(APP_URL, fresh.clone());
        return fresh;
      } catch (error) {
        const cachedShell = await caches.match(APP_URL);
        if (cachedShell) {
          return cachedShell;
        }
        throw error;
      }
    })());
    return;
  }

  event.respondWith((async () => {
    const cached = await caches.match(request);
    if (cached) {
      return cached;
    }

    try {
      const fresh = await fetch(request);
      if (PRECACHE_URLS.includes(url.href)) {
        const cache = await caches.open(CACHE_NAME);
        await cache.put(request, fresh.clone());
      }
      return fresh;
    } catch (error) {
      if (request.destination === 'image') {
        const fallback = await caches.match(IMAGE_FALLBACK_URL);
        if (fallback) {
          return fallback;
        }
      }
      throw error;
    }
  })());
});
JS;
    exit;
}

// ---------------------------------------------------------------------------
// Roteamento: sem ?api → serve o HTML; com ?api=<endpoint> → proxy MQTT
// ---------------------------------------------------------------------------
$api = isset($_GET['api']) ? trim($_GET['api']) : null;

if ($api === null) {
    // Modo HTML: serve a interface web
    header('Content-Type: text/html; charset=utf-8');
    readfile(__DIR__ . '/app.html');
    exit;
}

// ---------------------------------------------------------------------------
// Modo proxy: publica comandos no HiveMQ e/ou lê estado retido do ESP32
// ---------------------------------------------------------------------------
$allowed = [
    'status', 'toggle', 'temperature', 'fan_toggle', 'fan_speed',
    'logs', 'logs_clear', 'diag_mqtt',
    'rtc_sync', 'fan_config', 'light_schedule',
];
if (!in_array($api, $allowed, true)) {
    http_response_code(400);
    header('Content-Type: application/json');
    echo json_encode(['error' => 'Endpoint desconhecido: ' . htmlspecialchars($api)]);
    exit;
}

header('Content-Type: application/json');
if (!empty($_SERVER['HTTP_ORIGIN']) && $_SERVER['HTTP_ORIGIN'] === $origin) {
    header('Access-Control-Allow-Origin: ' . $origin);
    header('Vary: Origin');
}

$includeDetails = isset($_GET['debug']) && $_GET['debug'] === '1';
$mqttClientPath = __DIR__ . '/mqtt_client.php';
if (!is_file($mqttClientPath)) {
    http_response_code(500);
    echo json_encode([
        'error' => 'Modulo interno MQTT ausente no servidor.',
        'hint'  => 'Publique o arquivo mqtt_client.php junto com index.php.',
    ]);
    error_log('[cache-api] mqtt_client.php ausente no servidor.');
    exit;
}

require_once $mqttClientPath;

/**
 * @brief Envia erro JSON padronizado para API e registra no error_log.
 */
function api_fail(
    int $status,
    string $message,
    ?Throwable $exception = null,
    bool $includeDetails = false
): void {
    http_response_code($status);

    $payload = ['error' => $message];
    if ($exception !== null) {
        $payload['hint'] = 'Verifique mqtt_host/mqtt_port/mqtt_user/mqtt_password e saida TLS MQTT (porta 8883).';
        if ($includeDetails) {
            $payload['details'] = $exception->getMessage();
        }
        error_log(sprintf(
            '[cache-api] %s | %s in %s:%d',
            $message,
            $exception->getMessage(),
            $exception->getFile(),
            $exception->getLine()
        ));
    } else {
        error_log('[cache-api] ' . $message);
    }

    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
}

/**
 * @brief Valida horário no formato HH:MM (24h).
 */
function is_valid_hhmm(string $time): bool
{
    if (!preg_match('/^\d{2}:\d{2}$/', $time)) {
        return false;
    }

    [$h, $m] = array_map('intval', explode(':', $time));
    return $h >= 0 && $h <= 23 && $m >= 0 && $m <= 59;
}

// Despacha a requisição da API
try {
    switch ($api) {

        case 'status':
        case 'temperature':  // temperatura está inclusa no payload de aquarium/status
            echo json_encode(mqtt_get_status(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS
            ));
            break;

        case 'toggle':
            echo json_encode(mqtt_send_command(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS,
                'aquarium/cmd/light', 'toggle'
            ));
            break;

        case 'fan_toggle':
            echo json_encode(mqtt_send_command(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS,
                'aquarium/cmd/fan', 'toggle'
            ));
            break;

        case 'fan_speed':
            if (!isset($_GET['value'])) {
                http_response_code(400);
                echo json_encode(['error' => 'Parâmetro value obrigatório para fan_speed']);
                break;
            }
            $fanSpeed = filter_var($_GET['value'], FILTER_VALIDATE_INT,
                                   ['options' => ['min_range' => 0, 'max_range' => 100]]);
            if ($fanSpeed === false || $fanSpeed === null) {
                http_response_code(400);
                echo json_encode(['error' => 'Parâmetro value inválido (use inteiro de 0 a 100)']);
                break;
            }
            echo json_encode(mqtt_send_command(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS,
                'aquarium/cmd/fan/speed', (string)$fanSpeed
            ));
            break;

        case 'rtc_sync':
            echo json_encode(mqtt_send_command(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS,
                'aquarium/cmd/rtc/sync', 'sync', 4800
            ));
            break;

        case 'fan_config':
            if (!isset($_GET['trigger']) || !isset($_GET['off'])) {
                http_response_code(400);
                echo json_encode([
                    'error' => 'Parâmetros trigger e off são obrigatórios para fan_config',
                ]);
                break;
            }

            $trigger = filter_var($_GET['trigger'], FILTER_VALIDATE_FLOAT);
            $off     = filter_var($_GET['off'], FILTER_VALIDATE_FLOAT);
            if ($trigger === false || $off === false) {
                http_response_code(400);
                echo json_encode([
                    'error' => 'Parâmetros trigger/off inválidos (use número decimal).',
                ]);
                break;
            }

            $trigger = round((float)$trigger, 1);
            $off     = round((float)$off, 1);
            if ($trigger < 15.0 || $trigger > 45.0 || $off < 10.0 || $off > 44.5 || $trigger <= $off) {
                http_response_code(400);
                echo json_encode([
                    'error' => 'Valores fora da faixa segura (trigger 15.0..45.0, off 10.0..44.5, trigger > off).',
                ]);
                break;
            }

            $payload = json_encode(
                ['trigger' => $trigger, 'off' => $off],
                JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES
            );

            echo json_encode(mqtt_send_command(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS,
                'aquarium/cmd/fan/config', $payload
            ));
            break;

        case 'light_schedule':
            if (!isset($_GET['on']) || !isset($_GET['off'])) {
                http_response_code(400);
                echo json_encode([
                    'error' => 'Parâmetros on e off são obrigatórios para light_schedule',
                ]);
                break;
            }

            $on  = trim((string)$_GET['on']);
            $off = trim((string)$_GET['off']);
            if (!is_valid_hhmm($on) || !is_valid_hhmm($off)) {
                http_response_code(400);
                echo json_encode([
                    'error' => 'Formato inválido. Use HH:MM (24h).',
                ]);
                break;
            }

            $payload = json_encode(
                ['on' => $on, 'off' => $off],
                JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES
            );
            echo json_encode(mqtt_send_command(
                $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS,
                'aquarium/cmd/light/schedule', $payload
            ));
            break;

        case 'logs':
            echo json_encode([
                'logs' => mqtt_get_logs(
                    $MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS
                )
            ]);
            break;

        case 'logs_clear':
            mqtt_clear_logs($MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS);
            echo json_encode(['ok' => true]);
            break;

        case 'diag_mqtt':
            if (!$includeDetails) {
                http_response_code(400);
                echo json_encode([
                    'error' => 'Use ?debug=1 para habilitar diagnostico de conectividade.',
                ]);
                break;
            }
            echo json_encode(
                mqtt_diag_connectivity($MQTT_HOST, $MQTT_PORT, $MQTT_USER, $MQTT_PASS),
                JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES
            );
            break;
    }
} catch (Throwable $e) {
    api_fail(502, 'Falha ao comunicar com o broker MQTT.', $e, $includeDetails);
}
