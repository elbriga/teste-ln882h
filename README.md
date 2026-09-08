# eTomada LN882H — Firmware mínimo com Recovery

Firmware mínimo para o **LN882H / LibreTiny** com uma regra simples de arquitetura:

> **`main.cpp` não deve mudar.**

Toda a aplicação normal fica em `app.cpp`, enquanto a infraestrutura de recuperação fica em `recovery.cpp`.

O objetivo é manter um ponto de entrada pequeno, previsível e difícil de quebrar durante a evolução do firmware. Antes de iniciar a aplicação, o `main.cpp` sempre executa a lógica de recovery e permite entrar em modo de recuperação através de **3 power cycles consecutivos**.

---

## Objetivo

O projeto foi organizado para que o fluxo principal permaneça sempre assim:

```text
BOOT
 |
 v
recoveryBoot()
 |
 +---- recovery? ---- sim ---> recoveryInit()
 |                            recoveryLoop()
 |
 não
 |
 v
appInit()
 |
 v
recoveryBootTick()
 |
 v
appLoop()
```

O `main.cpp` só faz a seleção entre:

- **modo normal**, executado por `app.cpp`;
- **modo recovery**, executado por `recovery.cpp`.

A ideia é que novas funcionalidades sejam adicionadas à aplicação sem alterar a lógica básica de boot.

---

## Estrutura

```text
include/
├── app.h
└── recovery.h

src/
├── main.cpp
├── app.cpp
└── recovery.cpp

platformio.ini
```

### `main.cpp`

É o ponto de entrada fixo do firmware.

Sua responsabilidade é somente:

1. inicializar a serial;
2. chamar `recoveryBoot()`;
3. entrar no recovery quando necessário;
4. iniciar a aplicação normal;
5. chamar periodicamente `recoveryBootTick()`;
6. executar `appLoop()`.

O contrato do projeto é:

> alterações da aplicação devem ser feitas em `app.cpp`, e não em `main.cpp`.

Isso reduz a chance de uma alteração comum da aplicação remover acidentalmente o mecanismo de recuperação.

---

## Como funciona o recovery por 3 power cycles

O recovery utiliza uma pequena área da flash para registrar os boots.

Configuração atual:

```cpp
#define RECOVERY_BOOT_COUNT   3
#define RECOVERY_BOOT_TIMEOUT 10000
```

Portanto:

- são necessários **3 boots consecutivos não confirmados**;
- um boot é considerado saudável depois de aproximadamente **10 segundos**.

### Boot normal

Ao ligar:

1. `recoveryBoot()` lê o histórico;
2. grava um registro `BOOT`;
3. inicia a aplicação;
4. após 10 segundos, `recoveryBootTick()` grava um registro `OK`.

O registro `OK` encerra a sequência de power cycles.

```text
BOOT
 |
 +---- 10 segundos funcionando ----> OK
```

Depois disso, um novo boot começa novamente do contador zero.

---

## Como entrar no modo recovery

Para forçar o recovery:

1. ligue o dispositivo;
2. desligue antes de completar 10 segundos;
3. ligue novamente;
4. desligue novamente antes dos 10 segundos;
5. ligue pela terceira vez.

Exemplo:

```text
Power ON
  BOOT 1
Power OFF antes de 10 s

Power ON
  BOOT 2
Power OFF antes de 10 s

Power ON
  BOOT 3
  -> RECOVERY
```

No terceiro boot consecutivo sem confirmação, `recoveryBoot()` retorna `true` e o `main.cpp` inicia:

```cpp
recoveryInit();
```

A partir daí, o `loop()` executa somente:

```cpp
recoveryLoop();
```

---

## Confirmação de boot saudável

O `main.cpp` chama continuamente:

```cpp
recoveryBootTick();
```

Quando o firmware permanece funcionando por pelo menos `RECOVERY_BOOT_TIMEOUT`, o recovery grava:

```text
OK
```

Isso significa que o boot foi considerado saudável e a sequência é zerada logicamente.

O registro só deixa de ficar pendente depois que a gravação do `OK` é concluída com sucesso.

---

## Storage do recovery

O contador não é armazenado regravando continuamente uma mesma variável.

O recovery utiliza um pequeno **log append-only** na flash.

Área atual:

```cpp
#define RECOVERY_FLASH_ADDR 0x1FF000
#define RECOVERY_FLASH_SIZE 0x1000
```

São utilizados dois valores de 32 bits:

```cpp
#define RECOVERY_REC_BOOT 0x424F4F54 // "BOOT"
#define RECOVERY_REC_OK   0x4F4B4F4B // "OKOK"
```

Exemplo de conteúdo:

```text
BOOT
OK
BOOT
OK
BOOT
BOOT
BOOT
OK
```

Durante o scan:

- `BOOT` incrementa o contador;
- `OK` zera o contador;
- `0xFFFFFFFF` indica o próximo slot vazio.

Essa estratégia evita apagar um setor inteiro a cada boot.

O setor só é apagado quando:

- está cheio; ou
- contém dados inválidos/corrompidos.

---

## Proteção do layout de flash

