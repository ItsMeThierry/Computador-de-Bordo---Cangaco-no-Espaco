#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>
#include "LoRaMESH.h"

// Código LoRa RadioEnge adaptado de Bruno Lopes

enum LoRaCommandType : uint8_t {
  CMD_READ        = 1, // Pede telemetria
  CMD_PING        = 2, 
  CMD_ARMAR       = 3, // Força estado SUBIDA
  CMD_RESET_BASE  = 4, // Zera baseline barométrica
  CMD_GET_GPS     = 5, // Solicita Coordenadas
  CMD_DOWNLOAD_SD = 6, // Inicia Dump do Cartão SD
  CMD_ERROR       = 7
};

struct LoRaCommand {
  uint8_t cmd;
};

struct LoRaResponse {
  uint8_t  resp;       
  uint32_t timestamp;  
  float    valor1;     // Usado para Altitude ou Latitude
  float    valor2;     // Usado para Aceleração ou Longitude
  uint16_t contador;   // Usado para enviar o Estado do Voo
};

class LoRaRadio {
public:
    LoRaRadio(int rxPin, int txPin, int m0Pin, int m1Pin, int auxPin)
        : _rxPin(rxPin),
          _txPin(txPin),
          _m0Pin(m0Pin),
          _m1Pin(m1Pin),
          _auxPin(auxPin),
          _serialLora(2),
          _lora(&_serialLora) {}

    // Inicializa e configura o módulo LoRa com parâmetros específicos para comunicação;
    // Exige um "baudrate" para configurar as portas RX e TX
    void begin(int baudrate) {
        _serialLoRa.begin(baudrate, SERIAL_8N1, _rxPin, _txPin);
        _lora.begin(false);

        _initialized = true;
    }
    
    // Recebe um comando da base de lançamentos
    LoRaCommand receive() {
        _lora.ReceivePacketCommand(&_senderId, &_commandIn, _rxBuffer, &_rxSize, 10);

        if (commandIn != 0x28) {
            LoRaCommand c;
            c.cmd = CMD_ERROR;

            return c;
        }

        return rxBuffer[0];
    }

    // TODO: Finalizar transmit e rever a estrutura de response
    bool transmit(LoRaResponse& response) {
        uint8_t _txBuffer;

        if (_lora.PrepareFrameCommand(_senderId, 0x28, _txBuffer, 4)) {
            _lora.SendPacket();
        }
    }

    // Verifica se o LoRa está inicializado
    bool isReady() const {
        return _initialized;
    }

private:
    int _rxPin, _txPin, _m0Pin, _m1Pin, _auxPin;
    bool _initialized;

    HardwareSerial _serialLora;
    LoRaMESH _lora;

    uint16_t _senderId;     
    uint8_t _commandIn;                      
    uint8_t _rxBuffer[MAX_PAYLOAD_SIZE];     
    uint8_t _rxSize;                  
};

#endif