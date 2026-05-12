#include "config/Constants.h"
#include "hardwareParts/Accelerometer.h"
#include "hardwareParts/Buzzer.h"
//#include "hardwareParts/SDCard.h"
#include "hardwareParts/LoRaRadio.h"

//Instanciando os objetos das classes no Hardware
Accelerometer MPU;
Buzzer buzzer(PIN_BUZZER);
//SDCard sdCard(PIN_SD_CS);
LoRaRadio lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1, PIN_LORA_AUX);
LoRaPayload payload;

// ===== SETUP =====
void setup() {

    Serial.begin(115200);
    delay(3000);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    // Inicializar buzzer (configura canal LEDC)
    buzzer.begin();

    // Inicializar SD Card (não-crítico — EEPROM é o armazenamento primário)
    // bool sdOk = sdCard.begin();
    // if (!sdOk) {
    //     Serial.println(F("AVISO: SD Card falhou — continuando sem gravação SD"));
    // } else {
    //     Serial.println(F("SD Card ok!"));
    // }

    // Inicializar acelerômetro (não-crítico — dados complementares)
    bool accOk = MPU.begin();
    if (!accOk) {
        Serial.println(F("AVISO: Acelerometro falhou — continuando sem dados de aceleração"));
    } else {
        Serial.println(F("Acelerometro ok!"));
    }

    lora.begin(9600);
    if (!lora.isReady()) {
        Serial.println(F("AVISO: LoRa falhou - continuando sem comunicação à base de lançamento"));
    } else {
        Serial.println(F("LoRa ok!"));
    }

    // TODO: Inicializar altímetro (CRÍTICO — único sensor que bloqueia o sistema)

    Serial.println(F("Sistema pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop() {
    //AccelData mpuData = MPU.readAcceleration();

    //MPU.printData(mpuData);

    // Atualizar buzzer (gerencia beep pattern não-bloqueante)
    // buzzer.setBeepPattern(500, 400, 3136);
    // buzzer.update();

    payload.message = "Olha a mensagem!";
    payload.time = millis();

    lora.transmit(payload);
 
}

