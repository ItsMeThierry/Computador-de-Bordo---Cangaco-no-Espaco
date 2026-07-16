#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>
#include <LoRa_E220.h>

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
          _lora(&Serial2, auxPin, m0Pin, m1Pin) {}

    // Inicializa e configura o módulo LoRa com parâmetros específicos para comunicação;
    // Exige um "baudrate" para configurar as portas RX e TX
    void begin(int baudrate) {
        _serialLoRa.begin(baudrate, SERIAL_8N1, _rxPin, _txPin);
        _lora.begin();

        _initialized = true;
    }
    
    // Recebe um comando da base de lançamentos
    LoRaCommand receive() {
        LoRaCommand c;

        if (_lora.available() > 1) {
            ResponseStructContainer rsc = _lora.receiveMessage(sizeof(LoRaCommand));

            if (rsc.status.code == E220_SUCCESS) {
                memcpy(&c, rsc.data, sizeof(LoRaCommand));
            } else {
                c.cmd = CMD_ERROR;
            }

            rsc.close();
        }

        return c;
    }

    bool transmit(LoRaResponse& response) {
        ResponseStatus status = _lora.sendMessage(&response, sizeof(response));

        return status.code == E220_SUCCESS;
    }

    // Verifica se o LoRa está inicializado
    bool isReady() const {
        return _initialized;
    }

private:
    int _rxPin, _txPin, _m0Pin, _m1Pin, _auxPin;
    bool _initialized;

    LoRa_E220 _lora;
};

#endif