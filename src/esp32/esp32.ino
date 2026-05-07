#include "config/Constants.h"
#include "hardwareParts/Accelerometer.h"
#include "hardwareParts/Buzzer.h"
#include "hardwareParts/SDCard.h"

//Instanciando os objetos das classes no Hardware
Accelerometer MPU;
Buzzer buzzer(PIN_BUZZER);
SDCard sdCard(PIN_SD_CS);

// ===== SETUP =====
void setup() {

    Serial.begin(115200);
    delay(3000);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    // Inicializar buzzer (configura canal LEDC)
    buzzer.begin();

    // Inicializar SD Card
    bool sdOk = sdCard.begin();

    // Inicializar acelerômetro
    bool accOk = MPU.begin();

    if (!accOk) {
        Serial.println(F("Erro: Acelerometro falhou"));
        while(1);
    }
    Serial.println(F("Acelerometro ok!"));

    if (!sdOk) {
        Serial.println(F("Erro: SD Card falhou"));
        while(1);
    }
    Serial.println(F("SD Card ok!"));

    Serial.println(F("Sistema pronto!"));

}

// ===== LOOP PRINCIPAL =====
void loop() {
    AccelData mpuData= MPU.readAcceleration();

    MPU.printData(mpuData);
}
