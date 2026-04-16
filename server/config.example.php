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

    // --- MQTT TLS (HiveMQ Cloud) ----------------------------------------------
    // Fluxo atual do servidor PHP: conexao MQTT segura (TLS) no broker.
    // Use os dados de host + porta TLS do painel HiveMQ.
    // Crie um usuário específico para o PHP no painel: https://console.hivemq.cloud
    'mqtt_host'     => 'SEU_CLUSTER.s1.eu.hivemq.cloud',
    'mqtt_port'     => 8883,
    'mqtt_user'     => 'betta-care-php',
    'mqtt_password' => 'SENHA_DO_PHP_AQUI',
];
