#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include "../hardwareParts/LoRaRadio.h"
#include "../hardwareParts/GPS.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"
#include "../config/Constants.h"

class Telemetry {
public:
    Telemetry(LoRaRadio* radio,
                           Altimeter* altimeter,
                           GPSSensor* gps,
                           FlightStateMachine* stateMachine,
                           DataStorage* dataStorage)
        : _radio(radio),
          _altimeter(altimeter),
          _gps(gps),
          _stateMachine(stateMachine),
          _dataStorage(dataStorage),
          _enabled(true),
          _txTimer(LORA_TX_INTERVAL_MS) {}

    void begin(int baudrate = 9600) {
        if (_radio != nullptr && !_radio->isReady()) {
            _radio->begin(baudrate);
        }

        Serial.println(F("[Telemetry] Módulo LoRa inicializado"));
    }

    /**
    * Executa a cada iteração do loop principal sem bloquear o sistema.
    */
   // TODO: Verificar funcionalidade do flightTimeMs
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
    LoRaRadio* _radio;
    Altimeter* _altimeter;
    GPSSensor* _gps;
    FlightStateMachine* _stateMachine;
    DataStorage* _dataStorage;
    bool _enabled;
    Timer _txTimer;

    void _listenCommands() {
        LoRaPacket<> packet;

        if (_radio->receivePacket(LORA_BASE_ID, &packet)) {
            uint8_t subCommand = packet[0];
            _handleCommand(subCommand);
        }
    }

    void _handleCommand(uint8_t command) {
        switch (command) {
            case CMD_ARMAR_FOGUETE: {
                if (_stateMachine != nullptr) {
                    _stateMachine->forceState(FlightState::ASCENT);
                    Serial.println(F("[RF4_CMD] Foguete armado -> Forçado estado SUBIDA"));
                    double alt = (_altimeter != nullptr) ? _altimeter->calculateAltitude() : 0.0;
                    _sendAutoTelemetry(alt, FlightState::ASCENT);
                }
                break;
            }

            case CMD_RESETAR_BASE: {
                if (_altimeter != nullptr) {
                    _altimeter->resetBaseline();
                    Serial.println(F("[RF4_CMD] Baseline do altímetro resetada"));
                    FlightState st = (_stateMachine != nullptr) ? _stateMachine->getCurrentState() : FlightState::PRE_FLIGHT;
                    _sendAutoTelemetry(0.0, st);
                }
                break;
            }

            case CMD_OBTER_GPS: {
                _sendGPSData();
                break;
            }

            case CMD_COLETAR_TELEMETRIA: {
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
        LoRaPacket<6> packet;

        packet.append(CMD_TELEMETRIA_AUTO);
        packet.append(static_cast<uint8_t>(state));
        packet.append(static_cast<int16_t>(altitudeBaro));

        _radio->sendPacket(LORA_BASE_ID, &packet);
    }

    void _sendGPSData() {
        LoRaPacket<9> packet;

        packet.append(CMD_OBTER_GPS);

        int32_t lat32 = 0;
        int32_t lon32 = 0;

        if (_gps != nullptr && _gps->hasFix()) {
            GPSData data = _gps->getLatestData();
            lat32 = static_cast<int32_t>(data.latitude * 1000000.0);
            lon32 = static_cast<int32_t>(data.longitude * 1000000.0);
        }

        packet.append(lat32);
        packet.append(lon32);

        _radio->sendPacket(LORA_BASE_ID, &packet);
    }

};

#endif