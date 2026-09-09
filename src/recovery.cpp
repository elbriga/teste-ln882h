#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Flash.h>

// WIFI
#define RECOVERY_WIFI_SSID "GLS"
#define RECOVERY_WIFI_PASS "Lola09876543*"

#define RECOVERY_AP_SSID_PREFIX "eTomada-Recovery-"
#define RECOVERY_AP_PASS "09876543"

#define RECOVERY_WIFI_TIMEOUT_MS 15000

// "EPROM"
#define RECOVERY_FLASH_ADDR 0x1FF000
#define RECOVERY_FLASH_SIZE 0x1000

#define RECOVERY_BOOT_COUNT 3
#define RECOVERY_BOOT_TIMEOUT 10000

#define RECOVERY_REC_BOOT 0x424F4F54 // "BOOT"
#define RECOVERY_REC_OK 0x4F4B4F4B   // "OKOK"

// LED
#define RECOVERY_LED_PIN PIN_PB04

static String apSSID;

static bool bootAguardandoOK = false;
static uint32_t bootInicio = 0;

static bool modoAP = false;

static WebServer server(80);

static bool otaOK = false;
static String otaErro;

static bool ledUltimoEstado = false;
void recoveryLedLoop()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);

    // Sincronizado com o segundo para piscarem juntos quando com NTP!
    bool estado = (tv.tv_usec / 100000) % 2;

    if (ledUltimoEstado != estado)
    {
        ledUltimoEstado = estado;
        digitalWrite(RECOVERY_LED_PIN, estado);
    }
}

static void wifiInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(RECOVERY_WIFI_SSID, RECOVERY_WIFI_PASS);

    Serial.printf("Conectando em %s", RECOVERY_WIFI_SSID);

    uint32_t inicio = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - inicio < RECOVERY_WIFI_TIMEOUT_MS)
    {
        recoveryLedLoop();

        Serial.print(".");
        delay(50);
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

    String mac = WiFi.macAddress();
    mac.replace(":", "");

    apSSID = String(RECOVERY_AP_SSID_PREFIX) + mac.substring(mac.length() - 4);

    if (!WiFi.softAP(apSSID.c_str(), RECOVERY_AP_PASS))
    {
        Serial.println("ERRO iniciando AP");
        return;
    }

    modoAP = true;

    Serial.printf("AP: %s\n", apSSID.c_str());
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
}

static void otaUpload(WebServer &server)
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

void recoveryAPIRegister(WebServer &server)
{
    server.on(
        "/api/ota",
        HTTP_POST,

        [&server]()
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
                "{\"msg\":\"OK\",\"reboot\":false}");

            otaOK = false;
        },

        [&server]()
        {
            otaUpload(server);
        });

    server.on("/api/reboot", HTTP_POST, [&server]()
              {
        server.send(
            200,
            "application/json",
            "{\"msg\":\"reboot\"}");

        delay(1000);
        Serial.flush();

        lt_reboot(); });
}

static void httpInit()
{
    server.on("/api/status", HTTP_GET, []()
              {
    IPAddress ip = modoAP
                       ? WiFi.softAPIP()
                       : WiFi.localIP();
    String ssid = modoAP
                       ? apSSID
                       : WiFi.SSID();
    char json[180];

    snprintf(
        json,
        sizeof(json),
        "{"
        "\"mode\":\"recovery\","
        "\"ssid\":\"%s\","
        "\"ip\":\"%u.%u.%u.%u\","
        "\"rssi\":%ld,"
        "\"uptime\":%lu"
        "}",
        ssid.c_str(),
        ip[0], ip[1], ip[2], ip[3],
        modoAP ? 0L : (long)WiFi.RSSI(),
        (unsigned long)millis());

    server.send(
        200,
        "application/json",
        json); });

    recoveryAPIRegister(server);

    server.begin();

    Serial.println("HTTP iniciado");
}

static bool recoveryStorageScan(uint32_t &proximoOffset, uint8_t &boots)
{
    boots = 0;

    for (uint32_t offset = 0; offset < RECOVERY_FLASH_SIZE; offset += sizeof(uint32_t))
    {
        uint32_t valor;

        if (!Flash.readBlock(
                RECOVERY_FLASH_ADDR + offset,
                (uint8_t *)&valor,
                sizeof(valor)))
        {
            return false;
        }

        // Primeiro slot vazio
        if (valor == 0xFFFFFFFF)
        {
            proximoOffset = offset;
            return true;
        }

        if (valor == RECOVERY_REC_BOOT)
        {
            if (boots < 255)
                boots++;

            continue;
        }

        if (valor == RECOVERY_REC_OK)
        {
            boots = 0;
            continue;
        }

        // Conteúdo desconhecido/corrompido
        return false;
    }

    // Setor cheio
    proximoOffset = RECOVERY_FLASH_SIZE;

    return true;
}

static bool recoveryStorageAppend(uint32_t valor)
{
    uint32_t offset;
    uint8_t boots;

    if (!recoveryStorageScan(offset, boots))
    {
        Serial.println("Recovery storage invalido, apagando");

        if (!Flash.eraseSector(RECOVERY_FLASH_ADDR))
            return false;

        offset = 0;
    }

    if (offset >= RECOVERY_FLASH_SIZE)
    {
        Serial.println("Recovery storage cheio, apagando");

        if (!Flash.eraseSector(RECOVERY_FLASH_ADDR))
            return false;

        offset = 0;
    }

    return Flash.writeBlock(
        RECOVERY_FLASH_ADDR + offset,
        (const uint8_t *)&valor,
        sizeof(valor));
}

bool recoveryBoot()
{
    // Segurança: este layout é especificamente para flash de 2 MiB
    if (Flash.getSize() != 0x200000)
    {
        Serial.printf(
            "Flash inesperada: %u bytes - recovery boot desativado\n",
            Flash.getSize());

        return false;
    }

    uint32_t offset;
    uint8_t boots;

    if (!recoveryStorageScan(offset, boots))
    {
        Serial.println("Inicializando recovery storage");

        if (!Flash.eraseSector(RECOVERY_FLASH_ADDR))
        {
            Serial.println("Erro apagando recovery storage");
            return false;
        }

        boots = 0;
    }

    boots++;

    Serial.printf(
        "Recovery boot: %u/%u\n",
        boots,
        RECOVERY_BOOT_COUNT);

    if (!recoveryStorageAppend(RECOVERY_REC_BOOT))
    {
        Serial.println("Erro gravando contador de recovery");
        return false;
    }

    if (boots >= RECOVERY_BOOT_COUNT)
    {
        Serial.println("Entrando em modo recovery");

        // Considera a sequência encerrada.
        // Assim, depois de um OTA/reboot, volta ao app normalmente.
        recoveryStorageAppend(RECOVERY_REC_OK);

        return true;
    }

    bootAguardandoOK = true;
    bootInicio = millis();

    return false;
}

void recoveryBootTick()
{
    if (!bootAguardandoOK)
        return;

    if (millis() - bootInicio < RECOVERY_BOOT_TIMEOUT)
        return;

    if (recoveryStorageAppend(RECOVERY_REC_OK))
    {
        Serial.println("Boot confirmado");
        bootAguardandoOK = false;
    }
}

void recoveryInit()
{
    Serial.println();
    Serial.println("=== eTomada Recovery LN882H ===");

    pinMode(RECOVERY_LED_PIN, OUTPUT);

    wifiInit();

    httpInit();
}

void recoveryLoop()
{
    server.handleClient();

    recoveryLedLoop();
}
