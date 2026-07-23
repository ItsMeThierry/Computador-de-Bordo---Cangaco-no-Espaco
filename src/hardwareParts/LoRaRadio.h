#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>
#include "./LoRaMESH.h"
#include "../../config/Constants.h"

// --- Comandos do Protocolo Radioenge ---
enum LoRaCommand : uint8_t {
    CMD_COLETAR_TELEMETRIA  = 0x22, // Coleta manual de telemetria
    CMD_BAIXAR_DADOS        = 0x23, // Inicia download do log CSV do SD
    CMD_FIM_DOWNLOAD        = 0x24, // Sinaliza fim do download do SD
    CMD_OBTER_GPS           = 0x25, // Solicita latitude e longitude
    CMD_ARMAR_FOGUETE       = 0x26, // Força estado SUBIDA
    CMD_TELEMETRIA_AUTO     = 0x27, // Pacote de telemetria autônoma durante o voo
    CMD_RESETAR_BASE        = 0x28  // Zera baseline de altitude barométrica
};

// ID Padrão da Base (Master) e Foguete (Slave)
constexpr uint16_t LORA_BASE_ID         = 0; // ID 0 é a Estação Master de Solo
constexpr uint16_t LORA_ROCKET_ID       = 1; // ID 1 é o Foguete
constexpr uint16_t LORA_DEST_BROADCAST  = 0xFFFF;
constexpr uint8_t  LORA_APP_COMMAND_ID  = 0x28; // Wrapper da Radioenge

template<uint8_t PACKET_MAX_SIZE = MAX_PAYLOAD_SIZE>
class LoRaPacket {
public:
    LoRaPacket() : _currentSize() {}

    void clear() {
        _currentSize = 0;
    }

    bool append(uint8_t value) {
        if (_currentSize + 1 > PACKET_MAX_SIZE) return false;
        _buffer[_currentSize++] = value;
        return true;
    }

    bool append(int16_t value) {
        if (_currentSize + 2 > PACKET_MAX_SIZE) return false;

        // Empacotamento Big-Endian
        _buffer[_currentSize++] = (value >> 8) & 0XFF;
        _buffer[_currentSize++] = value & 0xFF;

        return true;
    }

    bool append(int32_t value) {
        if (_currentSize + 4 > PACKET_MAX_SIZE) return false;

        // Empacotamento Big-Endian
        _buffer[_currentSize++] = (value >> 24) & 0xFF;
        _buffer[_currentSize++] = (value >> 16) & 0xFF;
        _buffer[_currentSize++] = (value >> 8)  & 0xFF;
        _buffer[_currentSize++] = value & 0xFF;

        return true;
    }

    uint8_t operator[](size_t index) const {
        return _buffer[index];
    }

    uint8_t* data() { 
        return _buffer; 
    }

    uint8_t* pSize() {
        return &_currentSize;
    }

    uint8_t size() const { 
        return _currentSize; 
    }

    uint8_t maxSize() const { 
        return PACKET_MAX_SIZE; 
    }

    bool isEmpty() const {
        return _currentSize == 0;
    };

    bool isFull() const {
        return _currentSize == PACKET_MAX_SIZE;
    };

    // Verifica se cabe mais dados
    uint8_t remaining() const { 
        return PACKET_MAX_SIZE - _currentSize; 
    };

private:
    uint8_t _buffer[PACKET_MAX_SIZE];
    uint8_t _currentSize = 0;
};

class LoRaRadio {
public:
    LoRaRadio()
        : _serialLora(LORA_UART),
          _lora(&_serialLora),
          _initialized(false) {}

    // Inicializa e configura o módulo LoRa com parâmetros específicos para comunicação;
    // Exige um "baudrate" para configurar as portas RX e TX
    void begin(int baudrate) {
        _serialLora.begin(baudrate, SERIAL_8N1, PIN_LORA_RX, PIN_LORA_TX);
        _lora.begin(false);
        _initialized = true;
    }
    
    template<uint8_t PACKET_SIZE>
    bool sendPacket(uint16_t destId, LoRaPacket<PACKET_SIZE>* packet) {
        if (!_initialized) return false;

        if (_lora.PrepareFrameCommand(destId, LORA_APP_COMMAND_ID, packet->data(), packet->size())) {
            return _lora.SendPacket();
        }

        return false;
    }

    template<uint8_t PACKET_SIZE>
    bool receivePacket(uint16_t senderId, LoRaPacket<PACKET_SIZE>* packet) {
        if (!_initialized) return false;

        uint16_t id = 0;
        uint8_t commandIn = 0;

        bool recieved = _lora.ReceivePacketCommand(&id, &commandIn, packet->data(), packet->pSize(), 10);
  
        return (recieved 
                && commandIn == LORA_APP_COMMAND_ID 
                && packet->size() > 0 
                && id == senderId);
    }

    // Verifica se o LoRa está inicializado
    bool isReady() const {
        return _initialized;
    }

private:
    HardwareSerial _serialLora;
    LoRaMESH _lora;   
    bool _initialized;              
};

#endif