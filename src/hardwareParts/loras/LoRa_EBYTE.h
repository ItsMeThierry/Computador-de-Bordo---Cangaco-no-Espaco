#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>
#include <LoRa_E220.h>

// Código LoRa da EBYTE adaptado de Bruno Lopes

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

enum LoRaResponseType : uint8_t {
  RESP_DATA       = 1, // Retorna Telemetria (Alt, Acc, Estado)
  RESP_PONG       = 2,
  RESP_GPS        = 3, // Retorna GPS (Lat, Lon)
  RESP_SD_FIM     = 4, // Sinaliza fim do download do SD
  RESP_ERROR      = 5
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
          _initialized(false),
          _lora(&Serial2, auxPin, m0Pin, m1Pin) {}

    // Inicializa e configura o módulo LoRa com parâmetros específicos para comunicação;
    // Exige um "baudrate" para configurar as portas RX e TX
    void begin(int baudrate = 9600) {
        Serial2.begin(baudrate, SERIAL_8N1, _rxPin, _txPin);
        _lora.begin();

        _initialized = true;
    }
    
    // Recebe um comando da base de lançamentos
    LoRaCommand receive_command() {
        LoRaCommand c;
        c.cmd = CMD_ERROR;

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

    LoRaResponse receive() {
        LoRaResponse r;
        r.resp = RESP_ERROR;
        r.timestamp = 0;
        r.valor1 = 0.0f;
        r.valor2 = 0.0f;
        r.contador = 0;

        if (_lora.available() > 1) {
            ResponseStructContainer rsc = _lora.receiveMessage(sizeof(LoRaResponse));

            if (rsc.status.code == E220_SUCCESS) {
                memcpy(&r, rsc.data, sizeof(LoRaResponse));

                // Verifica se é um pacote inválido (tipos de 1 a 4). 
                // Se for lixo RF, limpa o buffer para ressincronizar.
                if (r.resp < RESP_DATA || r.resp > RESP_SD_FIM) {
                    Serial.println(F("[AVISO] Ruido RF detetado. A limpar buffer..."));
                    while(Serial2.available()) Serial2.read(); // Limpa o lixo
                    r.resp = RESP_ERROR;
                }

            } else {
                r.resp = RESP_ERROR;
            }

            rsc.close();
        }

        return r;
    }

    bool transmit(LoRaResponse& response) {
        ResponseStatus status = _lora.sendMessage(&response, sizeof(response));

        return status.code == E220_SUCCESS;
    }

    bool transmit(LoRaCommand& command) {
        ResponseStatus status = _lora.sendMessage(&command, sizeof(command));

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
