#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// CONFIGURACAO DO WI-FI
IPAddress local_IP(10, 141, 68, 50);
IPAddress gateway(10, 141, 68, 29);
IPAddress subnet(255, 255, 255, 0);

const char* ssid = "S23 FE";
const char* password = "czrw8850";

// Definição dos pinos
#define PIN_SSR    23

// Estado atual da luminária
bool luminariaLigada = false;

// Instância do servidor web
AsyncWebServer server(80);

/**
 * Liga ou desliga a luminária através do SSR
 * @param ligar - true para ligar, false para desligar
 */
void setLuminaria(bool ligar) {
  digitalWrite(PIN_SSR, ligar ? HIGH : LOW);
  luminariaLigada = ligar;
  Serial.print("Luminária: ");
  Serial.println(ligar ? "LIGADA" : "DESLIGADA");
}

/**
 * Conecta ao Wi-Fi com as configurações definidas
 * Tenta conectar em loop até conseguir
 */
void conectarWiFi() {
  // Configura IP estático
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha ao configurar IP estático");
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  
  // Loop até conectar
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("Wi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

/**
 * Configura os endpoints do servidor web
 */
void configurarServidor() {
  // Configuração CORS para todos os endpoints
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // Endpoint para alternar estado da luminária
  server.on("/alt", HTTP_GET, [](AsyncWebServerRequest *request){
    // Alterna o estado atual
    setLuminaria(!luminariaLigada);
    
    Serial.println("Estado alterado via web");
    
    // Resposta JSON compatível com a página
    DynamicJsonDocument doc(200);
    doc["luminaria_ligada"] = luminariaLigada;
    doc["modo"] = "MANUAL";
    doc["hora"] = "--:--";
    doc["horario_automatico_ativo"] = false;
    
    String response;
    serializeJson(doc, response);
    
    // Headers CORS específicos para esta resposta
    request->send(200, "application/json", response);
  });
  
  // Endpoint para consultar status atual
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(200);
    
    doc["luminaria_ligada"] = luminariaLigada;
    doc["modo"] = "MANUAL";
    doc["hora"] = "--:--";
    doc["horario_automatico_ativo"] = false;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
  });
  
  // Handler para requisições OPTIONS (preflight CORS)
  server.onNotFound([](AsyncWebServerRequest *request){
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "text/plain", "Not found");
    }
  });
  
  server.begin();
  Serial.println("Servidor web iniciado");
}

void setup() {
  // Inicialização serial
  Serial.begin(115200);
  delay(2000); // Aguarda estabilizar
  Serial.println("=== CONTROLE DE AQUARIO INICIANDO ===");

  // Inicialização dos pinos
  pinMode(PIN_SSR, OUTPUT);
  
  // Estado inicial: desligado
  setLuminaria(false);

  // Conecta ao Wi-Fi
  conectarWiFi();
  
  // Configura servidor web
  configurarServidor();
  
  Serial.println("Sistema pronto!");
}

void loop() {
  // Verifica se Wi-Fi ainda está conectado, reconecta se necessário
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado, tentando reconectar...");
    conectarWiFi();
  }

  delay(1000); // Loop mais lento, apenas para verificar Wi-Fi
}
