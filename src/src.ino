#include "config/Constants.h"
#include "hardwareParts/Altimeter.h"
#include "hardwareParts/Accelerometer.h"
#include "hardwareParts/Buzzer.h"
#include "hardwareParts/SDCard.h"
// #include "hardwareParts/LoRaRadio.h"
#include "hardwareParts/GPS.h"
#include "hardwareParts/LED_RGB.h"

#include "logic/DataLogger.h"
#include "logic/DataStorage.h"

#include "utils/Timer.h"

// Instanciando os objetos das classes no Hardware
Altimeter altimeter;
Accelerometer accelerometer;
Buzzer buzzer(PIN_BUZZER);
SDCard sdCard(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
// LoRaRadio lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1, PIN_LORA_AUX);
LED_RGB led(PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
GPSSensor gps(PIN_GPS_RX, PIN_GPS_TX);

DataLogger dataLogger(&sdCard);
DataStorage dataStorage;

FlightStateMachine stateMachine;

Timer sensorsTimer(SENSOR_READ_INTERVAL_MS);

double lastAltitude = 0.0;
unsigned long lastAltitudeReadMs = 0;
bool hasLastAltitude = false;
unsigned long lastSerialHeartbeatMs = 0;

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

void saveFakeEepromSamples()
{
    for (int i = 0; i < 20; i++)
    {
        AccelData fakeAccel;
        fakeAccel.accX = 0.10f + (i * 0.01f);
        fakeAccel.accY = -0.05f;
        fakeAccel.accZ = 9.80f + (i * 0.02f);
        fakeAccel.gyrX = 0.0f;
        fakeAccel.gyrY = 0.0f;
        fakeAccel.gyrZ = 0.0f;
        fakeAccel.temp = 25.0f;
        fakeAccel.valid = true;

        const unsigned long flightTimeMs = i * 50UL;
        const double altitude = 100.0 + (i * 2.5);
        dataStorage.saveFlightSample(flightTimeMs, altitude, fakeAccel);
    }

    dataStorage.flushPendingWrites();
    Serial.println(F("[DataStorage] 20 amostras falsas salvas"));
}

void handleSerialCommand()
{
    if (!Serial.available())
    {
        return;
    }

    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();

    Serial.print(F("[Serial] Comando recebido: "));
    Serial.println(command);

    if (command == F("EEPROM_READ"))
    {
        dataStorage.flushPendingWrites();
        Serial.println(F("[DataStorage] BEGIN CSV"));
        dataStorage.printFlightLogCsv(Serial);
        Serial.println(F("[DataStorage] END CSV"));
    }
    else if (command == F("EEPROM_INFO"))
    {
        Serial.print(F("[DataStorage] Registros: "));
        Serial.print(dataStorage.getRecordCount());
        Serial.print(F("/"));
        Serial.print(dataStorage.getMaxFlightDataPoints());
        Serial.print(F(" | bytes: "));
        Serial.print(dataStorage.getStorageSizeBytes());
        Serial.print(F(" | fator: 1/"));
        Serial.print(dataStorage.getDecimationFactor());
        Serial.print(F(" | proximo indice: "));
        Serial.print(dataStorage.getOverwriteIndex());
        Serial.print(F(" | pendentes: "));
        Serial.println(dataStorage.getPendingWriteCount());
    }
    else if (command == F("EEPROM_CLEAR"))
    {
        dataStorage.clearFlightLog();
    }
    else if (command == F("EEPROM_FAKE_SAVE"))
    {
        saveFakeEepromSamples();
    }
    else if (command.length() > 0)
    {
        Serial.println(F("[DataStorage] Comandos: EEPROM_INFO, EEPROM_READ, EEPROM_CLEAR, EEPROM_FAKE_SAVE"));
    }
}

void printSerialHeartbeat()
{
    const unsigned long now = millis();
    if (now - lastSerialHeartbeatMs < 2000)
    {
        return;
    }

    lastSerialHeartbeatMs = now;
    Serial.println(F("[Serial] Aguardando comando. Use EEPROM_INFO, EEPROM_FAKE_SAVE ou EEPROM_READ"));
}

// ===== SETUP =====
void setup()
{

    Serial.begin(115200);
    delay(1000);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));
    dataStorage.begin();
    Serial.println(F("[DataStorage] Comandos: EEPROM_INFO, EEPROM_READ, EEPROM_CLEAR, EEPROM_FAKE_SAVE"));

    // Indica boot
    led.begin();
    led.setColor(0, 0, 255);
    led.setBlinkEnabled(false);

    delay(3000);

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
        while (1)
        {
            handleSerialCommand();
            printSerialHeartbeat();
            led.update();
            delay(1);
        }
    }

    // Inicialização de componentes não-críticos
    gps.begin(9600);

    bool sdOk = sdCard.begin();

    if (!sdOk)
    {
        led.setColor(0, 0, 0); // Amarelo - houve falha
        // TODO: Som de buzzer específico para indicar falha
    }
    else
    {
        led.setColor(0, 255, 0); // Verde - boot ok
        // TODO: Som de buzzer específico para indicar sucesso
    }

    delay(1000);

    // altimeter.resetBaseline();

    dataLogger.begin();

    Serial.println(F("Sistema pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop()
{
    handleSerialCommand();
    printSerialHeartbeat();
    gps.update();

    // Leitura dos sensores a 20 Hz
    if (sensorsTimer.isReady())
    {
        const unsigned long now = millis();
        const double pressure = altimeter.readPressure();
        const double altitude = altimeter.calculateAltitude();
        const AccelData accel = accelerometer.readAcceleration();
        const GPSData gpsData = gps.getLatestData();

        float verticalVelocity = 0.0f;

        if (hasLastAltitude)
        {
            const unsigned long dtMs = now - lastAltitudeReadMs;
            if (dtMs > 0)
            {
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
        if (oldState == FlightState::PRE_FLIGHT && currentState == FlightState::ASCENT)
        {
            dataStorage.clearFlightLog();
        }

        const unsigned long flightTimeMs = (currentState == FlightState::PRE_FLIGHT)
                                               ? 0
                                               : now - stateMachine.getFlightStartTime();

        dataLogger.update(flightTimeMs, altitude, pressure, accel, gpsData, currentState);
        dataStorage.update(flightTimeMs, altitude, accel, currentState);
    }

    led.update();
    buzzer.update();
}
