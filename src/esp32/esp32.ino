// #include "config/Constants.h"
#include "hardwareParts/Accelerometer.h"

Accelerometer MPU;

// ===== SETUP =====
void setup() {

    Serial.begin(115200);
    delay(3000);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    bool accOk = MPU.begin();

    if (!accOk) {
        Serial.println("Deu problema");
        while(1);
    }

    Serial.println("Acelerometro ok!");

}

// ===== LOOP PRINCIPAL =====
void loop() {
    AccelData mpuData= MPU.readAcceleration();

    MPU.printData(mpuData);
}
