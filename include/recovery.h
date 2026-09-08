#pragma once

#include <WebServer.h>

bool recoveryBoot();
void recoveryBootTick();

void recoveryInit();
void recoveryLoop();

void recoveryAPIRegister(WebServer &server);
