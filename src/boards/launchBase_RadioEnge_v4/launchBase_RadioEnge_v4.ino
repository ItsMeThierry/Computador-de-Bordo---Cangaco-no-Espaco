#include <Arduino.h>
#include "../../hardwareParts/loras/LoRa_RadioEnge_v4.h"
#include "../../config/Constants.h"

// Pinos da Serial2 do LoRa no ESP32 Master Base
constexpr int LORA_BASE_RX_PIN = PIN_LORA_RX;
constexpr int LORA_BASE_TX_PIN = PIN_LORA_TX;

LoRaRadioEnge_v4 lora(LORA_BASE_RX_PIN, LORA_BASE_TX_PIN);

void processCommandPC(String cmd);
void sendCommandLoRa(uint8_t comando);
void processReceivedPacket(uint8_t* buffer, uint8_t size);
void printHelp();

void setup() {
    Serial.begin(115200);
    delay(500);

    lora.begin(9600, false);

    Serial.println(F("================================================="));
    Serial.println(F("[BASE_V4] Estação Solo Radioenge v4 Iniciada."));
    Serial.println(F("[BASE_V4] Digite HELP para listar os comandos."));
    Serial.println(F("================================================="));
}

void loop() {
    // 1. Escuta pacotes RF do Foguete (Slave)
    uint16_t senderId = 0;
    uint8_t commandIn = 0;
    uint8_t rxBuffer[MAX_PAYLOAD_SIZE];
    uint8_t rxSize = 0;

    if (lora.receivePacket(&senderId, &commandIn, rxBuffer, &rxSize, 10)) {
        if (commandIn == LORA_APP_COMMAND_ID_V4 && rxSize > 0) {
            processReceivedPacket(rxBuffer, rxSize);
        }
    }

    // 2. Escuta comandos do Computador (CLI Serial)
    if (Serial.available() > 0) {
        String inputCmd = Serial.readStringUntil('\n');
        processCommandPC(inputCmd);
    }
}

void processCommandPC(String cmd) {
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "ARMAR") {
        Serial.println(F("[BASE_V4] Enviando comando: ARMAR FOGUETE..."));
        sendCommandLoRa(CMD_ARMAR_FOGUETE_V4);
    } 
    else if (cmd == "GPS") {
        Serial.println(F("[BASE_V4] Enviando comando: OBTER GPS..."));
        sendCommandLoRa(CMD_OBTER_GPS_V4);
    } 
    else if (cmd == "BAIXAR" || cmd == "SD") {
        Serial.println(F("[BASE_V4] Enviando comando: BAIXAR DADOS DO SD..."));
        sendCommandLoRa(CMD_BAIXAR_SD_V4);
    } 
    else if (cmd == "RESET_BASE_LINE" || cmd == "RESET") {
        Serial.println(F("[BASE_V4] Enviando comando: RESETAR BASELINE ALTITUDE..."));
        sendCommandLoRa(CMD_RESETAR_BASE_V4);
    } 
    else if (cmd == "READ" || cmd == "PING") {
        Serial.println(F("[BASE_V4] Solicitando telemetria..."));
        sendCommandLoRa(CMD_COLETAR_TELEMETRIA_V4);
    } 
    else if (cmd == "HELP") {
        printHelp();
    } 
    else if (cmd.length() > 0) {
        Serial.println(F("[BASE_V4] Comando desconhecido. Digite HELP."));
    }
}

void sendCommandLoRa(uint8_t comando) {
    uint8_t txBuffer[1];
    txBuffer[0] = comando;
    bool ok = lora.sendPacket(LORA_ROCKET_ID_V4, txBuffer, 1);

    if (ok) {
        Serial.println(F("[BASE_V4] Pacote enviado com sucesso."));
    } else {
        Serial.println(F("[BASE_V4] Falha ao enviar pacote."));
    }
}

void processReceivedPacket(uint8_t* buffer, uint8_t size) {
    uint8_t tipoPacote = buffer[0];

    switch (tipoPacote) {
        case CMD_TELEMETRIA_AUTO_V4: {
            uint8_t estado = buffer[1];
            int16_t altitude = (buffer[2] << 8) | buffer[3];

            Serial.print(F("[TELEMETRIA_V4] Estado: "));
            Serial.print(estado);
            Serial.print(F(" | Altitude: "));
            Serial.print(altitude);
            Serial.println(F(" m"));
            break;
        }

        case CMD_OBTER_GPS_V4: {
            if (size < 9) {
                Serial.println(F("[ERRO] Pacote GPS incompleto."));
                break;
            }

            int32_t lat32 = (buffer[1] << 24) | (buffer[2] << 16) | (buffer[3] << 8) | buffer[4];
            int32_t lon32 = (buffer[5] << 24) | (buffer[6] << 16) | (buffer[7] << 8) | buffer[8];

            float lat = lat32 / 1000000.0f;
            float lon = lon32 / 1000000.0f;

            Serial.print(F("[GPS_V4] Lat: "));
            Serial.print(lat, 6);
            Serial.print(F(" | Lon: "));
            Serial.println(lon, 6);
            break;
        }

        case CMD_BAIXAR_SD_V4: {
            Serial.print(F("[SD_DATA_V4] "));
            for (int i = 1; i < size; i++) {
                Serial.print((char)buffer[i]);
            }
            break;
        }

        case CMD_FIM_ARQUIVO_V4: {
            Serial.println(F("\n[BASE_V4] Download do SD concluído com sucesso."));
            break;
        }

        default:
            Serial.println(F("[BASE_V4] Pacote não reconhecido."));
            break;
    }
}

void printHelp() {
    Serial.println(F("\n--- Comandos Base Radioenge v4 ---"));
    Serial.println(F(" ARMAR  -> Força estado SUBIDA"));
    Serial.println(F(" GPS    -> Solicita coordenadas Lat/Lon"));
    Serial.println(F(" BAIXAR -> Inicia download do CSV do SD"));
    Serial.println(F(" RESET  -> Zera a baseline de altitude barométrica"));
    Serial.println(F(" READ   -> Solicita telemetria pontual"));
    Serial.println(F(" HELP   -> Lista comandos"));
}
