#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "app.h"
#include "recovery.h"

#define WIFI_SSID "GLS"
#define WIFI_PASS "Lola09876543*"

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
            "\"ip\":\"%u.%u.%u.%u\","
            "\"rssi\":%ld,"
            "\"uptime\":%lu"
            "}",
            ip[0], ip[1], ip[2], ip[3],
            (long)WiFi.RSSI(),
            (unsigned long)millis());

        server.send(
            200,
            "application/json",
            json); });

    recoveryOTARegister(server);

    server.begin();

    Serial.println("HTTP APP iniciado");
}

void appInit()
{
    Serial.println("Iniciando APP");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("Conectando em %s", WIFI_SSID);

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
