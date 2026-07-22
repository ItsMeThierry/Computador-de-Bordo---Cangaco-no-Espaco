#include "../../config/Constants.h"
#include "../../hardwareParts/Altimeter.h"
#include "../../hardwareParts/Accelerometer.h"
#include "../../hardwareParts/Buzzer.h"
#include "../../hardwareParts/LED_RGB.h"
#include "../../hardwareParts/GPS.h"
#include "../../hardwareParts/SDCard.h"
#include "../../hardwareParts/loras/LoRa_RadioEnge_v4.h"

#include "../../logic/DataStorage.h"
#include "../../logic/FlightStateMachine.h"
#include "../../logic/Telemetry_RadioEnge_v4.h"

#include "../../utils/Timer.h"

// Instanciando Objetos de Hardware
Altimeter altimeter;
Accelerometer accelerometer;
Buzzer buzzer(PIN_BUZZER);
LED_RGB led(PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
GPSSensor gps(PIN_GPS_RX, PIN_GPS_TX);
SDCard sdCard;
LoRaRadioEnge_v4 loraRadio(PIN_LORA_RX, PIN_LORA_TX);

// Armazenamento e Estado
DataStorage dataStorage;
FlightStateMachine stateMachine;
Telemetry_RadioEnge_v4 telemetry(&loraRadio, &altimeter, &gps, &stateMachine, &dataStorage, &sdCard);

Timer sensorsTimer(SENSOR_READ_INTERVAL_MS);

double lastAltitude = 0.0;
unsigned long lastAltitudeReadMs = 0;
bool hasLastAltitude = false;

float getSelectedAccel(const AccelData &accel) {
    if (!accel.valid) return 0.0f;
    if (VERTICAL_ACCEL_AXIS == 0) return accel.accX;
    if (VERTICAL_ACCEL_AXIS == 1) return accel.accY;
    return accel.accZ;
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(1000);
    randomSeed(micros());
    Serial.println(F("Iniciando Computador de Bordo ESP32 - RadioEnge v4"));

    dataStorage.begin();

    // Indica boot via LED
    led.begin();
    led.setColor(0, 0, 255); // Azul
    led.setBlinkEnabled(false);

    delay(2000);

    buzzer.begin();

    bool accOk = accelerometer.begin();
    bool altOk = altimeter.begin();

    if (!accOk || !altOk) {
        if (!accOk && !altOk) {
            led.setColor(255, 0, 0); // Vermelho
            Serial.println(F("AVISO: Altímetro e Acelerômetro falharam — interrompendo inicialização"));
        } else {
            led.setColor(255, 0, 255); // Magenta
            if (!accOk) Serial.println(F("AVISO: Acelerômetro falhou — interrompendo inicialização"));
            if (!altOk) Serial.println(F("AVISO: Altímetro falhou — interrompendo inicialização"));
        }

        led.setBlinkInterval(500);
        led.setBlinkEnabled(true);

        while (1) led.update();
    }

    // Inicialização de componentes secundários (GPS, SD e Radioenge LoRa v4)
    gps.begin(9600);
    sdCard.begin();
    loraRadio.begin(9600, false);
    telemetry.begin();

    led.setColor(0, 255, 0); // Verde - Boot OK
    delay(1000);

    Serial.println(F("Sistema de Bordo Radioenge v4 Pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop() {
    const unsigned long now = millis();

    // 1. Atualização do GPS
    gps.update();

    // 2. Leitura dos sensores a 50 Hz (20ms)
    if (sensorsTimer.isReady()) {
        const double altitude = altimeter.calculateAltitude();
        const AccelData accel = accelerometer.readAcceleration();

        float verticalVelocity = 0.0f;
        if (hasLastAltitude) {
            const unsigned long dtMs = now - lastAltitudeReadMs;
            if (dtMs > 0) {
                verticalVelocity = static_cast<float>((altitude - lastAltitude) / (dtMs / 1000.0));
            }
        }

        lastAltitude = altitude;
        lastAltitudeReadMs = now;
        hasLastAltitude = true;

        const float selectedAccel = getSelectedAccel(accel);

        const FlightState oldState = stateMachine.getCurrentState();
        stateMachine.update(altitude, verticalVelocity, selectedAccel);
        const FlightState currentState = stateMachine.getCurrentState();

        if (oldState == FlightState::PRE_FLIGHT && currentState == FlightState::ASCENT) {
            dataStorage.clearFlightLog();
        }

        const unsigned long flightTimeMs = (currentState == FlightState::PRE_FLIGHT)
                                               ? 0
                                               : now - stateMachine.getFlightStartTime();

        dataStorage.update(flightTimeMs, altitude, accel, currentState);

        // 3. Atualização não-bloqueante da Lógica LoRa Telemetry v4
        telemetry.update(flightTimeMs, altitude, currentState);
    }

    led.update();
    buzzer.update();
}
