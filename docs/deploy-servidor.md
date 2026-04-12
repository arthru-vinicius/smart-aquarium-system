# 📄 Documento Técnico — Endpoint Oculto em Ambiente WordPress

## 🎯 Objetivo

Implementar um endpoint HTTP simples (GET/POST) dentro de um servidor WordPress existente, sem interferir no funcionamento da aplicação principal e mantendo baixa visibilidade para outros desenvolvedores.

O endpoint será utilizado para fins pessoais e não requer autenticação complexa nem persistência em banco de dados.

---

## 🏗️ Contexto do Ambiente

O servidor já hospeda uma aplicação baseada em WordPress com WooCommerce, utilizando a estrutura padrão:

* `wp-admin/`
* `wp-content/`
* `wp-includes/`
* arquivos raiz como `index.php`, `.htaccess`, `wp-config.php`, etc.

O WordPress utiliza regras de rewrite que redirecionam requisições não resolvidas para o `index.php`.

Importante:

* Requisições para arquivos/pastas que **existem fisicamente** não passam pelo WordPress
* Isso permite criar endpoints independentes dentro do mesmo servidor

---

## 📁 Estratégia de Implementação

O endpoint será implementado dentro do diretório:

```
/wp-content/uploads/.cache-api/
```

### Justificativa:

* `uploads` é um diretório já existente e funcional
* Diretórios iniciados com `.` são menos visíveis
* Não interfere no core do WordPress
* Não aparece na biblioteca de mídia (não indexado no banco)

---

## 🌐 Estratégia de Acesso (Segurança + Discrição)

O endpoint será acessado através de uma URL disfarçada como arquivo estático:

```
/wp-content/uploads/.cache-api/cache-<SECRET>.js
```

### Exemplo:

```
https://dominio.com/wp-content/uploads/.cache-api/cache-k92jd8s.js
```

### Características:

* O arquivo **não existe fisicamente**
* A URL é interceptada via `.htaccess`
* O `<SECRET>` funciona como chave de acesso
* A URL simula um asset comum (JS), reduzindo suspeita

---

## 🔧 Configuração do Servidor (.htaccess)

Arquivo localizado em:

```
/wp-content/uploads/.cache-api/.htaccess
```

Conteúdo:

```apache
Options -Indexes

RewriteEngine On

# Roteia requisições disfarçadas para o index.php
RewriteRule ^cache-([a-zA-Z0-9]+)\.js$ index.php?key=$1 [L]

# Bloqueia qualquer acesso direto que não siga o padrão
RewriteCond %{REQUEST_URI} !^/wp-content/uploads/.cache-api/cache-[a-zA-Z0-9]+\.js$
RewriteRule ^ - [F]
```

---

## 🧠 Lógica da Aplicação (index.php)

Responsabilidades:

1. Validar o secret recebido via URL
2. Permitir apenas requisições autorizadas
3. Processar métodos GET e POST
4. Retornar respostas simples (JSON ou HTML)

### Exemplo base:

```php
<?php

$SECRET = 'k92jd8s';

$key = $_GET['key'] ?? '';

if ($key !== $SECRET) {
    http_response_code(403);
    exit('Forbidden');
}

// Define resposta como JSON
header('Content-Type: application/json');

// Roteamento simples
$method = $_SERVER['REQUEST_METHOD'];

if ($method === 'GET') {
    echo json_encode([
        'status' => 'ok',
        'method' => 'GET'
    ]);
    exit;
}

if ($method === 'POST') {
    $input = file_get_contents('php://input');

    echo json_encode([
        'status' => 'ok',
        'method' => 'POST',
        'data' => $input
    ]);
    exit;
}

// Método não permitido
http_response_code(405);
echo json_encode(['error' => 'Method not allowed']);
```

---

## 🔒 Considerações de Segurança

* O endpoint não deve expor erros (desativar `display_errors`)
* O secret deve ser suficientemente aleatório
* Evitar uso de query strings para autenticação
* O uso de URL disfarçada reduz visibilidade, mas não substitui segurança real
* Idealmente rodar apenas sob HTTPS

---

## 👻 Discrição e Invisibilidade

Medidas adotadas:

* Diretório oculto (`.cache-api`)
* Nome que simula cache/asset
* Endpoint mascarado como `.js`
* Sem arquivos reais correspondentes
* Fora do core do WordPress

Resultado:

* Baixa probabilidade de detecção em debug casual
* Não interfere em deploy ou funcionamento do WP
* Não aparece no painel administrativo

---

## ⚠️ Restrições e Boas Práticas

Não fazer:

* Não modificar `wp-includes` ou `wp-admin`
* Não alterar `.htaccess` principal do WordPress
* Não expor o endpoint publicamente sem necessidade
* Não reutilizar o mesmo secret em outros contextos

---

## 🚀 Possíveis Extensões Futuras

* Adicionar logs de requisição
* Implementar whitelist de IP
* Adicionar múltiplas rotas internas
* Criar um mini-router
* Validar payloads JSON

---

## ✅ Conclusão

A solução proposta:

* É simples e eficiente
* Não interfere no WordPress
* Mantém baixo nível de exposição
* Atende ao objetivo de uso pessoal com segurança básica adequada

---