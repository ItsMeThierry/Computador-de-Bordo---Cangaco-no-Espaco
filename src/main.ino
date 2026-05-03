#include "config/Constants.h"
#include "hardware/LED_RGB.h"
#include "hardware/Buzzer.h"
#include "hardware/SDCard.h"
#include "hardware/Altimeter.h"
#include "hardware/Accelerometer.h"
#include "hardware/GPSSensor.h"
#include "hardware/LoRaRadio.h"
#include "utils/MovingAverage.h"
#include "utils/CircularQueue.h"
#include "logic/FlightStateMachine.h"
#include "logic/RecoverySystem.h"
#include "logic/DataStorage.h"
#include "logic/DataLogger.h"
#include "logic/Telemetry.h"

// ===== SETUP =====
void setup() {

    Serial.begin(115200);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    // TODO: Implementar lógica de setup somente após de implementar as funcionalidades dos arquivos
}

// ===== LOOP PRINCIPAL =====
void loop() {
    // TODO: Implementar lógica de loop somente após de implementar as funcionalidades dos arquivos
}
