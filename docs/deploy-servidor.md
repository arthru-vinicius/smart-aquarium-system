# Deploy no EC2 (Amazon Linux 2023) com Docker + Nginx Gateway

Este guia cobre o deploy da aplicacao do aquario em um servidor com:

- `apps-gateway` (Nginx central em container)
- projetos multiplos no diretorio `/home/ec2-user/apps`
- backend PHP/FPM em container
- broker HiveMQ via MQTT TLS (`8883`)

## Decisao de transporte MQTT

A versao atual do `server/` usa MQTT TLS padrao (`8883`) e nao WebSocket.

Motivo pratico: menor overhead de protocolo no servidor (sem handshake/frame WebSocket) mantendo criptografia TLS fim a fim.

## Nome discreto do projeto no servidor

Diretorio recomendado:

- `/home/ec2-user/apps/rm-cache-runtime`

## 1) Subir o codigo no EC2

No EC2:

```bash
cd /home/ec2-user/apps
mkdir -p rm-cache-runtime
cd rm-cache-runtime
# clone/copie este repositorio aqui
```

Estrutura esperada:

- `/home/ec2-user/apps/rm-cache-runtime/server/index.php`
- `/home/ec2-user/apps/rm-cache-runtime/server/app.html`
- `/home/ec2-user/apps/rm-cache-runtime/server/mqtt_client.php`
- `/home/ec2-user/apps/rm-cache-runtime/server/config.php`

## 2) Configurar `server/config.php`

Base em `server/config.example.php`:

- `secret`
- `mqtt_host`
- `mqtt_port` = `8883`
- `mqtt_user`
- `mqtt_password`

## 3) Criar stack Docker do aquario (PHP-FPM)

Em `/home/ec2-user/apps/rm-cache-runtime/docker-compose.yml`:

```yaml
services:
  aquarium-php:
    image: php:8.3-fpm-alpine
    container_name: aquarium-php
    restart: unless-stopped
    volumes:
      - /home/ec2-user/apps/rm-cache-runtime/server:/var/www/rm-cache-runtime/server:ro
    working_dir: /var/www/rm-cache-runtime/server
    networks:
      - apps_gateway_net
    healthcheck:
      test: ["CMD-SHELL", "php-fpm -t || exit 1"]
      interval: 30s
      timeout: 5s
      retries: 3

networks:
  apps_gateway_net:
    external: true
    name: apps_gateway_net
```

Subir:

```bash
cd /home/ec2-user/apps/rm-cache-runtime
docker compose up -d
docker ps | grep aquarium-php
```

## 4) Ajustar gateway Nginx

Os arquivos de referencia foram atualizados neste repositorio em `tmp/`:

- `tmp/docker-compose.yml`
- `tmp/nginx.conf`

### 4.1 `docker-compose` do gateway

Adicionar volume read-only para o codigo do aquario:

- `/home/ec2-user/apps/rm-cache-runtime/server:/var/www/rm-cache-runtime/server:ro`

### 4.2 `nginx.conf` do gateway

Blocos adicionados para:

1. receber `/rm-cache-runtime/cache-<secret>.js`
2. encaminhar para `aquarium-php:9000` (FastCGI)
3. injetar `key=<secret>` no query string (equivalente ao `.htaccess`)
4. liberar somente icones PWA (`betta-icon-192/512`)
5. bloquear o resto de `/rm-cache-runtime/` com `403`

Aplicar e recarregar gateway:

```bash
cd /home/ec2-user/apps/rm-apps-gateway
docker compose up -d
docker exec -it apps-gateway nginx -t
docker exec -it apps-gateway nginx -s reload
```

## 5) Testes de conectividade MQTT no servidor

Teste no host EC2:

```bash
HOST="SEU_CLUSTER.s1.eu.hivemq.cloud"
PORT=8883
timeout 6 bash -lc "cat < /dev/null > /dev/tcp/$HOST/$PORT" && echo "TCP OK" || echo "TCP FALHOU"
timeout 10 openssl s_client -connect "$HOST:$PORT" -servername "$HOST" -brief < /dev/null
```

Teste dentro do container PHP:

```bash
docker exec -it aquarium-php php -r '
$h="SEU_CLUSTER.s1.eu.hivemq.cloud"; $p=8883;
$ctx=stream_context_create(["ssl"=>["verify_peer"=>true,"verify_peer_name"=>true,"peer_name"=>$h,"SNI_enabled"=>true]]);
$e=0; $s="";
$fp=@stream_socket_client("tls://$h:$p",$e,$s,8,STREAM_CLIENT_CONNECT,$ctx);
var_export(["ok"=>(bool)$fp,"errno"=>$e,"error"=>$s]); echo PHP_EOL;
if($fp) fclose($fp);
'
```

## 6) Testes funcionais da aplicacao

Assumindo dominio `router.recifemecatron.com`:

1. Abrir:
   - `https://router.recifemecatron.com/rm-cache-runtime/cache-<secret>.js`
2. Testar API:
   - `?api=status`
   - `?api=logs`
   - `?api=rtc_sync`
   - `?api=fan_config&trigger=29.0&off=27.5`
   - `?api=light_schedule&on=10:00&off=17:00`
3. Diagnostico detalhado:
   - `?api=diag_mqtt&debug=1`
4. Validar PWA:
   - `?manifest=1`
   - `?sw=1`
5. Confirmar bloqueio:
   - `https://router.recifemecatron.com/rm-cache-runtime/qualquer-coisa` -> `403`

## 7) Observacoes importantes

- Em Nginx, `.htaccess` nao tem efeito. As regras de seguranca ficam no `nginx.conf`.
- O endpoint protegido continua sendo `cache-<secret>.js`.
- Se `diag_mqtt` retornar `Connection refused` ou timeout, o bloqueio e de rede/egress do ambiente.
- Mantenha `server/config.php` fora de versionamento.
