<?php
/**
 * @file config.example.php
 * @brief Template de configuração — copie para config.php e preencha os valores reais.
 *        config.php está no .gitignore e nunca deve ser versionado.
 */
return [
    // Secret que aparece na URL: /cache-api/cache-<SECRET>.js
    // Gere uma string aleatória, ex: openssl rand -hex 16
    'secret' => 'REPLACE_WITH_RANDOM_32_CHAR_STRING',

    // IP estático do ESP32 na rede local
    'esp32_ip'      => '10.141.68.50',
    'esp32_port'    => 80,

    // Token compartilhado com o firmware do ESP32 (API_AUTH_TOKEN em firmware/config.h)
    'esp32_api_token' => 'REPLACE_WITH_THE_SAME_TOKEN_USED_BY_ESP32',

    // Timeout das requisições curl para o ESP32 (segundos)
    'esp32_timeout' => 5,
];
