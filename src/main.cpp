#include <Arduino.h>

#include "recovery.h"
#include "app.h"

static bool modoRecovery = false;

void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("=== eTomada LN882H ===");

    modoRecovery = recoveryBoot();
    if (modoRecovery)
    {
        recoveryInit();
        return;
    }

    appInit();
}

void loop()
{
    if (modoRecovery)
    {
        recoveryLoop();
        return;
    }
    recoveryBootTick(); // Marca o boot como saudavel apos 10 segundos

    appLoop();

    yield();
}
