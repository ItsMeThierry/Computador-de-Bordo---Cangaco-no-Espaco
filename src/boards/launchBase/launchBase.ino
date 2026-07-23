#include <Arduino.h>
#include "../../hardwareParts/LoRaRadio.h"
#include "../../config/Constants.h"

LoRaRadio lora();

void processCommandPC(String cmd);
void sendCommandLoRa(uint8_t comando);
void processReceivedPacket(LoRaPacket* packet);
void printHelp();

void setup() {
    Serial.begin(115200);
    delay(500);

    lora.begin(9600);

    Serial.println(F("================================================="));
    Serial.println(F("[BASE] Estação Solo Radioenge Iniciada."));
    Serial.println(F("[BASE] Digite HELP para listar os comandos."));
    Serial.println(F("================================================="));
}

void loop() {
    // 1. Escuta pacotes RF do Foguete (Slave)
    LoRaPacket<> packet;

    if (lora.receivePacket(LORA_ROCKET_ID, &packet)) {
        processReceivedPacket(&packet);
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
        Serial.println(F("[BASE] Enviando comando: ARMAR FOGUETE..."));
        sendCommandLoRa(CMD_ARMAR_FOGUETE);
    } 
    else if (cmd == "GPS") {
        Serial.println(F("[BASE] Enviando comando: OBTER GPS..."));
        sendCommandLoRa(CMD_OBTER_GPS);
    } 
    else if (cmd == "RESET_BASE_LINE" || cmd == "RESET") {
        Serial.println(F("[BASE] Enviando comando: RESETAR BASELINE ALTITUDE..."));
        sendCommandLoRa(CMD_RESETAR_BASE);
    } 
    else if (cmd == "READ" || cmd == "PING") {
        Serial.println(F("[BASE] Solicitando telemetria..."));
        sendCommandLoRa(CMD_COLETAR_TELEMETRIA);
    } 
    else if (cmd == "HELP") {
        printHelp();
    } 
    else if (cmd.length() > 0) {
        Serial.println(F("[BASE] Comando desconhecido. Digite HELP."));
    }
}

void sendCommandLoRa(uint8_t comando) {
    LoRaPacket<1> packet;

    packet.append(command);
    
    bool ok = lora.sendPacket(LORA_ROCKET_ID, &packet);

    if (ok) {
        Serial.println(F("[BASE] Pacote enviado com sucesso."));
    } else {
        Serial.println(F("[BASE] Falha ao enviar pacote."));
    }
}

void processReceivedPacket(LoRaPacket* packet) {
    uint8_t tipoPacote = packet[0];

    switch (tipoPacote) {
        case CMD_TELEMETRIA_AUTO: {
            uint8_t estado = packet[1];
            int16_t altitude = (packet[2] << 8) | packet[3];

            Serial.print(F("[TELEMETRIA] Estado: "));
            Serial.print(estado);
            Serial.print(F(" | Altitude: "));
            Serial.print(altitude);
            Serial.println(F(" m"));
            break;
        }

        case CMD_OBTER_GPS: {
            if (size < 9) {
                Serial.println(F("[ERRO] Pacote GPS incompleto."));
                break;
            }

            int32_t lat32 = (packet[1] << 24) | (packet[2] << 16) | (packet[3] << 8) | packet[4];
            int32_t lon32 = (packet[5] << 24) | (packet[6] << 16) | (packet[7] << 8) | packet[8];

            float lat = lat32 / 1000000.0f;
            float lon = lon32 / 1000000.0f;

            Serial.print(F("[GPS] Lat: "));
            Serial.print(lat, 6);
            Serial.print(F(" | Lon: "));
            Serial.println(lon, 6);
            break;
        }

        default:
            Serial.println(F("[BASE] Pacote não reconhecido."));
            break;
    }
}

void printHelp() {
    Serial.println(F("\n--- Comandos Base Radioenge ---"));
    Serial.println(F(" ARMAR  -> Força estado SUBIDA"));
    Serial.println(F(" GPS    -> Solicita coordenadas Lat/Lon"));
    Serial.println(F(" RESET  -> Zera a baseline de altitude barométrica"));
    Serial.println(F(" READ   -> Solicita telemetria pontual"));
    Serial.println(F(" HELP   -> Lista comandos"));
}
