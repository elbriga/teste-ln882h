#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "app.h"
#include "recovery.h"

#define APP_WIFI_SSID "GLS"
#define APP_WIFI_PASS "Lola09876543*"

static WebServer server(80);

static void httpInit()
{
    server.on("/api/status", HTTP_GET, []()
              {
        IPAddress ip = WiFi.localIP();

        char json[192];

        snprintf(
            json,
            sizeof(json),
            "{"
            "\"mode\":\"app\","
            "\"ssid\":\"%s\","
            "\"ip\":\"%u.%u.%u.%u\","
            "\"rssi\":%ld,"
            "\"uptime\":%lu"
            "}",
            WiFi.SSID().c_str(),
            ip[0], ip[1], ip[2], ip[3],
            (long)WiFi.RSSI(),
            (unsigned long)millis());

        server.send(
            200,
            "application/json",
            json); });

    recoveryAPIRegister(server);

    server.begin();

    Serial.println("HTTP APP iniciado");
}

void appInit()
{
    Serial.println("Iniciando APP");

    WiFi.mode(WIFI_STA);
    WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASS);

    Serial.printf("Conectando em %s", APP_WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println();

    Serial.print("IP APP: ");
    Serial.println(WiFi.localIP());

    httpInit();
}

void appLoop()
{
    server.handleClient();
}
