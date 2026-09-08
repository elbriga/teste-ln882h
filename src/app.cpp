#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "app.h"
#include "recovery.h"

#define APP_RELE_PIN PIN_PB03
#define APP_LED_PIN PIN_PB04

#define APP_WIFI_SSID "GLS"
#define APP_WIFI_PASS "Lola09876543*"

static WebServer server(80);

static bool releEstado = false;

static int ledUltimoDecimo = -1;
static int ledUltimoEstado = -1;
void ledLoop()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);

    int decimo = tv.tv_usec / 100000;
    if (decimo == ledUltimoDecimo)
        return;
    ledUltimoDecimo = decimo;

    // Sincronizado com o segundo!
    bool estado = decimo < 2;

    if (estado != ledUltimoEstado)
    {
        ledUltimoEstado = estado;
        digitalWrite(APP_LED_PIN, !estado);
    }
}

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

    server.on("/api/toggle", HTTP_GET, []()
              {
            
                releEstado = !releEstado;

                digitalWrite(APP_RELE_PIN, releEstado);
            
                server.send(
                    200,
                    "application/json",
                    "{\"msg\":\"OK\"}"); });

    recoveryAPIRegister(server);

    server.begin();

    Serial.println("HTTP APP iniciado");
}

void appInit()
{
    Serial.println("Iniciando APP");

    pinMode(APP_LED_PIN, OUTPUT);
    digitalWrite(APP_LED_PIN, LOW);

    pinMode(APP_RELE_PIN, OUTPUT);
    digitalWrite(APP_RELE_PIN, releEstado);

    WiFi.mode(WIFI_STA);
    WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASS);

    Serial.printf("Conectando em %s", APP_WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(50);
    }

    Serial.println();

    Serial.print("IP APP: ");
    Serial.println(WiFi.localIP());

    httpInit();
}

void appLoop()
{
    server.handleClient();

    ledLoop();
}
