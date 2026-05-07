#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>
#include <EBYTE.h>

class LoRaRadio {
public:
    LoRaRadio(int rxPin, int txPin, int m0Pin, int m1Pin, int auxPin);
    void begin(int baudrate);
    // void setMode();
    void setModeNormal();
    bool transmit(const String& message);
    bool isReady() const;

private:
    int _rxPin, _txPin, _m0Pin, _m1Pin, _auxPin;
    EBYTE _lora;
    bool _initialized;
};

#endif