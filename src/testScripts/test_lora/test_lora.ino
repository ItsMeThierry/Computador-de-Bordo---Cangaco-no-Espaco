#include "../../hardwareParts/LoRaRadio.h"

LoRaRadio lora();

void setup() {
    Serial.begin(115200);
    delay(1000);

    lora.begin(9600);
}

void loop() {
    LoRaPacket<> packet;

    if (_radio->receivePacket(LORA_BASE_ID, &packet)) {
        uint8_t subCommand = packet[0];
        _handleCommand(subCommand);
    }
}

void _handleCommand(uint8_t command) {
    switch (command) {
        case CMD_ARMAR_FOGUETE: {
            Serial.println(F("[RF4_CMD] Foguete armado -> Forçado estado SUBIDA"));
            break;
        }

        case CMD_RESETAR_BASE: {
            Serial.println(F("[RF4_CMD] Baseline do altímetro resetada"));
            break;
        }

        case CMD_OBTER_GPS: {
            Serial.println(F("[RF4_CMD] Enviando GPS"));
            _sendGPS();
            break;
        }

        case CMD_COLETAR_TELEMETRIA: {
            Serial.println(F("[RF4_CMD] Coletando Telemetria"));
            _sendTelemetry();
            break;
        }

        default:
            Serial.print(F("[RF4_CMD] Comando desconhecido: 0x"));
            Serial.println(command, HEX);
            break;
    }
}

void _sendGPS() {
    LoRaPacket<9> packet;

    packet.append(CMD_OBTER_GPS);

    packet.append(67);
    packet.append(76);

    _radio->sendPacket(LORA_BASE_ID, &packet);
}

void _sendTelemetry() {
    LoRaPacket<6> packet;

    packet.append(CMD_TELEMETRIA_AUTO);
    packet.append(1);
    packet.append(1000);

    _radio->sendPacket(LORA_BASE_ID, &packet);
}