#include "../../config/Constants.h"
#include "../../hardwareParts/Altimeter.h"
#include "../../hardwareParts/Accelerometer.h"
#include "../../hardwareParts/Buzzer.h"
#include "../../hardwareParts/LoRaRadio.h"
#include "../../hardwareParts/LED_RGB.h"
#include "../../hardwareParts/GPS.h"

#include "../../logic/DataStorage.h"
#include "../../logic/Telemetry.h"

#include "../../utils/Timer.h"

// Instanciando os objetos das classes no Hardware
Altimeter altimeter;
Accelerometer accelerometer;
Buzzer buzzer;
LED_RGB led;
LoRaRadio lora;
GPSSensor gps;

// Armazenamento e Estado
DataStorage dataStorage;
FlightStateMachine stateMachine;
Telemetry telemetry(&lora, &altimeter, &gps, &stateMachine, &dataStorage);

Timer sensorsTimer(SENSOR_READ_INTERVAL_MS);

double lastAltitude = 0.0;
unsigned long lastAltitudeReadMs = 0;
bool hasLastAltitude = false;

float getSelectedAccel(const AccelData &accel)
{
    if (!accel.valid)
        return 0.0f;
    if (VERTICAL_ACCEL_AXIS == 0)
        return accel.accX;
    if (VERTICAL_ACCEL_AXIS == 1)
        return accel.accY;
    return accel.accZ;
}

// ===== SETUP =====
void setup()
{
    Serial.begin(115200);
    delay(1000);
    randomSeed(micros());
    Serial.println(F("Iniciando Computador de Bordo ESP32"));
    dataStorage.begin();

    // Indica boot
    led.begin();
    led.setColor(0, 0, 255);
    led.setBlinkEnabled(false);

    // Inicializar buzzer (configura canal LEDC)
    buzzer.begin();

    bool accOk = accelerometer.begin(); // Inicializar acelerômetro (CRÍTICO)
    bool altOk = altimeter.begin();     // Inicializar altimetro (CRÍTICO)

    if (!accOk || !altOk) 
    {
        if (!accOk && !altOk) 
        {
            led.setColor(255, 0, 0); // Vermelho
            Serial.println(F("AVISO: Altímetro e Acelerometro falharam — interrompendo inicialização"));
        }

        if (!accOk && altOk || accOk && !altOk) 
        {
            led.setColor(255, 0, 255); // Magenta
            if (!accOk)
                Serial.println(F("AVISO: Acelerometro falhou — interrompendo inicialização"));
            if (!altOk)
                Serial.println(F("AVISO: Altímetro falhou — interrompendo inicialização"));
        }

        led.setBlinkInterval(500);
        led.setBlinkEnabled(true);

        while(1) led.update();
    }

    // Inicialização de componentes não-críticos
    gps.begin(9600);
    telemetry.begin();

    led.setColor(0, 255, 0); // Verde - boot ok
    // TODO: Som de buzzer específico para indicar sucesso

    delay(1000);

    // altimeter.resetBaseline();

    Serial.println(F("Sistema pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop()
{
    // Leitura dos sensores a 20 Hz
    if (sensorsTimer.isReady())
    {
        // Coleta os dados dos sensores
        const unsigned long now = millis();
        const double altitude = altimeter.calculateAltitude();
        const AccelData accel = accelerometer.readAcceleration();

        // Calcula a velocidade com base na altitude
        float verticalVelocity = 0.0f;

        if (hasLastAltitude){
            const unsigned long dtMs = now - lastAltitudeReadMs;
            if (dtMs > 0){
                verticalVelocity = static_cast<float>((altitude - lastAltitude) / (dtMs / 1000.0));
            }
        }

        lastAltitude = altitude;
        lastAltitudeReadMs = now;
        hasLastAltitude = true;

        // Coleta a aceleração pro eixo configurado
        const float selectedAccel = getSelectedAccel(accel);

        // Transição de maquina de estado
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
        telemetry.update(flightTimeMs, altitude, currentState);
    }

    led.update();
    buzzer.update();
}
