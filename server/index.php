<?php
/**
 * @file index.php
 * @brief Proxy PHP entre o browser e o ESP32.
 *
 * Dois modos de operação:
 *   - ?manifest=1 → retorna o manifest PWA dinâmico
 *   - ?sw=1       → retorna o service worker dinâmico
 *   - Sem ?api    → serve a interface web (app.html)
 *   - Com ?api    → repassa a requisição ao ESP32 via curl e retorna o JSON
 *
 * O secret é injetado pela reescrita do .htaccess: cache-<SECRET>.js → ?key=<SECRET>
 */

ini_set('display_errors', 0);
error_reporting(0);

// ---------------------------------------------------------------------------
// Carrega configuração
// ---------------------------------------------------------------------------
$config        = require __DIR__ . '/config.php';
$SECRET        = $config['secret'];
$ESP32_IP      = $config['esp32_ip'];
$ESP32_PORT    = $config['esp32_port'];
$ESP32_API_TOKEN = trim((string)($config['esp32_api_token'] ?? ''));
$ESP32_TIMEOUT = $config['esp32_timeout'];

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
// Roteamento: sem ?api → serve o HTML; com ?api=<endpoint> → proxy ESP32
// ---------------------------------------------------------------------------
$api = isset($_GET['api']) ? trim($_GET['api']) : null;

if ($api === null) {
    // Modo HTML: serve a interface web
    header('Content-Type: text/html; charset=utf-8');
    readfile(__DIR__ . '/app.html');
    exit;
}

// ---------------------------------------------------------------------------
// Modo proxy: repassa requisição para o ESP32
// ---------------------------------------------------------------------------
$allowed = ['status', 'toggle', 'temperature', 'fan_toggle', 'fan_speed'];
if (!in_array($api, $allowed, true)) {
    http_response_code(400);
    header('Content-Type: application/json');
    echo json_encode(['error' => 'Endpoint desconhecido: ' . htmlspecialchars($api)]);
    exit;
}

$url = "http://{$ESP32_IP}:{$ESP32_PORT}/{$api}";

// Para fan_speed: valida e repassa o parâmetro value ao ESP32
if ($api === 'fan_speed') {
    if (!isset($_GET['value'])) {
        http_response_code(400);
        header('Content-Type: application/json');
        echo json_encode(['error' => 'Parâmetro value obrigatório para fan_speed']);
        exit;
    }

    $rawFanSpeed = trim((string)$_GET['value']);
    if ($rawFanSpeed === '' || !preg_match('/^\d{1,3}$/', $rawFanSpeed)) {
        http_response_code(400);
        header('Content-Type: application/json');
        echo json_encode(['error' => 'Parâmetro value inválido (use inteiro de 0 a 100)']);
        exit;
    }

    $fanSpeed = (int)$rawFanSpeed;
    if ($fanSpeed < 0 || $fanSpeed > 100) {
        http_response_code(400);
        header('Content-Type: application/json');
        echo json_encode(['error' => 'Parâmetro value fora do intervalo (0-100)']);
        exit;
    }

    $url .= '?value=' . $fanSpeed;
}

$headers = ['Accept: application/json'];
if ($ESP32_API_TOKEN !== '') {
    $headers[] = 'X-Api-Token: ' . $ESP32_API_TOKEN;
}

$ch = curl_init($url);
curl_setopt_array($ch, [
    CURLOPT_RETURNTRANSFER => true,
    CURLOPT_TIMEOUT        => $ESP32_TIMEOUT,
    CURLOPT_CONNECTTIMEOUT => $ESP32_TIMEOUT,
    CURLOPT_HTTPHEADER     => $headers,
]);

$body   = curl_exec($ch);
$status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
$error  = curl_error($ch);
curl_close($ch);

header('Content-Type: application/json');
if (!empty($_SERVER['HTTP_ORIGIN']) && $_SERVER['HTTP_ORIGIN'] === $origin) {
    header('Access-Control-Allow-Origin: ' . $origin);
    header('Vary: Origin');
}

if ($body === false || !empty($error)) {
    http_response_code(502);
    echo json_encode([
        'error'   => 'ESP32 inacessível',
        'details' => $error,
    ]);
    exit;
}

http_response_code($status ?: 502);
echo $body;
