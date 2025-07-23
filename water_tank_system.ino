#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <HardwareSerial.h>

// Configuracoes do sensor
HardwareSerial sensorSerial(2);
#define RXD2 16
#define TXD2 17
#define SENSOR_POWER_PIN 4

// Configuracoes WiFi
String wifiSSID = "";
String wifiPassword = "";
const char* apSSID = "WaterTank_Config";
const char* apPassword = "759684";

// Servidor web
AsyncWebServer server(80);

// Preferencias para armazenar configuracoes
Preferences preferences;

// Variaveis de controle
unsigned long lastWiFiAttempt = 0;
unsigned long wifiFailStartTime = 0;
unsigned long lastActivity = 0;
bool wifiFailMode = false;
bool accessPointMode = false;
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR unsigned long lastSuccessfulConnection = 0;

// Configuracoes de economia de energia
#define LIGHT_SLEEP_TIMEOUT 30000  // 30 segundos
#define WIFI_RECONNECT_INTERVAL 30000  // 30 segundos
#define AP_FALLBACK_TIME 172800  // 48 horas

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    bootCount++;
    Serial.println("=== SISTEMA INICIADO ===");
    Serial.printf("Boot numero: %d\n", bootCount);
    Serial.printf("Razao do reset: %d\n", esp_reset_reason());
    Serial.printf("Memoria livre: %d bytes\n", ESP.getFreeHeap());
    
    // Configurar pin de alimentacao do sensor
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, LOW);
    Serial.println("Sensor desligado inicialmente");
    
    // Inicializar preferencias
    preferences.begin("wifi", false);
    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    lastSuccessfulConnection = preferences.getULong("lastConn", 0);
    
    Serial.printf("SSID configurado: %s\n", wifiSSID.c_str());
    
    // Verificar se deve entrar em modo AP
    checkAccessPointFallback();
    
    if (!accessPointMode) {
        connectToWiFi();
    } else {
        startAccessPointMode();
    }
    
    // Configurar rotas do servidor
    setupWebServer();
    
    // Iniciar servidor
    server.begin();
    Serial.println("Servidor HTTP iniciado");
    
    // Configurar economia de energia (Light Sleep ao inves de Deep Sleep)
    esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_TIMEOUT * 1000);
    
    lastActivity = millis();
    Serial.println("=== SETUP CONCLUIDO ===");
}

void loop() {
    // Verificar reconexao WiFi se necessario
    if (!accessPointMode && WiFi.status() != WL_CONNECTED) {
        handleWiFiReconnection();
    }
    
    // Light sleep apos inatividade (mantem WiFi ativo)
    if (millis() - lastActivity > LIGHT_SLEEP_TIMEOUT) {
        Serial.println("Entrando em light sleep para economia de energia...");
        Serial.printf("WiFi status: %s\n", getWiFiStatusString().c_str());
        
        // Desligar sensor para economizar energia
        digitalWrite(SENSOR_POWER_PIN, LOW);
        
        // Light sleep mantem WiFi ativo
        esp_light_sleep_start();
        
        Serial.println("Acordou do light sleep");
        lastActivity = millis();
    }
    
    delay(1000);
}

void checkAccessPointFallback() {
    unsigned long now = millis() / 1000;
    unsigned long timeSinceLastSuccess = now - lastSuccessfulConnection;
    
    Serial.printf("Tempo desde ultima conexao bem-sucedida: %lu segundos\n", timeSinceLastSuccess);
    
    if (wifiSSID.length() == 0 || timeSinceLastSuccess > AP_FALLBACK_TIME) {
        accessPointMode = true;
        Serial.println("Entrando em modo AP fallback");
    }
}

void connectToWiFi() {
    if (wifiSSID.length() == 0) {
        Serial.println("ERRO: Nenhuma rede WiFi configurada");
        startAccessPointMode();
        return;
    }
    
    Serial.printf("Conectando ao WiFi: %s\n", wifiSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi conectado com sucesso!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        
        // Salvar timestamp da conexao bem-sucedida
        preferences.putULong("lastConn", millis() / 1000);
        wifiFailMode = false;
        
        // Configurar NTP
        configTime(0, 0, "pool.ntp.org");
        Serial.println("NTP configurado");
    } else {
        Serial.printf("ERRO: Falha na conexao WiFi (Status: %s)\n", getWiFiStatusString().c_str());
        if (!wifiFailMode) {
            wifiFailStartTime = millis();
            wifiFailMode = true;
        }
    }
}

