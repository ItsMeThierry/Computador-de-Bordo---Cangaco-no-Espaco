#include "../../logic/DataStorage.h"

DataStorage dataStorage;

unsigned long lastSerialHeartbeatMs = 0;

void saveFakeEepromSamples()
{
    for (int i = 0; i < 20; i++)
    {
        const FlightState fakeState = static_cast<FlightState>(i % 7);
        const unsigned long fakeTimeMs = i * 50UL;
        const double fakeAltitude = i * 2.5;
        AccelData fakeAccel;
        fakeAccel.accX = i * 0.15f;
        fakeAccel.accY = -i * 0.07f;
        fakeAccel.accZ = 9.81f + (i * 0.03f);
        fakeAccel.gyrX = i * 0.10f;
        fakeAccel.gyrY = -i * 0.05f;
        fakeAccel.gyrZ = i * 0.02f;
        fakeAccel.temp = 25.0f;
        fakeAccel.valid = true;

        dataStorage.saveFlightSample(fakeTimeMs, fakeState, fakeAltitude, fakeAccel);
    }

    dataStorage.flushPendingWrites();
    Serial.println(F("[DataStorage] 20 pontos falsos salvos"));
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
        Serial.print(F("[DataStorage] Backend: "));
        Serial.println(dataStorage.isUsingFlightLogPartition() ? F("flightlog") : F("EEPROM/NVS"));
        Serial.print(F("[DataStorage] Registros: "));
        Serial.print(dataStorage.getRecordCount());
        Serial.print(F("/"));
        Serial.print(dataStorage.getMaxFlightDataPoints());
        Serial.print(F(" | bytes: "));
        Serial.print(dataStorage.getStorageSizeBytes());
        Serial.print(F(" | amostra: "));
        Serial.print(dataStorage.getSampleSizeBytes());
        Serial.print(F(" | dados em: "));
        Serial.print(dataStorage.getDataStartOffset());
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

void printSerialHeartbeat() {
    const unsigned long now = millis();

    if (now - lastSerialHeartbeatMs < 2000) {
        return;
    }

    lastSerialHeartbeatMs = now;
    Serial.println(F("[Serial] Aguardando comando. Use EEPROM_INFO, EEPROM_FAKE_SAVE ou EEPROM_READ"));
}

void handleSerialCommand(){
    if (!Serial.available()){
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
        Serial.print(F("[DataStorage] Backend: "));
        Serial.println(dataStorage.isUsingFlightLogPartition() ? F("flightlog") : F("EEPROM/NVS"));
        Serial.print(F("[DataStorage] Registros: "));
        Serial.print(dataStorage.getRecordCount());
        Serial.print(F("/"));
        Serial.print(dataStorage.getMaxFlightDataPoints());
        Serial.print(F(" | bytes: "));
        Serial.print(dataStorage.getStorageSizeBytes());
        Serial.print(F(" | amostra: "));
        Serial.print(dataStorage.getSampleSizeBytes());
        Serial.print(F(" | dados em: "));
        Serial.print(dataStorage.getDataStartOffset());
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    randomSeed(micros());
    Serial.println(F("Iniciando Teste da EEPROM"));
    dataStorage.begin();
    Serial.println(F("[DataStorage] Comandos: EEPROM_INFO, EEPROM_READ, EEPROM_CLEAR, EEPROM_FAKE_SAVE"));
}

void loop() {
    handleSerialCommand();
    printSerialHeartbeat();
}