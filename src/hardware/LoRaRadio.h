#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>

class LoRaRadio {
public:
    LoRaRadio(int rxPin, int txPin, int m0Pin, int m1Pin);
    void begin();
    void setModeNormal();
    bool transmit(const String& message);
    bool isReady() const;

private:
    int _rxPin, _txPin, _m0Pin, _m1Pin;
    bool _initialized;

    void _setMode(uint8_t m0, uint8_t m1);
};

#endif