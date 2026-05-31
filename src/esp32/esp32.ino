#include "config/Constants.h"
//#include "hardwareParts/Accelerometer.h"
#include "hardwareParts/Buzzer.h"
//#include "hardwareParts/SDCard.h"
#include "hardwareParts/LoRaRadio.h"
#include "hardwareparts/GPS.h"

//Instanciando os objetos das classes no Hardware
Accelerometer MPU;
Buzzer buzzer(PIN_BUZZER);
//SDCard sdCard(PIN_SD_CS);
LoRaRadio lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1, PIN_LORA_AUX);
LoRaPayload payload;

// GPS NEO-6M
GPSSensor gps(PIN_GPS_RX, PIN_GPS_TX);
float lastGpsPrint;

// ===== SETUP =====
void setup() {

    Serial.begin(115200);
    delay(3000);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    // Indica boot
    led.begin();
    led.setColor(0, 0, 255);
    led.setBlinkEnabled(false);

    // Inicializar buzzer (configura canal LEDC)
    buzzer.begin();
    
    bool sdOk = sdCard.begin(); // Inicializar SD Card (não-crítico — EEPROM é o armazenamento primário)
    bool accOk = MPU.begin(); // Inicializar acelerômetro (não-crítico — dados complementares)

    if (!sdOk) {
        Serial.println(F("AVISO: SD Card falhou — continuando sem gravação SD"));
    } else {
        Serial.println(F("SD Card ok!"));
    }

    // Inicializar acelerômetro (não-crítico — dados complementares)
    bool accOk = MPU.begin();
    if (!accOk) {
        Serial.println(F("AVISO: Acelerometro falhou — continuando sem dados de aceleração"));
    } else {
        Serial.println(F("Acelerometro ok!"));
    }

    // lora.begin(9600);
    // if (!lora.isReady()) {
    //     Serial.println(F("AVISO: LoRa falhou - continuando sem comunicação à base de lançamento"));
    // } else {
    //     Serial.println(F("LoRa ok!"));
    // }

    // TODO: Inicializar altímetro (CRÍTICO — único sensor que bloqueia o sistema)

    // Inicializar GPS
    gps.begin(9600); // NEO-6M geralmente trabalha em 9600 baud

    // TODO: Calibrar baseline

    Serial.println(F("Sistema pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop() {

    // Debug do GPS a cada 1 segundo
    gps.update();
    if (millis() - lastGpsPrint >= 1000) {
        lastGpsPrint = millis();

        if (gps.hasFix()) {
            GPSData gpsData = gps.getLatestData();

            Serial.println(F("===== GPS ====="));

            Serial.print(F("Latitude: "));
            Serial.println(gpsData.latitude, 6);

            Serial.print(F("Longitude: "));
            Serial.println(gpsData.longitude, 6);

            Serial.print(F("Altitude: "));
            Serial.print(gpsData.altitude);
            Serial.println(F(" m"));

            Serial.print(F("Velocidade: "));
            Serial.print(gpsData.speed);
            Serial.println(F(" km/h"));

            Serial.print(F("Fix Quality: "));
            Serial.println(gpsData.fixQuality);

            Serial.println(F("================"));
        } else {
            Serial.println(F("GPS sem fix..."));
        }
    }

}