O endereço usado pelo recovery foi escolhido especificamente para o layout atual de **2 MiB**.

Antes de usar o storage, o código verifica:

```cpp
Flash.getSize() == 0x200000
```

Se o tamanho da flash for diferente, o mecanismo de contador é desativado.

Isso evita escrever cegamente em `0x1FF000` em um dispositivo com layout diferente.

> Antes de mudar placa, tamanho de flash ou particionamento, revise `RECOVERY_FLASH_ADDR`.

---

## Modo recovery

Quando ativado, o recovery inicializa:

- LED de indicação;
- Wi-Fi;
- servidor HTTP;
- API de OTA.

Primeiro tenta conectar como estação à rede configurada.

Se não conseguir dentro do timeout:

```cpp
#define RECOVERY_WIFI_TIMEOUT_MS 15000
```

é criado um Access Point com prefixo:

```text
eTomada-Recovery-
```

seguido pelos últimos caracteres do MAC.

Assim, mesmo que a aplicação normal esteja quebrada ou o Wi-Fi configurado nela não funcione, ainda existe uma forma de acessar o firmware de recuperação.

---

## API do recovery

### Status

```http
GET /api/status
```

Exemplo de resposta:

```json
{
  "mode": "recovery",
  "ssid": "eTomada-Recovery-1234",
  "ip": "192.168.4.1",
  "rssi": 0,
  "uptime": 12345
}
```

---

### OTA

```http
POST /api/ota?tamanho=<bytes>
```

O arquivo é recebido usando `multipart/form-data`.

Exemplo:

```bash
curl \
  -F "file=@firmware.uf2" \
  "http://IP_DO_DISPOSITIVO/api/ota?tamanho=$(stat -c%s firmware.uf2)"
```

O recovery verifica:

- se `tamanho` foi informado;
- se o tamanho é múltiplo de 512 bytes;
- se `Update.begin()` foi iniciado;
- se todos os blocos foram escritos;
- se `Update.end()` terminou corretamente.

O reboot após o upload é manual.

---

### Reboot

```http
POST /api/reboot
```

Exemplo:

```bash
curl -X POST http://IP_DO_DISPOSITIVO/api/reboot
```

O endpoint responde ao cliente e depois executa:

```cpp
lt_reboot();
```

---

## Aplicação normal

A aplicação fica isolada em:

```text
app.cpp
```

Ela implementa:

```cpp
void appInit();
void appLoop();
```

No firmware de exemplo, a aplicação:

- conecta ao Wi-Fi;
- inicia um servidor HTTP;
- controla um relé;
- pisca o LED;
- disponibiliza `/api/status`;
- disponibiliza `/api/toggle`;
- também registra a API de OTA através de `recoveryAPIRegister()`.

Isso permite reutilizar a mesma implementação de OTA tanto no modo normal quanto no recovery.

---

## Regra para evolução do projeto

A organização esperada é:

```text
main.cpp
   |
   +-- recovery.cpp   infraestrutura de recuperação
   |
   +-- app.cpp        aplicação que pode evoluir livremente
```

Ao adicionar sensores, relés, MQTT, regras, interface web ou qualquer outra funcionalidade:

**edite `app.cpp` ou módulos chamados por ele.**

Evite adicionar lógica da aplicação diretamente ao `main.cpp`.

O `main.cpp` deve continuar sendo apenas o pequeno supervisor responsável por decidir:

```text
APP ou RECOVERY?
```

---

## `main.cpp` de referência

O ponto de entrada deve permanecer essencialmente assim:

```cpp
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

    recoveryBootTick();

    appLoop();

    yield();
}
```

Esse arquivo é o núcleo da arquitetura e deve permanecer simples.

---

## PlatformIO

Configuração mínima atual:

```ini
[env:generic-ln882h]
platform = libretiny
board = generic-ln882h
framework = arduino
```

Compilar:

```bash
pio run
```

Gravar pela interface configurada no projeto:

```bash
pio run -t upload
```

---

## Limitações importantes

Este projeto implementa um **recovery lógico dentro do mesmo firmware**.

Ele não é, por si só, um bootloader independente nem uma partição de recovery fisicamente protegida.

Isso significa que uma atualização OTA instala uma nova versão do firmware que também contém `main.cpp` e `recovery.cpp`.

A proteção desta arquitetura vem principalmente do contrato de desenvolvimento:

- manter `main.cpp` estável;
- manter `recovery.cpp` pequeno e conservador;
- alterar a aplicação principalmente através de `app.cpp` e seus módulos;
- testar alterações no recovery com mais cuidado do que alterações comuns da aplicação.

Para um recovery realmente imutável seria necessária uma arquitetura diferente, com bootloader ou partição separada e protegida.

---

## Resumo

A proposta deste projeto é simples:

```text
main.cpp não muda
        |
        v
todo boot passa pelo recoveryBoot()
        |
        +---- 3 boots interrompidos ----> RECOVERY
        |
        +---- boot saudável -----------> APP
```

Com isso, o firmware normal pode evoluir mantendo sempre um caminho previsível para recuperação através de **3 power cycles**.
