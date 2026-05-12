#include <SoftwareSerial.h>
#include "EBYTE.h"

#define PIN_TX 5
#define PIN_RX 4
#define PIN_M1 3
#define PIN_M0 2
#define PIN_AX 6

struct LoRaPayload {
  String message;
  unsigned long time;
};


SoftwareSerial ESerial(PIN_RX, PIN_TX);
EBYTE Transceiver(&ESerial, PIN_M0, PIN_M1, PIN_AX);
unsigned long now;
LoRaPayload payload;

void setup() {

  Serial.begin(9600);

  ESerial.begin(9600);
  Serial.println("Starting Reader");

  Transceiver.init();
  Transceiver.SetAddressH(1);
  Transceiver.SetAddressL(1);
  Transceiver.SetChannel(23);
  Transceiver.PrintParameters();

}

void loop() {

  if (ESerial.available()) {
    Transceiver.GetStruct(&payload, sizeof(payload));

    Serial.println(payload.message);
    Serial.print("Tempo: "); Serial.println(payload.time);
    now = millis();
  }
  else {
    if ((millis() - now) > 1000) {
      Serial.println("Searching: ");
      now = millis();
    }
  }

}