void handleWiFiReconnection() {
    if (millis() - lastWiFiAttempt > WIFI_RECONNECT_INTERVAL) {
        lastWiFiAttempt = millis();
        Serial.printf("Tentando reconectar WiFi... (Status atual: %s)\n", getWiFiStatusString().c_str());
        WiFi.reconnect();
        
        // Se falhou por muito tempo, considerar modo AP
        if (wifiFailMode && (millis() - wifiFailStartTime > AP_FALLBACK_TIME * 1000)) {
            Serial.println("WiFi falhou por muito tempo, iniciando modo AP");
            startAccessPointMode();
        }
    }
}

void startAccessPointMode() {
    accessPointMode = true;
    Serial.println("Iniciando modo Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    
    Serial.printf("Modo AP iniciado com sucesso\n");
    Serial.printf("SSID: %s\n", apSSID);
    Serial.printf("Password: %s\n", apPassword);
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void setupWebServer() {
    // Rota principal para medicao
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("=== REQUISICAO DE MEDICAO RECEBIDA ===");
        Serial.printf("Cliente: %s\n", request->client()->remoteIP().toString().c_str());
        Serial.printf("User-Agent: %s\n", request->hasHeader("User-Agent") ? request->getHeader("User-Agent")->value().c_str() : "N/A");
        
        // Atualizar atividade
        lastActivity = millis();
        
        // Ligar sensor
        Serial.println("Ligando sensor...");
        digitalWrite(SENSOR_POWER_PIN, HIGH);
        delay(2000);
        
        // Inicializar UART do sensor
        sensorSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
        delay(500);
        Serial.println("UART do sensor inicializada");
        
        // Realizar medicoes
        float distance = readAverageDistance();
        
        // Desligar sensor
        sensorSerial.end();
        digitalWrite(SENSOR_POWER_PIN, LOW);
        Serial.println("Sensor desligado");
        
        // Calcular valores
        float volume = calculateVolumeLiters(distance);
        int percent = calculatePercent(distance);
        
        // Obter timestamp
        time_t now;
        time(&now);
        struct tm * timeinfo = gmtime(&now);
        char timestamp[30];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
        
        // Criar resposta JSON
        StaticJsonDocument<200> doc;
        doc["distance_cm"] = distance;
        doc["volume_liters"] = volume;
        doc["percent"] = percent;
        doc["timestamp"] = timestamp;
        doc["boot_count"] = bootCount;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["free_heap"] = ESP.getFreeHeap();
        
        String response;
        serializeJson(doc, response);
        
        Serial.printf("Resposta JSON: %s\n", response.c_str());
        request->send(200, "application/json", response);
        Serial.println("=== RESPOSTA ENVIADA ===");
    });
    
    // Pagina de configuracao WiFi (modo AP)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        lastActivity = millis();
        
        if (accessPointMode) {
            String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Configuracao WiFi - Water Tank</title>
    <meta charset="UTF-8">
</head>
<body>
    <h2>Configurar WiFi</h2>
    <form action="/setwifi" method="POST">
        <p>SSID: <input type="text" name="ssid" required></p>
        <p>Senha: <input type="password" name="password"></p>
        <p><input type="submit" value="Salvar"></p>
    </form>
    <hr>
    <p>Status: Modo AP ativo</p>
    <p>Boot: )" + String(bootCount) + R"(</p>
    <p>Memoria livre: )" + String(ESP.getFreeHeap()) + R"( bytes</p>
</body>
</html>
            )";
            request->send(200, "text/html", html);
        } else {
            String status = "Sistema funcionando. WiFi: " + WiFi.localIP().toString() + 
                          " (RSSI: " + String(WiFi.RSSI()) + " dBm)";
            request->send(200, "text/plain", status);
        }
    });
    
    // Salvar configuracoes WiFi
    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request){
        lastActivity = millis();
        
        if (accessPointMode && request->hasParam("ssid", true)) {
            String newSSID = request->getParam("ssid", true)->value();
            String newPassword = request->hasParam("password", true) ? 
                               request->getParam("password", true)->value() : "";
            
            Serial.printf("Salvando nova configuracao WiFi: %s\n", newSSID.c_str());
            
            preferences.putString("ssid", newSSID);
            preferences.putString("password", newPassword);
            
            request->send(200, "text/html", 
                         "<h2>Configuracoes salvas!</h2><p>Reiniciando em 3 segundos...</p>");
            
            Serial.println("Reiniciando para aplicar nova configuracao...");
            delay(3000);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Parametros invalidos");
        }
    });
    
    // Rota de debug
    server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
        lastActivity = millis();
        
        String debug = "=== DEBUG INFO ===\n";
        debug += "Boot Count: " + String(bootCount) + "\n";
        debug += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
        debug += "WiFi Status: " + getWiFiStatusString() + "\n";
        debug += "WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\n";
        debug += "IP: " + WiFi.localIP().toString() + "\n";
        debug += "Access Point Mode: " + String(accessPointMode ? "true" : "false") + "\n";
        debug += "Last Activity: " + String(millis() - lastActivity) + " ms ago\n";
        
        request->send(200, "text/plain", debug);
    });
    
    // Configurar OTA
    ElegantOTA.begin(&server);
    Serial.println("OTA configurado");
}

