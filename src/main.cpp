#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
// #include <ESP.h>

#define WIFI_SSID "GLS"
#define WIFI_PASS "Lola09876543*"

WebServer server(80);

static bool otaOK = false;
static bool reiniciar = false;
static uint32_t reiniciarEm = 0;

static void otaUpload()
{
    HTTPUpload &upload = server.upload();

    switch (upload.status)
    {
    case UPLOAD_FILE_START:
    {
        otaOK = false;

        size_t tamanho = server.arg("tamanho").toInt();

        Serial.printf("OTA inicio: %s (%u bytes)\n",
                      upload.filename.c_str(),
                      tamanho);

        if (!tamanho)
        {
            Serial.println("Tamanho invalido");
            return;
        }

        if (!Update.begin(tamanho))
        {
            Serial.printf("Update.begin erro: %s\n",
                          Update.errorString());
            return;
        }

        break;
    }

    case UPLOAD_FILE_WRITE:
        if (!Update.isRunning())
            return;

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            Serial.printf("Update.write erro: %s\n",
                          Update.errorString());
        }

        break;

    case UPLOAD_FILE_END:
        if (!Update.isRunning())
            return;

        if (!Update.end())
        {
            Serial.printf("Update.end erro: %s\n",
                          Update.errorString());
            return;
        }

        Serial.printf("OTA concluido: %u bytes\n",
                      upload.totalSize);

        otaOK = true;
        break;

    case UPLOAD_FILE_ABORTED:
        Serial.println("OTA abortado");

        if (Update.isRunning())
            Update.abort();

        break;
    }
}

static void httpInit()
{
    server.on("/api/status", HTTP_GET, []()
              {
        String json =
            String("{\"mode\":\"recovery\",") +
            "\"ip\":\"" + WiFi.localIP() + "\"," +
            "\"rssi\":" + WiFi.RSSI() + "," +
            "\"uptime\":" + millis() +
            "}";

        server.send(200, "application/json", json); });

    server.on(
        "/api/ota",
        HTTP_POST,

        []()
        {
            if (!otaOK)
            {
                server.send(
                    500,
                    "application/json",
                    String("{\"msg\":\"OTA falhou\",\"erro\":\"") +
                        Update.errorString() +
                        "\"}");

                return;
            }

            server.send(
                200,
                "application/json",
                "{\"msg\":\"OK\",\"reboot\":true}");

            // não reinicia dentro do handler:
            // deixa a resposta HTTP sair primeiro
            reiniciar = true;
            reiniciarEm = millis() + 1000;
        },

        otaUpload);

    server.on("/api/reboot", HTTP_POST, []()
              {
        server.send(
            200,
            "application/json",
            "{\"msg\":\"reboot\"}");

        reiniciar = true;
        reiniciarEm = millis() + 1000; });

    server.begin();

    Serial.println("HTTP iniciado");
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("=== eTomada Recovery LN882H ===");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("Conectando em %s", WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println();
    Serial.printf("IP: ");
    Serial.println(WiFi.localIP());

    httpInit();
}

void loop()
{
    server.handleClient();

    if (reiniciar && (int32_t)(millis() - reiniciarEm) >= 0)
    {
        Serial.println("Reiniciando...");
        delay(50);
        ESP.restart();
    }
}
