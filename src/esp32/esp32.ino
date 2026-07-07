/**
 * esp32.ino — Computador de Bordo Cangaço no Espaço
 *
 * Máquina de 7 estados de voo:
 *   PRE_FLIGHT → ASCENT → ACTIVATE_SKIB_ONE → RECOVERY_STAGE_ONE
 *   → ACTIVATE_SKIB_TWO → RECOVERY_STAGE_TWO → LANDED
 *
 * Sensores: BMP280 (altitude), MPU6050 (aceleração), NEO-6M (GPS 5Hz)
 * Atuadores: 2x SKIB, Buzzer, LED RGB, LoRa, SD Card, EEPROM
 */

#include "config/Constants.h"
#include "hardwareParts/Accelerometer.h"
#include "hardwareParts/Altimeter.h"
#include "hardwareParts/Buzzer.h"
#include "hardwareParts/LED_RGB.h"
#include "hardwareParts/SDCard.h"
#include "hardwareParts/LoRaRadio.h"
#include "hardwareParts/GPS.h"

#include "logic/FlightStateMachine.h"
#include "logic/RecoverySystem.h"
#include "logic/DataLogger.h"
// #include "logic/Telemetry.h"  // TODO: Habilitar quando LoRa estiver pronto

#include "utils/Timer.h"
#include "utils/MovingAverage.h"
#include "utils/FlightStateUtils.h"

// ========== INSTÂNCIAS DE HARDWARE ==========
Altimeter altimeter;
Accelerometer MPU;
Buzzer buzzer(PIN_BUZZER);
LED_RGB led(PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
SDCard sdCard(PIN_SD_CS);
GPSSensor gps(PIN_GPS_RX, PIN_GPS_TX);
LoRaRadio lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1, PIN_LORA_AUX);

// ========== INSTÂNCIAS DE LÓGICA ==========
FlightStateMachine fsm;
RecoverySystem recovery(PIN_SKIB_1, PIN_SKIB_2, &buzzer);
DataLogger dataLogger(&sdCard);
// Telemetry telemetry(&lora, &gps);

// ========== FILTROS E TIMERS ==========
MovingAverage<WINDOW_SIZE> altitudeFilter;
Timer sensorTimer(SENSOR_READ_INTERVAL_MS);  // 20Hz — leitura de sensores

// ========== VARIÁVEIS DE ESTADO ==========
double currentAltitude   = 0.0;    // Altitude suavizada (m)
double previousAltitude  = 0.0;    // Altitude anterior para cálculo de velocidade
float  verticalVelocity  = 0.0f;   // Velocidade vertical (m/s, positivo = subindo)
float  verticalAccelMs2  = 0.0f;   // Aceleração no eixo vertical (m/s²)
unsigned long lastSensorReadTime = 0;  // Para cálculo de deltaT

// Dados dos sensores — atualizados a cada ciclo
AccelData  accelData;
GPSData    gpsData;

// Status dos sensores
bool altimeterOk = false;
bool accelOk     = false;
bool sdOk        = false;

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);

    // CRÍTICO: Inicializar SKIBs primeiro — garantir LOW antes de qualquer coisa
    recovery.begin();

    delay(2000);
    Serial.println(F("========================================"));
    Serial.println(F("  Computador de Bordo — Cangaço no Espaço"));
    Serial.println(F("  Máquina de 7 Estados"));
    Serial.println(F("========================================"));

    // LED: indica boot
    led.begin();
    led.setColor(0, 0, 255);  // Azul durante inicialização
    led.setBlinkEnabled(false);

    // Buzzer
    buzzer.begin();

    // Altímetro BMP280 — CRÍTICO para transições de estado
    altimeterOk = altimeter.begin();
    if (!altimeterOk) {
        Serial.println(F("ERRO CRÍTICO: Altímetro BMP280 falhou!"));
        // TODO: Decidir se bloqueia ou continua sem altímetro
    } else {
        Serial.println(F("Altímetro BMP280 ok!"));
    }

    // Acelerômetro MPU6050 — CRÍTICO para transição PRE_FLIGHT → ASCENT
    accelOk = MPU.begin();
    if (!accelOk) {
        Serial.println(F("AVISO: Acelerômetro MPU6050 falhou"));
    } else {
        Serial.println(F("Acelerômetro MPU6050 ok!"));
    }

    // SD Card — não-crítico
    sdOk = sdCard.begin();
    if (!sdOk) {
        Serial.println(F("AVISO: SD Card falhou — continuando sem gravação SD"));
    } else {
        Serial.println(F("SD Card ok!"));
        dataLogger.begin();
    }

    // GPS NEO-6M — inicializa e configura 5Hz
    gps.begin(GPS_UART_BAUD);
    gps.setEnabled(false);  // Desativado no PRE_FLIGHT (conforme diagrama)

    // LoRa — TODO: Habilitar quando pronto
    // lora.begin(LORA_UART_BAUD);
    // telemetry.begin();

    // Configurar periféricos para estado inicial (PRE_FLIGHT)
    _configurePeripheralsForState(FlightState::PRE_FLIGHT);

    // Consumir o flag de state changed do boot
    fsm.hasStateChanged();

    lastSensorReadTime = millis();
    Serial.println(F("Sistema pronto! Estado: PRE_FLIGHT"));
    Serial.println(F("========================================"));
}

