/**
 * @file main.cpp
 * @author Eduardo Guse (eduautomatiza@yahoo.com.br)
 * @brief
 * @version 0.1
 * @date 2024-03-18
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define FILESYSTEM SPIFFS
#define DEFAULT_SSID "my-wifi-ssid"
#define DEFAULT_PASS "my-wifi-password"

void loadConfig(void);
void loadConfigValues(JsonVariant config);
void saveDefaultConfig(const char* filename, JsonVariant config);

void sendScan(void);
void getWifiScan(JsonVariant scan);
void sendFile(const char* file_name, const char* content_type);
void saveUploadConfigFile(const char* filename);

String ssid;
String password;
const char* const host = "esp32";
WebServer server(80);

void loop(void) {
  server.handleClient();
  yield();  // allow the cpu to switch to other tasks
}

void setup(void) {
  Serial.begin(115200);

  if (!FILESYSTEM.begin(true, "/spiffs")) {
    log_e("error, an Error has occurred while mounting SPIFFS");
  }

  loadConfig();

  // WIFI init
  {
    WiFi.onEvent([](WiFiEvent_t event) {
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP |
          event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
        if (WiFi.localIP() != IPAddress(0U)) {
          MDNS.begin(host);
          Serial.printf("\n\nConectado com \"%s\". \n\thttp://%s\n\thttp://%s.local\n\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), host);
        }
      } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("Tentando conectar com \"%s\"\n", ssid.c_str());
        WiFi.begin(ssid, password);
      } else if (event == ARDUINO_EVENT_WIFI_AP_START) {
        Serial.printf("\n\nConecte no WiFi \"%s\" usando a senha \"%s\"e acesse:\n\thttp://%s\n\n", DEFAULT_SSID, DEFAULT_PASS, WiFi.softAPIP().toString().c_str());
      }
    });

    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASS);
    Serial.printf("Tentando conectar com \"%s\"\n", ssid.c_str());
    WiFi.begin(ssid, password);
  }
  // Webserver init
  {
    server.on("/scan.json", HTTP_GET, sendScan);
    server.on("/config.json", HTTP_POST, []() { saveUploadConfigFile("/config.json"); });
    server.on("/config.json", HTTP_GET, []() { sendFile("/config.json", "application/json"); });
    server.on("/favicon.ico", HTTP_GET, []() { sendFile("/favicon.ico", "image/x-icon"); });
    server.onNotFound([]() { sendFile("/index.html", "text/html"); });
    server.begin();
  }
  log_v("HTTP server started");
}

void sendScan(void) {
  JsonDocument doc;
  String buffer;
  getWifiScan(doc);
  serializeJson(doc, buffer);
  server.send(200, "application/json", buffer);
}

void getWifiScan(JsonVariant scan) {
  JsonArray list = scan["scan"].to<JsonArray>();
  int n = 0;
  n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    JsonObject scan_item = list.add<JsonObject>();
    scan_item["ssid"] = WiFi.SSID(i);
    scan_item["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    scan_item["rssi"] = WiFi.RSSI(i);
    scan_item["bssid"] = WiFi.BSSIDstr(i);
    scan_item["channel"] = WiFi.channel(i);
  }
  WiFi.scanDelete();
}

void sendFile(const char* file_name, const char* content_type) {
  File file = FILESYSTEM.open(file_name, "r");
  if (!file.isDirectory()) {
    server.streamFile(file, content_type);
  } else {
    server.send(500, "text/plain", "Desculpe, erro ao acessar o recurso!");
  }
  file.close();
}

void saveUploadConfigFile(const char* filename) {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    auto content = server.arg("plain");
    auto error = deserializeJson(doc, content);
    if (!error) {
      File file = FILESYSTEM.open(filename, "w");
      file.print(content);
      file.close();
      loadConfigValues(doc);
      server.send(200);
    } else {
      server.send(400);
    }
  } else {
    server.send(404);
  }
}

void saveDefaultConfig(const char* filename, JsonVariant config) {
  JsonObject wifi = config["wifi"].to<JsonObject>();
  wifi["ssid"] = DEFAULT_SSID;
  wifi["psk"] = DEFAULT_PASS;

  File file = FILESYSTEM.open(filename, "w");
  serializeJson(config, file);
  file.close();
}

void loadConfigValues(JsonVariant config) {
  JsonObject wifi = config["wifi"];
  ssid = wifi["ssid"].as<String>();
  password = wifi["psk"].as<String>();
}

void loadConfig(void) {
  JsonDocument doc;
  const char* filename = "/config.json";

  File file = FILESYSTEM.open(filename, "r");
  auto error = deserializeJson(doc, file);
  file.close();

  if (error) {
    doc.clear();
    saveDefaultConfig(filename, doc);
  }
  loadConfigValues(doc);
}
