#ifndef LORA_RADIOENGE_V4_H
#define LORA_RADIOENGE_V4_H

#include <Arduino.h>
#include "LoRaMESH_v4.h"

// --- Comandos do Protocolo Radioenge v4 ---
enum LoRaCommandRadioEnge_v4 : uint8_t {
    CMD_COLETAR_TELEMETRIA_V4 = 0x22, // Coleta manual de telemetria
    CMD_BAIXAR_SD_V4          = 0x23, // Inicia download do log CSV do SD
    CMD_FIM_ARQUIVO_V4        = 0x24, // Sinaliza fim do download do SD
    CMD_OBTER_GPS_V4          = 0x25, // Solicita latitude e longitude
    CMD_ARMAR_FOGUETE_V4      = 0x26, // Força estado SUBIDA
    CMD_TELEMETRIA_AUTO_V4    = 0x27, // Pacote de telemetria autônoma durante o voo
    CMD_RESETAR_BASE_V4       = 0x28  // Zera baseline de altitude barométrica
};

// ID Padrão da Base (Master) e Foguete (Slave)
constexpr uint16_t LORA_BASE_ID_V4         = 0; // ID 0 é a Estação Master de Solo
constexpr uint16_t LORA_ROCKET_ID_V4       = 1; // ID 1 é o Foguete
constexpr uint16_t LORA_DEST_BROADCAST_V4 = 0xFFFF;
constexpr uint8_t  LORA_APP_COMMAND_ID_V4  = 0x28; // Wrapper da Radioenge

class LoRaRadioEnge_v4 {
public:
    LoRaRadioEnge_v4(int rxPin, int txPin, int uartNr = 2)
        : _rxPin(rxPin),
          _txPin(txPin),
          _uartNr(uartNr),
          _serialLora(uartNr),
          _lora(&_serialLora),
          _initialized(false) {}

    void begin(int baudrate = 9600, bool debug = false) {
        _serialLora.begin(baudrate, SERIAL_8N1, _rxPin, _txPin);
        _lora.begin(debug);
        _initialized = true;
    }

    bool sendPacket(uint16_t destId, uint8_t* payload, uint8_t payloadSize) {
        if (!_initialized) return false;
        if (_lora.PrepareFrameCommand(destId, LORA_APP_COMMAND_ID_V4, payload, payloadSize)) {
            return _lora.SendPacket();
        }
        return false;
    }

    bool receivePacket(uint16_t* senderId, uint8_t* commandIn, uint8_t* payload, uint8_t* payloadSize, uint32_t timeoutMs = 10) {
        if (!_initialized) return false;
        return _lora.ReceivePacketCommand(senderId, commandIn, payload, payloadSize, timeoutMs);
    }

    bool isReady() const {
        return _initialized;
    }

    LoRaMESH* getMeshDriver() {
        return &_lora;
    }

private:
    int _rxPin;
    int _txPin;
    int _uartNr;
    HardwareSerial _serialLora;
    LoRaMESH _lora;
    bool _initialized;
};

#endif