float readAverageDistance() {
    Serial.println("Iniciando leituras do sensor...");
    float sum = 0;
    int validReadings = 0;
    
    for (int i = 0; i < 5; i++) {
        Serial.printf("Leitura %d/5: ", i + 1);
        float distance = readSensor();
        
        if (distance > 0 && distance >= 18 && distance <= 94) {
            sum += distance;
            validReadings++;
            Serial.printf("%.2f cm (valida)\n", distance);
        } else {
            Serial.printf("%.2f cm (invalida - fora do range)\n", distance);
        }
        
        delay(200);
    }
    
    if (validReadings == 0) {
        Serial.println("ERRO: Nenhuma leitura valida obtida");
        return -1;
    }
    
    float average = sum / validReadings;
    Serial.printf("Distancia media: %.2f cm (%d leituras validas)\n", average, validReadings);
    return average;
}

float readSensor() {
    // Limpar buffer
    while (sensorSerial.available()) {
        sensorSerial.read();
    }
    
    // Comando para JSN-SR04T UART
    uint8_t cmd[4] = {0x55, 0xAA, 0x01, 0x00};
    sensorSerial.write(cmd, 4);
    
    // Aguardar resposta
    unsigned long start = millis();
    while (sensorSerial.available() < 4 && millis() - start < 300) {
        delay(10);
    }
    
    if (sensorSerial.available() >= 4) {
        uint8_t resp[4];
        sensorSerial.readBytes(resp, 4);
        
        // Verificar se resposta e valida
        if (resp[0] == 0xFF && resp[1] == 0xFF) {
            int distanceMM = (resp[2] << 8) | resp[3];
            return distanceMM / 10.0;
        } else {
            Serial.printf("Resposta invalida do sensor: %02X %02X %02X %02X\n", 
                         resp[0], resp[1], resp[2], resp[3]);
        }
    } else {
        Serial.printf("Timeout na leitura do sensor (disponivel: %d bytes)\n", 
                     sensorSerial.available());
    }
    
    return -1;
}

float calculateVolumeLiters(float distanceCM) {
    if (distanceCM < 0) return 0;
    
    float waterHeight = 94.0 - distanceCM;
    if (waterHeight < 0) waterHeight = 0;
    if (waterHeight > 76.0) waterHeight = 76.0;
    
    return waterHeight * 28.1616;
}

int calculatePercent(float distanceCM) {
    if (distanceCM < 0) return 0;
    
    float waterHeight = 94.0 - distanceCM;
    if (waterHeight < 0) waterHeight = 0;
    if (waterHeight > 76.0) waterHeight = 76.0;
    
    return (waterHeight / 76.0) * 100.0;
}

String getWiFiStatusString() {
    switch (WiFi.status()) {
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
        default: return "UNKNOWN";
    }
}