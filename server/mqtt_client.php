<?php
declare(strict_types=1);

/**
 * Cliente MQTT 3.1.1 sobre TLS (porta 8883), sem dependencias externas.
 */

/**
 * @param resource $socket
 */
function mqtt_set_timeout($socket, float $seconds): void
{
    $seconds = max(0.1, $seconds);
    $sec = (int) floor($seconds);
    $usec = (int) floor(($seconds - $sec) * 1000000);
    stream_set_timeout($socket, $sec, $usec);
}

/**
 * @param resource $socket
 */
function mqtt_read_exact($socket, int $length): string
{
    $data = '';
    while (strlen($data) < $length) {
        $chunk = fread($socket, $length - strlen($data));
        if ($chunk === false || $chunk === '') {
            $meta = stream_get_meta_data($socket);
            if (!empty($meta['timed_out'])) {
                throw new RuntimeException('Timeout de leitura no socket.');
            }
            if (!empty($meta['eof'])) {
                throw new RuntimeException('Conexao encerrada pelo broker.');
            }
            usleep(10000);
            continue;
        }
        $data .= $chunk;
    }

    return $data;
}

/**
 * @param resource $socket
 */
function mqtt_write_all($socket, string $data): void
{
    $written = 0;
    $total = strlen($data);

    while ($written < $total) {
        $chunk = fwrite($socket, substr($data, $written));
        if ($chunk === false || $chunk === 0) {
            $meta = stream_get_meta_data($socket);
            if (!empty($meta['timed_out'])) {
                throw new RuntimeException('Timeout de escrita no socket.');
            }
            throw new RuntimeException('Falha ao escrever no socket.');
        }
        $written += $chunk;
    }
}

/**
 * @return resource
 */
function mqtt_open_tls(string $host, int $port, float $timeoutSec = 10.0)
{
    $sslContext = stream_context_create([
        'ssl' => [
            'verify_peer'       => true,
            'verify_peer_name'  => true,
            'allow_self_signed' => false,
            'SNI_enabled'       => true,
            'peer_name'         => $host,
        ],
    ]);

    $errno = 0;
    $errstr = '';
    $socket = @stream_socket_client(
        "tls://{$host}:{$port}",
        $errno,
        $errstr,
        $timeoutSec,
        STREAM_CLIENT_CONNECT,
        $sslContext
    );

    if (!is_resource($socket)) {
        throw new RuntimeException("Falha ao abrir socket TLS para {$host}:{$port} ({$errno}: {$errstr}).");
    }

    stream_set_blocking($socket, true);
    mqtt_set_timeout($socket, $timeoutSec);
    return $socket;
}

function mqtt_encode_string(string $value): string
{
    return pack('n', strlen($value)) . $value;
}

function mqtt_encode_remaining_length(int $length): string
{
    if ($length < 0) {
        throw new RuntimeException('Remaining length invalido.');
    }

    $encoded = '';
    do {
        $digit = $length % 128;
        $length = intdiv($length, 128);
        if ($length > 0) {
            $digit |= 0x80;
        }
        $encoded .= chr($digit);
    } while ($length > 0);

    return $encoded;
}

function mqtt_build_packet(int $firstByte, string $body): string
{
    return chr($firstByte) . mqtt_encode_remaining_length(strlen($body)) . $body;
}

function mqtt_build_connect_packet(string $clientId, string $username, string $password): string
{
    $hasUser = $username !== '';
    $hasPass = $password !== '';
    if ($hasPass && !$hasUser) {
        throw new RuntimeException('Configuracao MQTT invalida: senha sem usuario.');
    }

    $flags = 0x02; // Clean Session
    if ($hasUser) {
        $flags |= 0x80;
    }
    if ($hasPass) {
        $flags |= 0x40;
    }

    $variableHeader =
        mqtt_encode_string('MQTT') .
        chr(0x04) .                 // MQTT 3.1.1
        chr($flags) .
        pack('n', 30);              // keep alive 30s

    $payload = mqtt_encode_string($clientId);
    if ($hasUser) {
        $payload .= mqtt_encode_string($username);
    }
    if ($hasPass) {
        $payload .= mqtt_encode_string($password);
    }

    return mqtt_build_packet(0x10, $variableHeader . $payload);
}

function mqtt_build_disconnect_packet(): string
{
    return "\xE0\x00";
}

function mqtt_build_subscribe_packet(int $packetId, string $topic): string
{
    $body = pack('n', $packetId) . mqtt_encode_string($topic) . chr(0x00); // QoS0
    return mqtt_build_packet(0x82, $body);
}

function mqtt_build_publish_packet(string $topic, string $payload, bool $retain): string
{
    $firstByte = 0x30 | ($retain ? 0x01 : 0x00); // QoS0
    $body = mqtt_encode_string($topic) . $payload;
    return mqtt_build_packet($firstByte, $body);
}

/**
 * @param resource $socket
 * @return array{type:int,flags:int,body:string}
 */
