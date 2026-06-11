#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"

// Configuración de red
const char* ssid = "Telemetria_Cohete";
const char* pass = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ============================================================
// ESTRUCTURA DE DATOS (debe coincidir con el transmisor)
// ============================================================
typedef struct struct_telemetria {
    float temp;
    float press;
    float alt;
    float ax, ay, az;
    float gx, gy, gz;
    double lat, lon;
    int sats;
} struct_telemetria;

struct_telemetria datosCohete; // Instancia global para almacenar lo recibido


// ============================================================
// CALLBACK DE RECEPCIÓN
// ============================================================
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {

  // Copiar datos recibidos a la estructura
  memcpy(&datosCohete, incomingData, sizeof(datosCohete));

  // Mostrar MAC del emisor
  Serial.print("MAC del emisor: ");
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();


  // Mostrar contenido
  Serial.print("Temperatura: ");
  Serial.println(datosCohete.temp);
  Serial.print("Presion: ");
  Serial.println(datosCohete.press);
  Serial.print("Altura: ");
  Serial.println(datosCohete.alt);
  //Serial.print("Aceleracion (X): ");
  //Serial.println(datosCohete.ax);
  //Serial.print("Aceleracion (Y): ");
  //Serial.println(datosCohete.ay);
  //Serial.print("Aceleracion (Z): ");
  //Serial.println(datosCohete.az);
  //Serial.print("Giroscopio (X): ");
  //Serial.println(datosCohete.gx);
  //Serial.print("Giroscopio (Y): ");
  //Serial.println(datosCohete.gy);
  //Serial.print("Giroscopio (Z): ");
  //Serial.println(datosCohete.gz);
  Serial.print("GPS (Latitud): ");
  Serial.println(datosCohete.lat);
  Serial.print("GPS (Longitud): ");
  Serial.println(datosCohete.lon);
  Serial.print("GPS (Numero de satelites): ");
  Serial.println(datosCohete.sats);
  Serial.println();
}

void setup() {
    Serial.begin(115200);


    // 1. Inicializar Almacenamiento (Archivos HTML/JS)
    if(!SPIFFS.begin(true)){ Serial.println("Error SPIFFS"); return; }

    // 2. Configurar WiFi en modo Dual (AP + Estación para ESP-NOW)
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, pass);
    IPAddress IP = WiFi.softAPIP();
    Serial.println("Servidor creado correctamente!");
    Serial.print("Conéctate a la red: ");
    Serial.println(ssid);
    Serial.print("IP del servidor: ");
    Serial.println(IP); // Normalmente será 192.168.4.1
    // Mostrar MAC del receptor
    Serial.print("MAC del receptor: ");
    Serial.println(WiFi.macAddress());

    // 3. Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);
    Serial.println("Receptor listo, esperando datos...");

    // 4. Configurar Servidor Web Asíncrono
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.serveStatic("/", SPIFFS, "/");
    server.begin();
}

void loop() {
    ws.cleanupClients(); // Limpiar conexiones muertas [8, 9]

    // Enviar datos al Dashboard cada 200ms (5Hz) [9, 10]
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 500) {
        lastTime = millis();

        //StaticJsonDocument<200> doc;
        //doc["temp"] = random(20, 30);
        //doc["acel"] = random(0, 500) / 100.0;
        //doc["alt"]  = random(0, 1000);
        //doc["baro"] = random(950, 1050);
        StaticJsonDocument<500> doc;
        doc["temp"] = datosCohete.temp;
        doc["baro"] = datosCohete.press;
        doc["alt"]  = datosCohete.alt;
        doc["ax"]   = datosCohete.ax;
        doc["ay"]   = datosCohete.ay;
        doc["az"]   = datosCohete.az;
        doc["gx"]   = datosCohete.gx;
        doc["gy"]   = datosCohete.gy;
        doc["gz"]   = datosCohete.gz;
        doc["gps_lat"] = datosCohete.lat;
        doc["gps_lon"] = datosCohete.lon;
        doc["gps_sat"] = datosCohete.sats;

        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString); // Difusión masiva a clientes conectados [9, 11]
    }
}
