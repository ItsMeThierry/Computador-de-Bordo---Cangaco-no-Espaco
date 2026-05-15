/**
 * DataLogger.cpp
 *
 * Implementação do módulo de gravação de dados no SD Card.
 * Baseado no PRD RF-05 (seção 4.5.2) e no Projeto de Refatoração (seção 16).
 *
 * Responsabilidades:
 *   - Grava dados de voo no SD Card em alta resolução (100Hz / 10ms)
 *   - Grava apenas após sair do estado PRE_FLIGHT
 *   - Formato CSV: Timestamp,Altitude,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,Temp,State
 *   - Flush forçado a cada gravação (segurança contra perda de energia)
 *
 * O timestamp é o tempo de voo em segundos, relativo ao início do voo
 * (fonte única de verdade: FlightStateMachine::getFlightStartTime()).
 */

#include "DataLogger.h"

// ============================================================================
// Construtor
// ============================================================================

DataLogger::DataLogger(SDCard* sdCard)
    : _sdCard(sdCard),
      _enabled(true),
      _timer(RECORD_INTERVAL_SD_MS)  // 10ms → 100Hz (PRD RF-05)
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

    // Cabeçalho CSV para versão ESP32 (PRD seção 4.5.2)
    // Inclui todos os campos do AccelData: aceleração (3 eixos),
    // giroscópio (3 eixos) e temperatura do MPU6050
    _sdCard->setHeader(F("Timestamp,Altitude,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,Temp,State"));

    Serial.println(F("[DataLogger] Inicializado — gravação a 100Hz (10ms)"));
}

// ============================================================================
// Atualização — chamada a cada ciclo do loop principal
// ============================================================================

void DataLogger::update(unsigned long flightTimeMs, double altitude,
                        const AccelData& accel, FlightState state) {

    // PRD RF-05 seção 4.5.2: "Início da gravação: Após sair do estado PRE_FLIGHT"
    if (state == FlightState::PRE_FLIGHT) {
        return;
    }

    // Verifica se gravação está habilitada e SD disponível
    if (!_enabled || _sdCard == nullptr || !_sdCard->isReady()) {
        return;
    }

    // Timer não-bloqueante a 100Hz (RECORD_INTERVAL_SD_MS = 10ms)
    if (!_timer.isReady()) {
        return;
    }

    // Formata e grava linha CSV
    String line = _formatLine(flightTimeMs, altitude, accel, state);
    _sdCard->writeLine(line);
}

// ============================================================================
// Habilita/desabilita gravação
// ============================================================================

void DataLogger::setEnabled(bool enabled) {
    _enabled = enabled;

    if (enabled) {
        _timer.reset();  // Reinicia timer ao reabilitar
        Serial.println(F("[DataLogger] Gravação habilitada"));
    } else {
        Serial.println(F("[DataLogger] Gravação desabilitada"));
    }
}

// ============================================================================
// Formatação da linha CSV
// ============================================================================

/**
 * _formatLine — Formata uma linha CSV com os dados de voo.
 *
 * Formato (PRD seção 4.5.2, versão ESP32):
 *   Timestamp,Altitude,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,Temp,State
 *
 * - Timestamp: tempo de voo em segundos (float, 3 casas decimais)
 * - Altitude: altitude suavizada em metros (2 casas decimais)
 * - AccelX/Y/Z: aceleração nos 3 eixos (m/s², 2 casas decimais)
 * - GyroX/Y/Z: giroscópio nos 3 eixos (rad/s, 2 casas decimais)
 * - Temp: temperatura do sensor (°C, 1 casa decimal)
 * - State: string "PRE_FLIGHT", "ASCENT" ou "RECOVERY"
 *
 * Se os dados do acelerômetro não forem válidos (accel.valid == false),
 * os campos são preenchidos com 0.00 para manter o formato CSV consistente.
 */
String DataLogger::_formatLine(unsigned long flightTimeMs, double altitude,
                               const AccelData& accel, FlightState state) {

    // Converte timestamp de ms para segundos (PRD: "Tempo de voo em segundos")
    float timestampSec = flightTimeMs / 1000.0f;

    // Buffer para formatação — evita concatenação excessiva de String
    // Formato: "0.010,12.34,1.23,-0.45,9.81,0.01,-0.02,0.03,24.5,ASCENT"
    // Tamanho máximo estimado: ~100 caracteres
    char buffer[120];

    if (accel.valid) {
        snprintf(buffer, sizeof(buffer),
                 "%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%s",
                 timestampSec,
                 altitude,
                 accel.accX,
                 accel.accY,
                 accel.accZ,
                 accel.gyrX,
                 accel.gyrY,
                 accel.gyrZ,
                 accel.temp,
                 FlightStateUtils::toString(state));
    } else {
        // Dados inválidos — mantém formato CSV consistente com zeros
        snprintf(buffer, sizeof(buffer),
                 "%.3f,%.2f,0.00,0.00,0.00,0.00,0.00,0.00,0.0,%s",
                 timestampSec,
                 altitude,
                 FlightStateUtils::toString(state));
    }

    return String(buffer);
}