function mqtt_wait_next_packet($socket, float $timeoutSec = 8.0): array
{
    mqtt_set_timeout($socket, $timeoutSec);

    $firstByteRaw = mqtt_read_exact($socket, 1);
    $firstByte = ord($firstByteRaw[0]);

    $multiplier = 1;
    $remainingLength = 0;
    do {
        $digitRaw = mqtt_read_exact($socket, 1);
        $digit = ord($digitRaw[0]);
        $remainingLength += ($digit & 0x7F) * $multiplier;
        $multiplier *= 128;
        if ($multiplier > 128 * 128 * 128 * 128) {
            throw new RuntimeException('Remaining length MQTT invalido.');
        }
    } while (($digit & 0x80) !== 0);

    $body = $remainingLength > 0 ? mqtt_read_exact($socket, $remainingLength) : '';

    return [
        'type'  => ($firstByte >> 4) & 0x0F,
        'flags' => $firstByte & 0x0F,
        'body'  => $body,
    ];
}

/**
 * @return array{topic:string,payload:string,qos:int,retain:bool,packet_id:int|null}
 */
function mqtt_parse_publish_packet(array $packet): array
{
    $body = $packet['body'];
    if (strlen($body) < 2) {
        throw new RuntimeException('PUBLISH MQTT invalido.');
    }

    $topicLen = unpack('nlen', substr($body, 0, 2))['len'];
    $offset = 2;
    if (strlen($body) < $offset + $topicLen) {
        throw new RuntimeException('PUBLISH MQTT truncado no topico.');
    }

    $topic = substr($body, $offset, $topicLen);
    $offset += $topicLen;

    $qos = ($packet['flags'] >> 1) & 0x03;
    $packetId = null;
    if ($qos > 0) {
        if (strlen($body) < $offset + 2) {
            throw new RuntimeException('PUBLISH MQTT truncado no packet id.');
        }
        $packetId = unpack('nid', substr($body, $offset, 2))['id'];
        $offset += 2;
    }

    return [
        'topic'     => $topic,
        'payload'   => substr($body, $offset),
        'qos'       => $qos,
        'retain'    => ($packet['flags'] & 0x01) === 0x01,
        'packet_id' => $packetId,
    ];
}

/**
 * @return resource
 */
function mqtt_connect_broker(string $host, int $port, string $username, string $password)
{
    $socket = mqtt_open_tls($host, $port, 10.0);

    try {
        $clientId = 'aquarium-php-tls-' . bin2hex(random_bytes(3));
        mqtt_write_all($socket, mqtt_build_connect_packet($clientId, $username, $password));

        $connAck = mqtt_wait_next_packet($socket, 8.0);
        if ($connAck['type'] !== 2 || strlen($connAck['body']) < 2) {
            throw new RuntimeException('CONNACK invalido do broker MQTT.');
        }

        $returnCode = ord($connAck['body'][1]);
        if ($returnCode !== 0x00) {
            $errors = [
                0x01 => 'Protocolo MQTT inaceitavel.',
                0x02 => 'Client ID rejeitado.',
                0x03 => 'Broker indisponivel.',
                0x04 => 'Usuario/senha invalidos.',
                0x05 => 'Nao autorizado.',
            ];
            $message = $errors[$returnCode] ?? 'Erro desconhecido no CONNACK.';
            throw new RuntimeException("Broker recusou conexao MQTT (code {$returnCode}): {$message}");
        }

        return $socket;
    } catch (Throwable $e) {
        if (is_resource($socket)) {
            fclose($socket);
        }
        throw $e;
    }
}

/**
 * @param resource|null $socket
 */
function mqtt_disconnect_broker($socket): void
{
    if (!is_resource($socket)) {
        return;
    }

    try {
        mqtt_write_all($socket, mqtt_build_disconnect_packet());
    } catch (Throwable $e) {
        // Ignora erro de fechamento.
    }
    fclose($socket);
}

/**
 * @param resource $socket
 */
function mqtt_subscribe_receive_payload($socket, string $topic, float $timeoutSec): ?string
{
    $packetId = random_int(1, 65535);
    mqtt_write_all($socket, mqtt_build_subscribe_packet($packetId, $topic));

    $deadline = microtime(true) + max(0.1, $timeoutSec);
    $subAcked = false;

    while (microtime(true) < $deadline) {
        $remaining = max(0.1, $deadline - microtime(true));
        $packet = mqtt_wait_next_packet($socket, $remaining);

        if ($packet['type'] === 9) { // SUBACK
            if (strlen($packet['body']) >= 2) {
                $ackId = unpack('nid', substr($packet['body'], 0, 2))['id'];
                if ($ackId === $packetId) {
                    $subAcked = true;
                }
            }
            continue;
        }

        if ($packet['type'] === 3) { // PUBLISH
            $publish = mqtt_parse_publish_packet($packet);
            if ($publish['topic'] === $topic) {
                return $publish['payload'];
            }
            continue;
        }
    }

    if (!$subAcked) {
        throw new RuntimeException("SUBACK nao recebido para o topico {$topic}.");
    }

    return null;
}

