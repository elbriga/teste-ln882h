#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <libretiny.h>

#define WIFI_SSID "GLS"
#define WIFI_PASS "Lola09876543*"

#define AP_SSID "eTomada-Recovery"
#define AP_PASS "09876543"

#define WIFI_TIMEOUT_MS 15000
static bool modoAP = false;

WebServer server(80);

static bool otaOK = false;
static String otaErro;

static bool reiniciar = false;
static uint32_t reiniciarEm = 0;

static void wifiInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("Conectando em %s", WIFI_SSID);

    uint32_t inicio = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - inicio < WIFI_TIMEOUT_MS)
    {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("WiFi conectado. IP: ");
        Serial.println(WiFi.localIP());
        return;
    }

    Serial.println();
    Serial.println("Falha ao conectar no WiFi");
    Serial.println("Iniciando AP de recovery...");

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAP(AP_SSID, AP_PASS))
    {
        Serial.println("ERRO iniciando AP");
        return;
    }

    modoAP = true;

    Serial.printf("AP: %s\n", AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
}

static void otaUpload()
{
    HTTPUpload &upload = server.upload();

    switch (upload.status)
    {
    case UPLOAD_FILE_START:
    {
        otaOK = false;
        otaErro = "";

        size_t tamanho = server.arg("tamanho").toInt();

        Serial.printf(
            "OTA inicio: %s (%u bytes)\n",
            upload.filename.c_str(),
            tamanho);

        if (!tamanho)
        {
            otaErro = "Tamanho invalido";
            Serial.println(otaErro);
            return;
        }

        // UF2 é formado por blocos de 512 bytes.
        if (tamanho % 512)
        {
            otaErro = "Tamanho nao e multiplo de 512 (arquivo nao parece UF2)";
            Serial.println(otaErro);
            return;
        }

        if (!Update.begin(tamanho, U_FLASH))
        {
            otaErro = Update.errorString();

            Serial.printf(
                "Update.begin erro: %s\n",
                otaErro.c_str());

            return;
        }

        break;
    }

    case UPLOAD_FILE_WRITE:
    {
        if (!Update.isRunning())
            return;

        size_t escrito = Update.write(
            upload.buf,
            upload.currentSize);

        if (escrito != upload.currentSize)
        {
            otaErro = Update.errorString();

            Serial.printf(
                "Update.write erro: %s (%u/%u)\n",
                otaErro.c_str(),
                escrito,
                upload.currentSize);
        }

        break;
    }

    case UPLOAD_FILE_END:
    {
        if (!Update.isRunning())
        {
            if (!otaErro.length())
                otaErro = "Update nao esta em execucao";

            return;
        }

        if (!Update.end())
        {
            otaErro = Update.errorString();

            Serial.printf(
                "Update.end erro: %s\n",
                otaErro.c_str());

            return;
        }

        Serial.printf(
            "OTA concluido: %u bytes\n",
            upload.totalSize);

        otaOK = true;
        break;
    }

    case UPLOAD_FILE_ABORTED:
    {
        otaErro = "Upload abortado";

        Serial.println(otaErro);

        if (Update.isRunning())
            Update.abort();

        break;
    }
    }
}

static void httpInit()
{
    server.on("/api/status", HTTP_GET, []()
              {
    IPAddress ip = modoAP
                       ? WiFi.softAPIP()
                       : WiFi.localIP();

    char json[180];

    snprintf(
        json,
        sizeof(json),
        "{"
        "\"mode\":\"recovery\","
        "\"wifi_mode\":\"%s\","
        "\"ip\":\"%u.%u.%u.%u\","
        "\"rssi\":%ld,"
        "\"uptime\":%lu"
        "}",
        modoAP ? "ap" : "sta",
        ip[0], ip[1], ip[2], ip[3],
        modoAP ? 0L : (long)WiFi.RSSI(),
        (unsigned long)millis());

    server.send(
        200,
        "application/json",
        json); });

    server.on(
        "/api/ota",
        HTTP_POST,

        []()
        {
            if (!otaOK)
            {
                String erro = otaErro;

                if (!erro.length())
                    erro = Update.errorString();

                server.send(
                    500,
                    "application/json",
                    String("{\"msg\":\"OTA falhou\",\"erro\":\"") +
                        erro +
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

    wifiInit();

    httpInit();
}

void loop()
{
    server.handleClient();

    if (reiniciar && (int32_t)(millis() - reiniciarEm) >= 0)
    {
        Serial.println("Reiniciando...");
        Serial.flush();
        delay(50);

        lt_reboot();
    }
}
