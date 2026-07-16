#include "config/Constants.h"
#include "hardwareParts/Altimeter.h"
#include "hardwareParts/Accelerometer.h"
#include "hardwareParts/Buzzer.h"
#include "hardwareParts/SDCard.h"
//#include "hardwareParts/LoRaRadio.h"
#include "hardwareParts/GPS.h"
#include "hardwareParts/LED_RGB.h"

#include "logic/DataLogger.h"

#include "utils/Timer.h"

//Instanciando os objetos das classes no Hardware
Altimeter altimeter;
Accelerometer accelerometer;
Buzzer buzzer(PIN_BUZZER);
SDCard sdCard(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
//LoRaRadio lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1, PIN_LORA_AUX);
//LoRaPayload payload;
LED_RGB led(PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
GPSSensor gps(PIN_GPS_RX, PIN_GPS_TX);

DataLogger dataLogger(&sdCard);

FlightStateMachine stateMachine;

Timer sensorsTimer(SENSOR_READ_INTERVAL_MS);

// ===== SETUP =====
void setup() {

    Serial.begin(115200);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    // Indica boot
    led.begin();
    led.setColor(0, 0, 255);
    led.setBlinkEnabled(false);

    delay(3000);

    // Inicializar buzzer (configura canal LEDC)
    buzzer.begin();

    // bool accOk = accelerometer.begin(); // Inicializar acelerômetro (CRÍTICO)
    // bool altOk = altimeter.begin();     // Inicializar altimetro (CRÍTICO)

    bool accOk = true;
    bool altOk = true;

    if (!accOk || !altOk) {
        if (!accOk && !altOk) { 
            led.setColor(255, 0, 0); // Vermelho
            Serial.println(F("AVISO: Altímetro e Acelerometro falharam — interrompendo inicialização"));
        }

        if (!accOk && altOk || accOk && !altOk) { 
            led.setColor(255, 0, 255); // Magenta
            if (!accOk) Serial.println(F("AVISO: Acelerometro falhou — interrompendo inicialização"));
            if (!altOk) Serial.println(F("AVISO: Altímetro falhou — interrompendo inicialização"));
        }
        
        led.setBlinkInterval(500);
        led.setBlinkEnabled(true);
        while(1) led.update();
    }

    // Inicialização de componentes não-críticos
    gps.begin(9600);

    bool sdOk = sdCard.begin();

    if (!sdOk) {
        led.setColor(0, 0, 0); // Amarelo - houve falha
        // TODO: Som de buzzer específico para indicar falha
    } else {
        led.setColor(0, 255, 0); // Verde - boot ok
        // TODO: Som de buzzer específico para indicar sucesso
    }

    delay(1000);

    //altimeter.resetBaseline();
 
    dataLogger.begin();

    Serial.println(F("Sistema pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop() {
    // Leitura dos sensores a 20 Hz
    // if (sensorsTimer.isReady()) {
    //     double pressure = altimeter.readPressure();
    //     double rawAltitude = altimeter.calculateAltitude();
    //     AccelData accel = accelerometer.readAcceleration();

    //     // FlightState oldState = stateMachine.getCurrentState();
    //     // stateMachine.update(rawAltitude, 0, accel.accZ);
    //     // FlightState newState = stateMachine.getCurrentState();
    // }

    led.update();
    buzzer.update();
}