// ========== LOOP PRINCIPAL ==========
void loop() {

    // ---- 1. Leitura de sensores a 20Hz (50ms) ----
    if (sensorTimer.isReady()) {
        unsigned long now = millis();
        float deltaT = (now - lastSensorReadTime) / 1000.0f;  // segundos
        lastSensorReadTime = now;

        // Altímetro
        if (altimeterOk) {
            double pressure = altimeter.readPressure();
            double rawAlt = altimeter.calculateAltitude(pressure, altimeter.getBaseline());
            previousAltitude = currentAltitude;
            currentAltitude = altitudeFilter.filter(rawAlt);

            // Calcular velocidade vertical (derivada numérica)
            if (deltaT > 0.001f) {  // Evitar divisão por zero
                verticalVelocity = (float)(currentAltitude - previousAltitude) / deltaT;
            }
        }

        // Acelerômetro
        if (accelOk) {
            accelData = MPU.readAcceleration();

            // Extrair aceleração no eixo vertical conforme configuração
            // VERTICAL_ACCEL_AXIS: 0=X, 1=Y, 2=Z
            switch (VERTICAL_ACCEL_AXIS) {
                case 0: verticalAccelMs2 = accelData.accX; break;
                case 1: verticalAccelMs2 = accelData.accY; break;
                case 2: verticalAccelMs2 = accelData.accZ; break;
                default: verticalAccelMs2 = accelData.accZ; break;
            }
        }

        // GPS (atualiza sempre que há dados disponíveis)
        gps.update();
        gpsData = gps.getLatestData();

        // ---- 2. Atualizar máquina de estados ----
        fsm.update(currentAltitude, verticalVelocity, verticalAccelMs2);

        // ---- 3. Verificar transição de estado ----
        if (fsm.hasStateChanged()) {
            FlightState newState = fsm.getCurrentState();
            FlightState oldState = fsm.getPreviousState();

            Serial.print(F("[MAIN] Estado: "));
            Serial.print(FlightStateUtils::toString(oldState));
            Serial.print(F(" → "));
            Serial.println(FlightStateUtils::toString(newState));

            // Ações de saída do estado anterior
            _onStateExit(oldState);

            // Ações de entrada no novo estado
            _onStateEnter(newState);

            // Reconfigurar periféricos
            _configurePeripheralsForState(newState);
        }

        // ---- 4. Gravação de dados ----
        unsigned long flightTimeMs = 0;
        if (fsm.getFlightStartTime() > 0) {
            flightTimeMs = millis() - fsm.getFlightStartTime();
        }

        dataLogger.update(flightTimeMs, currentAltitude, verticalVelocity,
                          accelData, gpsData, fsm.getCurrentState());

        // TODO: EEPROM recording com Timer separado conforme estado
        // TODO: Telemetria LoRa
    }

    // ---- 5. Atualizar periféricos de feedback (non-blocking) ----
    buzzer.update();
    led.update();
}

// ========== AÇÕES DE ENTRADA/SAÍDA DE ESTADO ==========

/**
 * Ações executadas ao ENTRAR em um novo estado.
 */
