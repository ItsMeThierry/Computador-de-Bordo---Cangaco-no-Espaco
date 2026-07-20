#include "../../hardwareParts/loras/LoRa_EBYTE.h"
#include "../../config/Constants.h"

LoRaRadio lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1, PIN_LORA_AUX);

void handleCommand(String cmd);
void sendCommand(LoRaCommandType cmd);
void printHelp();

void setup() {
    Serial.begin(115200);
    delay(500);
    lora.begin(LORA_UART_BAUD);

    Serial.println("Iniciando Base de Lancamento (Digite HELP para listar comandos).");
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readString();
        handleCommand(input);
    }

    LoRaResponse response = lora.receive();
    if (response.resp != RESP_ERROR) {
        Serial.print(F("Resposta LoRa: "));
        Serial.println(response.resp);
    }
}

void handleCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "READ") sendCommand(CMD_READ);
    else if (cmd == "PING") sendCommand(CMD_PING);
    else if (cmd == "ARMAR") sendCommand(CMD_ARMAR);
    else if (cmd == "RESET") sendCommand(CMD_RESET_BASE);
    else if (cmd == "GPS") sendCommand(CMD_GET_GPS);
    else if (cmd == "SD") sendCommand(CMD_DOWNLOAD_SD);
    else if (cmd == "HELP") printHelp();
    else if (cmd.length() > 0) Serial.println(F("Comando desconhecido. Digite HELP."));
}

void sendCommand(LoRaCommandType cmd) {
    LoRaCommand c;
    c.cmd = cmd;
    bool ok = lora.transmit(c);

    if (ok) {
        Serial.println(F(">> Comando RF enviado."));
    } else {
        Serial.println(F(">> Falha ao enviar comando RF."));
    }
}

void printHelp() {
    Serial.println(F("\n--- Comandos ---"));
    Serial.println(F(" READ  -> Telemetria | GPS   -> Coordenadas"));
    Serial.println(F(" ARMAR -> Forca Voo  | RESET -> Zera Altimetro"));
    Serial.println(F(" SD    -> Baixa Log  | PING  -> Testa Conexao"));
}