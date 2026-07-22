#ifndef TELEMETRY_RADIOENGE_V4_H
#define TELEMETRY_RADIOENGE_V4_H

#include <Arduino.h>
#include "../hardwareParts/loras/LoRa_RadioEnge_v4.h"
#include "../hardwareParts/Altimeter.h"
#include "../hardwareParts/GPS.h"
#include "../hardwareParts/SDCard.h"
#include "../logic/FlightStateMachine.h"
#include "../logic/DataStorage.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"
#include "../config/Constants.h"

class Telemetry_RadioEnge_v4 {
public:
    Telemetry_RadioEnge_v4(LoRaRadioEnge_v4* radio,
                           Altimeter* altimeter,
                           GPSSensor* gps,
                           FlightStateMachine* stateMachine,
                           DataStorage* dataStorage = nullptr,
                           SDCard* sdCard = nullptr)
        : _radio(radio),
          _altimeter(altimeter),
          _gps(gps),
          _stateMachine(stateMachine),
          _dataStorage(dataStorage),
          _sdCard(sdCard),
          _enabled(true),
          _txTimer(LORA_TX_INTERVAL_MS) {}

    void begin() {
        if (_radio != nullptr && !_radio->isReady()) {
            _radio->begin(9600, false);
        }
        Serial.println(F("[Telemetry_v4] Módulo LoRa Radioenge v4 inicializado"));
    }

    /**
     * Executa a cada iteração do loop principal sem bloquear o sistema.
     */
    void update(unsigned long flightTimeMs, double altitudeBaro, FlightState currentState) {
        if (!_enabled || _radio == nullptr) return;

        // 1. Escuta comandos recebidos da Base (Master)
        _listenCommands();

        // 2. Transmite telemetria autônoma de voo a cada 1 Hz durante SUBIDA e DESCIDA
        if (currentState == FlightState::ASCENT ||
            currentState == FlightState::RECOVERY_STAGE_ONE ||
            currentState == FlightState::RECOVERY_STAGE_TWO) {
            
            if (_txTimer.isReady()) {
                _sendAutoTelemetry(altitudeBaro, currentState);
            }
        }
    }

    void setEnabled(bool enabled) {
        _enabled = enabled;
    }

private:
    LoRaRadioEnge_v4* _radio;
    Altimeter* _altimeter;
    GPSSensor* _gps;
    FlightStateMachine* _stateMachine;
    DataStorage* _dataStorage;
    SDCard* _sdCard;
    bool _enabled;
    Timer _txTimer;

    void _listenCommands() {
        uint16_t senderId = 0;
        uint8_t commandIn = 0;
        uint8_t rxBuffer[MAX_PAYLOAD_SIZE];
        uint8_t rxSize = 0;

        // Timeout rápido de 5ms para não travar o loop de sensores/controle
        if (_radio->receivePacket(&senderId, &commandIn, rxBuffer, &rxSize, 5)) {
            if (commandIn == LORA_APP_COMMAND_ID_V4 && rxSize > 0) {
                uint8_t subCommand = rxBuffer[0];
                _handleCommand(senderId, subCommand);
            }
        }
    }

    void _handleCommand(uint16_t senderId, uint8_t command) {
        switch (command) {
            case CMD_ARMAR_FOGUETE_V4: {
                if (_stateMachine != nullptr) {
                    _stateMachine->forceState(FlightState::ASCENT);
                    Serial.println(F("[RF4_CMD] Foguete armado -> Forçado estado SUBIDA"));
                }
                break;
            }

            case CMD_RESETAR_BASE_V4: {
                if (_altimeter != nullptr) {
                    _altimeter->resetBaseline();
                    Serial.println(F("[RF4_CMD] Baseline do altímetro resetada"));
                }
                break;
            }

            case CMD_OBTER_GPS_V4: {
                _sendGPSData(senderId);
                break;
            }

            case CMD_BAIXAR_SD_V4: {
                // Apenas transfere SD se estiver em solo por segurança
                FlightState st = (_stateMachine != nullptr) ? _stateMachine->getCurrentState() : FlightState::PRE_FLIGHT;
                if (st == FlightState::PRE_FLIGHT || st == FlightState::LANDED) {
                    _streamSDData(senderId);
                } else {
                    Serial.println(F("[RF4_CMD] Ignorado download SD — Foguete em voo!"));
                }
                break;
            }

            case CMD_COLETAR_TELEMETRIA_V4: {
                double alt = (_altimeter != nullptr) ? _altimeter->calculateAltitude() : 0.0;
                FlightState st = (_stateMachine != nullptr) ? _stateMachine->getCurrentState() : FlightState::PRE_FLIGHT;
                _sendAutoTelemetry(alt, st);
                break;
            }

            default:
                Serial.print(F("[RF4_CMD] Comando desconhecido: 0x"));
                Serial.println(command, HEX);
                break;
        }
    }

    void _sendAutoTelemetry(double altitudeBaro, FlightState state) {
        uint8_t txBuffer[6];
        txBuffer[0] = CMD_TELEMETRIA_AUTO_V4;
        txBuffer[1] = static_cast<uint8_t>(state);

        int16_t altInt = static_cast<int16_t>(altitudeBaro);
        txBuffer[2] = (altInt >> 8) & 0xFF;
        txBuffer[3] = altInt & 0xFF;

        _radio->sendPacket(LORA_DEST_BROADCAST_V4, txBuffer, 4);
    }

    void _sendGPSData(uint16_t senderId) {
        uint8_t txBuffer[9];
        txBuffer[0] = CMD_OBTER_GPS_V4;

        int32_t lat32 = 0;
        int32_t lon32 = 0;

        if (_gps != nullptr && _gps->hasFix()) {
            GPSData data = _gps->getLatestData();
            lat32 = static_cast<int32_t>(data.latitude * 1000000.0);
            lon32 = static_cast<int32_t>(data.longitude * 1000000.0);
        }

        // Empacotamento Big-Endian (4 bytes Lat, 4 bytes Lon)
        txBuffer[1] = (lat32 >> 24) & 0xFF;
        txBuffer[2] = (lat32 >> 16) & 0xFF;
        txBuffer[3] = (lat32 >> 8)  & 0xFF;
        txBuffer[4] = lat32 & 0xFF;

        txBuffer[5] = (lon32 >> 24) & 0xFF;
        txBuffer[6] = (lon32 >> 16) & 0xFF;
        txBuffer[7] = (lon32 >> 8)  & 0xFF;
        txBuffer[8] = lon32 & 0xFF;

        _radio->sendPacket(senderId, txBuffer, 9);
        Serial.println(F("[RF4] Telemetria GPS transmitida via LoRa"));
    }

    void _streamSDData(uint16_t senderId) {
        Serial.println(F("[RF4] Iniciando transmissão de dados do SD..."));
        
        if (_sdCard != nullptr && _sdCard->isReady()) {
            File file = SD.open("/flight_data.csv", FILE_READ);
            if (file) {
                uint8_t txBuffer[100];
                txBuffer[0] = CMD_BAIXAR_SD_V4;

                while (file.available()) {
                    int index = 1;
                    while (file.available() && index < 100) {
                        txBuffer[index++] = file.read();
                    }
                    _radio->sendPacket(senderId, txBuffer, index);
                    delay(300); // Intervalo de cadência RF
                }
                file.close();
            }
        }

        // Pacote finalizador
        uint8_t endBuffer[1] = { CMD_FIM_ARQUIVO_V4 };
        _radio->sendPacket(senderId, endBuffer, 1);
        Serial.println(F("[RF4] Download de log finalizado"));
    }
};

#endif
