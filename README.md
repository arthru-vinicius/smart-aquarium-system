# Smart Aquarium System

Sistema de controle inteligente para aquário com interface web, desenvolvido para ESP32 com controle remoto via Wi-Fi.

## Visão Geral

O Smart Aquarium System é uma solução IoT simples e eficaz para controlar a iluminação de aquários remotamente. O sistema utiliza um ESP32 como controlador principal, oferecendo uma interface web responsiva para controle manual da luminária através de qualquer dispositivo conectado à mesma rede Wi-Fi.

## Arquitetura do Sistema

### Hardware
- **Microcontrolador**: ESP32-D0WD-V3
- **Relé**: SSR (Solid State Relay) conectado ao pino GPIO 23
- **Alimentação**: 5V via USB ou fonte externa
- **Conectividade**: Wi-Fi 802.11 b/g/n

### Software
- **Firmware**: Arduino IDE com bibliotecas específicas
- **Interface**: HTML5 + CSS3 + JavaScript vanilla
- **Comunicação**: HTTP REST API com JSON

## Tecnologias e Bibliotecas Utilizadas

### ESP32 - Bibliotecas Principais

#### 1. WiFi.h
```cpp
#include <WiFi.h>
```
**Função**: Biblioteca nativa do ESP32 para conectividade Wi-Fi
**Uso no projeto**:
- Configuração de IP estático
- Conexão automática à rede Wi-Fi
- Monitoramento do status de conexão
- Reconexão automática em caso de falha

**Principais métodos utilizados**:
- `WiFi.config()` - Configuração de IP estático
- `WiFi.begin()` - Inicialização da conexão
- `WiFi.status()` - Verificação do status
- `WiFi.localIP()` - Obtenção do IP atribuído

#### 2. ESPAsyncWebServer.h
```cpp
#include <ESPAsyncWebServer.h>
```
**Função**: Servidor web assíncrono de alta performance para ESP32
**Vantagens**:
- Não bloqueia o loop principal
- Suporte a múltiplas conexões simultâneas
- Baixo consumo de recursos
- Resposta rápida a requisições

**Uso no projeto**:
- Criação de endpoints REST
- Gerenciamento de requisições HTTP
- Configuração de headers CORS
- Resposta em formato JSON

#### 3. ArduinoJson.h
```cpp
#include <ArduinoJson.h>
```
**Função**: Biblioteca para serialização/deserialização JSON
**Uso no projeto**:
- Formatação de respostas da API
- Estruturação de dados de status
- Comunicação padronizada com o frontend

## Comunicação Cliente-Servidor

### Protocolo HTTP REST API

O sistema implementa uma API REST simples com dois endpoints principais:

#### Endpoint: `/alt` (GET)
**Função**: Alterna o estado da luminária
**Resposta**:
```json
{
  "luminaria_ligada": true,
  "modo": "MANUAL",
  "hora": "--:--",
  "horario_automatico_ativo": false
}
```

#### Endpoint: `/status` (GET)
**Função**: Consulta o status atual do sistema
**Resposta**: Mesmo formato do endpoint `/alt`

### Configuração CORS

Para permitir requisições cross-origin do navegador:

```cpp
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
```

**Por que é necessário**:
- Navegadores bloqueiam requisições entre origens diferentes por segurança
- O HTML é aberto localmente (origin 'null')
- O ESP32 está em IP diferente (10.141.68.50)
- Headers CORS permitem essa comunicação

### Fluxo de Comunicação

1. **Cliente** faz requisição HTTP GET para ESP32
2. **ESP32** processa a requisição no handler assíncrono
3. **ESP32** executa a ação (alternar relé ou consultar status)
4. **ESP32** formata resposta JSON
5. **ESP32** envia resposta com headers CORS
6. **Cliente** recebe e processa a resposta
7. **Interface** é atualizada com novos dados

## Frontend - Tecnologias Web

### HTML5
- Estrutura semântica moderna
- Meta tags para responsividade
- Favicon em formato SVG inline

### CSS3
- Gradientes lineares para visual moderno
- Flexbox para layout responsivo
- Transições e animações suaves
- Media queries para dispositivos móveis
- Box-shadow e border-radius para design material

### JavaScript (Vanilla)
- Fetch API para requisições HTTP assíncronas
- Promises com async/await
- Manipulação dinâmica do DOM
- Event listeners para interatividade
- Tratamento de erros com try/catch

#### Exemplo de Requisição:
```javascript
const response = await fetch('http://10.141.68.50/alt', { method: 'GET' });
const data = await response.json();
updateStatus(data);
```

## Configuração de Rede

### IP Estático
```cpp
IPAddress local_IP(10, 141, 68, 50);
IPAddress gateway(10, 141, 68, 29);
IPAddress subnet(255, 255, 255, 0);
```

**Vantagens**:
- Endereço fixo e previsível
- Não depende de DHCP
- Facilita configuração do cliente
- Evita mudanças de IP após reinicialização

### Credenciais Wi-Fi
```cpp
const char* ssid = "S23 FE";
const char* password = "czrw8850";
```

## Características Técnicas

### Performance
- **Tempo de resposta**: < 100ms para requisições locais
- **Consumo de memória**: ~47KB de variáveis globais
- **Armazenamento**: ~1MB de código compilado
- **Conexões simultâneas**: Suporte a múltiplos clientes

### Confiabilidade
- Reconexão automática Wi-Fi
- Tratamento de erros no cliente
- Feedback visual de carregamento
- Estado persistente do relé

### Segurança
- Rede Wi-Fi protegida por WPA2
- Comunicação apenas na rede local
- Sem armazenamento de dados sensíveis
- Headers CORS configurados adequadamente

## Vantagens da Arquitetura Escolhida

1. **Simplicidade**: Código limpo e fácil de manter
2. **Performance**: Servidor assíncrono não bloqueia operações
3. **Flexibilidade**: Interface web acessível de qualquer dispositivo
4. **Escalabilidade**: Fácil adição de novos endpoints
5. **Manutenibilidade**: Separação clara entre hardware e interface
6. **Custo-benefício**: Utiliza componentes baratos e amplamente disponíveis

## Possíveis Melhorias Futuras

- Implementação de HTTPS para maior segurança
- Autenticação de usuários
- Logs de atividade
- Agendamento de horários
- Integração com sensores (temperatura, pH)
- Notificações push
- Interface mobile nativa

---

**Desenvolvido com foco em simplicidade, confiabilidade e facilidade de uso.**
