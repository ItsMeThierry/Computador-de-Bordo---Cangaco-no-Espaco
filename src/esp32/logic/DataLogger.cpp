/**
 * DataLogger.cpp
 *
 * Gravação de dados de voo no SD Card em alta resolução (100Hz / 10ms).
 *
 * Política de gravação por estado:
 *   PRE_FLIGHT:          Desativada (economiza espaço)
 *   ASCENT:              SD 100Hz
 *   ACTIVATE_SKIB_ONE:   SD 100Hz
 *   RECOVERY_STAGE_ONE:  SD 100Hz
 *   ACTIVATE_SKIB_TWO:   SD 100Hz
 *   RECOVERY_STAGE_TWO:  SD 100Hz
 *   LANDED:              Desativada
 *
 * Formato CSV:
 *   Timestamp,Altitude,VertVel,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,Temp,Lat,Lon,GpsAlt,State
 */

#include "DataLogger.h"

// ============================================================================
// Construtor
// ============================================================================

DataLogger::DataLogger(SDCard* sdCard)
    : _sdCard(sdCard),
      _enabled(true),
      _timer(SD_INTERVAL_MS)  // 10ms → 100Hz
{
}

// ============================================================================
// Inicialização — define o cabeçalho CSV
// ============================================================================

void DataLogger::begin() {
    if (_sdCard == nullptr) {
        Serial.println(F("[DataLogger] Erro: ponteiro SDCard nulo"));
        return;
    }

    // Cabeçalho CSV completo com dados de GPS
    _sdCard->setHeader(
        F("Timestamp,Altitude,VertVel,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,Temp,Lat,Lon,GpsAlt,State")
    );

    Serial.println(F("[DataLogger] Inicializado — gravação a 100Hz (10ms)"));
}

// ============================================================================
// Atualização — chamada a cada ciclo do loop principal
// ============================================================================

void DataLogger::update(unsigned long flightTimeMs, double altitude,
                        float verticalVelocity, const AccelData& accel,
                        const GPSData& gpsData, FlightState state) {

    // Estados sem gravação
    if (state == FlightState::PRE_FLIGHT || state == FlightState::LANDED) {
        return;
    }

    // Verifica se gravação está habilitada e SD disponível
    if (!_enabled || _sdCard == nullptr || !_sdCard->isReady()) {
        return;
    }

    // Timer não-bloqueante a 100Hz
    if (!_timer.isReady()) {
        return;
    }

    // Formata e grava linha CSV
    String line = _formatLine(flightTimeMs, altitude, verticalVelocity,
                              accel, gpsData, state);
    _sdCard->writeLine(line);
}

// ============================================================================
// Habilita/desabilita gravação
// ============================================================================

void DataLogger::setEnabled(bool enabled) {
    _enabled = enabled;

    if (enabled) {
        _timer.reset();
        Serial.println(F("[DataLogger] Gravação habilitada"));
    } else {
        Serial.println(F("[DataLogger] Gravação desabilitada"));
    }
}

// ============================================================================
// Formatação da linha CSV
// ============================================================================

String DataLogger::_formatLine(unsigned long flightTimeMs, double altitude,
                               float verticalVelocity, const AccelData& accel,
                               const GPSData& gpsData, FlightState state) {

    float timestampSec = flightTimeMs / 1000.0f;

    // Buffer maior para incluir dados de GPS
    char buffer[200];

    if (accel.valid) {
        snprintf(buffer, sizeof(buffer),
                 "%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.6f,%.6f,%.1f,%s",
                 timestampSec,
                 altitude,
                 verticalVelocity,
                 accel.accX, accel.accY, accel.accZ,
                 accel.gyrX, accel.gyrY, accel.gyrZ,
                 accel.temp,
                 gpsData.latitude, gpsData.longitude, gpsData.altitude,
                 FlightStateUtils::toString(state));
    } else {
        snprintf(buffer, sizeof(buffer),
                 "%.3f,%.2f,%.2f,0.00,0.00,0.00,0.00,0.00,0.00,0.0,%.6f,%.6f,%.1f,%s",
                 timestampSec,
                 altitude,
                 verticalVelocity,
                 gpsData.latitude, gpsData.longitude, gpsData.altitude,
                 FlightStateUtils::toString(state));
    }

    return String(buffer);
}