void _onStateEnter(FlightState state) {
    switch (state) {
        case FlightState::ASCENT:
            gps.setEnabled(true);  // Ativar GPS a partir do ASCENT
            Serial.println(F("[MAIN] Voo iniciado! GPS ativado."));
            break;

        case FlightState::ACTIVATE_SKIB_ONE:
            recovery.activateSkibOne();
            Serial.println(F("[MAIN] SKIB 1 ativado (drogue)"));
            break;

        case FlightState::RECOVERY_STAGE_ONE:
            recovery.deactivateSkibOne();
            Serial.println(F("[MAIN] SKIB 1 desativado — drogue aberto"));
            break;

        case FlightState::ACTIVATE_SKIB_TWO:
            recovery.activateSkibTwo();
            Serial.println(F("[MAIN] SKIB 2 ativado (principal)"));
            break;

        case FlightState::RECOVERY_STAGE_TWO:
            recovery.deactivateSkibTwo();
            Serial.println(F("[MAIN] SKIB 2 desativado — principal aberto"));
            break;

        case FlightState::LANDED:
            Serial.println(F("[MAIN] POUSO DETECTADO! Gravação encerrada."));
            break;

        default:
            break;
    }
}

/**
 * Ações executadas ao SAIR de um estado.
 */
void _onStateExit(FlightState state) {
    // Reservado para limpeza futura se necessário
    (void)state;
}

// ========== CONFIGURAÇÃO DE PERIFÉRICOS POR ESTADO ==========

/**
 * Configura LED, Buzzer e GPS conforme o estado atual.
 * Chamada uma vez a cada transição de estado.
 */
void _configurePeripheralsForState(FlightState state) {
    switch (state) {

        case FlightState::PRE_FLIGHT:
            // LED: Azul, blink 1s
            led.setColor(0, 0, 255);
            led.setBlinkInterval(LED_BLINK_PRE_FLIGHT_MS);
            led.setBlinkEnabled(true);
            // Buzzer: 1500Hz, beep a cada 3s por 400ms
            buzzer.setBeepPattern(BEEP_PERIOD_PRE_FLIGHT_MS, BEEP_DURATION_MS, BEEP_FREQ_PRE_FLIGHT);
            break;

        case FlightState::ASCENT:
            // LED: Ciano, blink rápido 250ms
            led.setColor(0, 255, 255);
            led.setBlinkInterval(LED_BLINK_ASCENT_MS);
            led.setBlinkEnabled(true);
            // Buzzer: 2093Hz, beep a cada 1s por 400ms
            buzzer.setBeepPattern(BEEP_PERIOD_ASCENT_MS, BEEP_DURATION_MS, BEEP_FREQ_ASCENT);
            break;

        case FlightState::ACTIVATE_SKIB_ONE:
            // LED: Vermelho sólido (emergência)
            led.setBlinkEnabled(false);
            led.setColor(255, 0, 0);
            // Buzzer: 3136Hz contínuo (gerenciado por playEmergency no RecoverySystem)
            buzzer.setBeepPattern(0, 0, 0);  // Desabilita pattern normal
            break;

        case FlightState::RECOVERY_STAGE_ONE:
            // LED: Laranja, blink 1.5s
            led.setColor(255, 100, 0);
            led.setBlinkInterval(LED_BLINK_RECOVERY_ONE_MS);
            led.setBlinkEnabled(true);
            // Buzzer: desativado
            buzzer.setBeepPattern(0, 0, 0);
            break;

        case FlightState::ACTIVATE_SKIB_TWO:
            // LED: Vermelho sólido (emergência)
            led.setBlinkEnabled(false);
            led.setColor(255, 0, 0);
            // Buzzer: 3136Hz contínuo (gerenciado por playEmergency no RecoverySystem)
            buzzer.setBeepPattern(0, 0, 0);
            break;

        case FlightState::RECOVERY_STAGE_TWO:
            // LED: Amarelo, blink 1.5s
            led.setColor(255, 255, 0);
            led.setBlinkInterval(LED_BLINK_RECOVERY_TWO_MS);
            led.setBlinkEnabled(true);
            // Buzzer: desativado
            buzzer.setBeepPattern(0, 0, 0);
            break;

        case FlightState::LANDED:
            // LED: Verde sólido (pouso seguro)
            led.setBlinkEnabled(false);
            led.setColor(0, 255, 0);
            // Buzzer: desativado
            buzzer.setBeepPattern(0, 0, 0);
            buzzer.noTone();
            break;
    }
}