function mqtt_get_status(string $host, int $port, string $user, string $pass): array
{
    $socket = mqtt_connect_broker($host, $port, $user, $pass);
    try {
        $payload = mqtt_subscribe_receive_payload($socket, 'aquarium/status', 4.0);
    } finally {
        mqtt_disconnect_broker($socket);
    }

    if ($payload === null) {
        return ['error' => 'ESP32 offline ou sem estado publicado no broker'];
    }

    $decoded = json_decode($payload, true);
    if (!is_array($decoded)) {
        return ['error' => 'Estado MQTT recebido em formato invalido'];
    }
    return $decoded;
}

function mqtt_send_command(
    string $host,
    int $port,
    string $user,
    string $pass,
    string $topic,
    string $payload,
    int $waitMs = 350
): array {
    $socket = mqtt_connect_broker($host, $port, $user, $pass);
    try {
        mqtt_write_all($socket, mqtt_build_publish_packet($topic, $payload, false));
    } finally {
        mqtt_disconnect_broker($socket);
    }

    usleep(max(0, $waitMs) * 1000);
    return mqtt_get_status($host, $port, $user, $pass);
}

function mqtt_get_logs(string $host, int $port, string $user, string $pass): array
{
    $socket = mqtt_connect_broker($host, $port, $user, $pass);
    try {
        $payload = mqtt_subscribe_receive_payload($socket, 'aquarium/logs', 3.5);
    } finally {
        mqtt_disconnect_broker($socket);
    }

    if ($payload === null) {
        return [];
    }

    $decoded = json_decode($payload, true);
    return is_array($decoded) ? $decoded : [];
}

function mqtt_clear_logs(string $host, int $port, string $user, string $pass): void
{
    $socket = mqtt_connect_broker($host, $port, $user, $pass);
    try {
        // Limpa retained no broker.
        mqtt_write_all($socket, mqtt_build_publish_packet('aquarium/logs', '[]', true));
        // Comanda ESP32 para limpar buffer local.
        mqtt_write_all($socket, mqtt_build_publish_packet('aquarium/cmd/logs/clear', 'clear', false));
    } finally {
        mqtt_disconnect_broker($socket);
    }
}

function mqtt_diag_connectivity(string $host, int $port, string $user, string $pass): array
{
    $report = [
        'host' => $host,
        'port' => $port,
        'dns'  => [
            'a_records' => [],
            'ok'        => false,
        ],
        'tcp'  => [
            'ok'     => false,
            'errno'  => null,
            'error'  => null,
            'ms'     => null,
        ],
        'tls'  => [
            'ok'             => false,
            'errno'          => null,
            'error'          => null,
            'ms'             => null,
            'peer_cn'        => null,
            'peer_issuer_cn' => null,
        ],
        'mqtt' => [
            'ok'    => false,
            'error' => null,
            'ms'    => null,
        ],
    ];

    $dns = @gethostbynamel($host);
    if (is_array($dns) && count($dns) > 0) {
        $report['dns']['a_records'] = $dns;
        $report['dns']['ok'] = true;
    }

    $errno = 0;
    $errstr = '';
    $t0 = microtime(true);
    $tcp = @fsockopen($host, $port, $errno, $errstr, 8);
    $report['tcp']['ms'] = (int) round((microtime(true) - $t0) * 1000);
    $report['tcp']['ok'] = is_resource($tcp);
    $report['tcp']['errno'] = $errno;
    $report['tcp']['error'] = $errstr;
    if (is_resource($tcp)) {
        fclose($tcp);
    }

    $errno = 0;
    $errstr = '';
    $sslContext = stream_context_create([
        'ssl' => [
            'verify_peer'       => true,
            'verify_peer_name'  => true,
            'allow_self_signed' => false,
            'SNI_enabled'       => true,
            'peer_name'         => $host,
            'capture_peer_cert' => true,
        ],
    ]);
    $t0 = microtime(true);
    $tls = @stream_socket_client(
        "tls://{$host}:{$port}",
        $errno,
        $errstr,
        10,
        STREAM_CLIENT_CONNECT,
        $sslContext
    );
    $report['tls']['ms'] = (int) round((microtime(true) - $t0) * 1000);
    $report['tls']['ok'] = is_resource($tls);
    $report['tls']['errno'] = $errno;
    $report['tls']['error'] = $errstr;

    if (is_resource($tls)) {
        $params = stream_context_get_params($tls);
        if (isset($params['options']['ssl']['peer_certificate'])) {
            $certInfo = @openssl_x509_parse($params['options']['ssl']['peer_certificate']);
            if (is_array($certInfo)) {
                $report['tls']['peer_cn'] = $certInfo['subject']['CN'] ?? null;
                $report['tls']['peer_issuer_cn'] = $certInfo['issuer']['CN'] ?? null;
            }
        }
        fclose($tls);
    }

    $t0 = microtime(true);
    try {
        $mqtt = mqtt_connect_broker($host, $port, $user, $pass);
        $report['mqtt']['ok'] = true;
        mqtt_disconnect_broker($mqtt);
    } catch (Throwable $e) {
        $report['mqtt']['ok'] = false;
        $report['mqtt']['error'] = $e->getMessage();
    }
    $report['mqtt']['ms'] = (int) round((microtime(true) - $t0) * 1000);

    return $report;
}
