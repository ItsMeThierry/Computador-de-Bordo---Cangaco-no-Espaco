/**
 * DataLogger.cpp
 *
 * Gravação de dados de voo no SD Card a 100Hz (10ms).
 * Grava apenas após sair do estado PRE_FLIGHT.
 * Flush forçado a cada gravação (segurança contra perda de energia).
 *
 * Timestamp relativo ao início do voo (FlightStateMachine::getFlightStartTime()).
 *
 * GPS atualiza a 1Hz; entre atualizações, campos GPS repetem último valor conhecido.
 *
 * TODO: Expor temperatura do BMP280 (readTemperature) no Altimeter.h
 */

// ======================================================================
// TOGGLE DE MODO DE GRAVAÇÃO
// ======================================================================
// Descomente para gravar TODOS os campos de todos os sensores (16 colunas).
// Comentado = modo normal otimizado (11 colunas, sem dados redundantes).
//
// Normal:    Timestamp,AltBaro,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,Lat,Lon,State
// Full data: Timestamp,AltBaro,Pressure,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,TempMPU,Lat,Lon,AltGPS,Speed,Fix,State
//
// #define FULL_DATA_LOGGING
// ======================================================================

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "../hardwareParts/SDCard.h"
#include "../hardwareParts/Accelerometer.h"
#include "../hardwareParts/GPS.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"
#include "../config/Constants.h"

class DataLogger {
public:
    DataLogger(SDCard* sdCard)
        : _sdCard(sdCard),
        _enabled(true),
        _timer(RECORD_INTERVAL_SD_MS) {}

    // Inicialização — define o cabeçalho CSV
    void begin() {
        if (_sdCard == nullptr) {
            Serial.println(F("[DataLogger] Erro: ponteiro SDCard nulo"));
            return;
        }

    #ifdef FULL_DATA_LOGGING
        // 16 colunas: adiciona Pressure, TempMPU, AltGPS, Speed, Fix
        // Aceleração/giroscópio com 4 casas decimais (~930 KB / 60s de voo)
        _sdCard->setHeader(F("Timestamp,AltBaro,Pressure,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,TempMPU,Lat,Lon,AltGPS,Speed,Fix,State"));
        Serial.println(F("[DataLogger] Modo FULL_DATA — 16 colunas"));
    #else
        // 11 colunas: sem Pressure, TempMPU, AltGPS, Speed, Fix
        // Aceleração/giroscópio com 2 casas decimais (~600 KB / 60s de voo)
        _sdCard->setHeader(F("Timestamp,AltBaro,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,Lat,Lon,State"));
        Serial.println(F("[DataLogger] Modo NORMAL — 11 colunas"));
    #endif

        Serial.println(F("[DataLogger] Inicializado — gravação a 100Hz (10ms)"));
    }

    /**
    * update() — Grava uma linha CSV no SD Card se o timer estiver pronto.
    *
    * @param flightTimeMs  Tempo de voo em milissegundos (fonte: FlightStateMachine)
    * @param altitude      Altitude barométrica suavizada (metros)
    * @param pressure      Pressão bruta do BMP280 (hPa) — usada apenas em FULL_DATA_LOGGING
    * @param accel         Dados do MPU6050 (aceleração + giroscópio + temperatura)
    * @param gps           Dados do GPS NEO-M6N (lat, lon, alt, speed, fix)
    * @param state         Estado atual da máquina de estados de voo
    */
    void update(unsigned long flightTimeMs, double altitude, double pressure,
                const AccelData& accel, const GPSData& gps, FlightState state) {

        // Não grava em PRE_FLIGHT (PRD RF-05 seção 4.5.2)
        if (state == FlightState::PRE_FLIGHT) {
            return;
        }

        if (!_enabled || _sdCard == nullptr || !_sdCard->isReady()) {
            return;
        }

        // Timer não-bloqueante a 100Hz
        if (!_timer.isReady()) {
            return;
        }

        String line = _formatLine(flightTimeMs, altitude, pressure, accel, gps, state);
        _sdCard->writeLine(line);
    }

    // Habilita/desabilita gravação
    void setEnabled(bool enabled) {
        _enabled = enabled;

        if (enabled) {
            _timer.reset();
            Serial.println(F("[DataLogger] Gravação habilitada"));
        } else {
            Serial.println(F("[DataLogger] Gravação desabilitada"));
        }
    }

private:
    SDCard* _sdCard;
    bool _enabled;
    Timer _timer;

    // Formatação da linha CSV
    String _formatLine(unsigned long flightTimeMs, double altitude, double pressure,
                        const AccelData& accel, const GPSData& gps, FlightState state) {

        float timestampSec = flightTimeMs / 1000.0f;

    #ifdef FULL_DATA_LOGGING
        char buffer[200];
    #else
        char buffer[140];
    #endif

        if (accel.valid) {

    #ifdef FULL_DATA_LOGGING
            snprintf(buffer, sizeof(buffer),
                    "%.3f,%.2f,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.1f,%.6f,%.6f,%.1f,%.1f,%u,%s",
                    timestampSec, altitude, pressure,
                    accel.accX, accel.accY, accel.accZ,
                    accel.gyrX, accel.gyrY, accel.gyrZ,
                    accel.temp,
                    gps.latitude, gps.longitude,
                    gps.altitude, gps.speed, gps.fixQuality,
                    FlightStateUtils::toString(state));
    #else
            snprintf(buffer, sizeof(buffer),
                    "%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.6f,%.6f,%s",
                    timestampSec, altitude,
                    accel.accX, accel.accY, accel.accZ,
                    accel.gyrX, accel.gyrY, accel.gyrZ,
                    gps.latitude, gps.longitude,
                    FlightStateUtils::toString(state));
    #endif

        } else {
            // Acelerômetro inválido — zeros para manter CSV consistente

    #ifdef FULL_DATA_LOGGING
            snprintf(buffer, sizeof(buffer),
                    "%.3f,%.2f,%.2f,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0,%.6f,%.6f,%.1f,%.1f,%u,%s",
                    timestampSec, altitude, pressure,
                    gps.latitude, gps.longitude,
                    gps.altitude, gps.speed, gps.fixQuality,
                    FlightStateUtils::toString(state));
    #else
            snprintf(buffer, sizeof(buffer),
                    "%.3f,%.2f,0.00,0.00,0.00,0.00,0.00,0.00,%.6f,%.6f,%s",
                    timestampSec, altitude,
                    gps.latitude, gps.longitude,
                    FlightStateUtils::toString(state));
    #endif
        }

        return String(buffer);
    }
};

#endif
