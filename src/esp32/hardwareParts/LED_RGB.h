#ifndef LED_RGB_H
#define LED_RGB_H

#include <Arduino.h>

class LED_RGB {
public:
    LED_RGB(int pinRed, int pinGreen, int pinBlue);
    void begin();
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    void setBlinkInterval(unsigned long intervalMs);
    void update();
    void setBlinkEnabled(bool enabled);
    void forceColor(bool force, uint8_t r, uint8_t g, uint8_t b);

private:
    int _pinR, _pinG, _pinB;
    int _channelR, _channelG, _channelB;
    unsigned long _blinkInterval;
    unsigned long _lastToggle;
    bool _blinkEnabled;
    bool _state;
    uint8_t _targetR, _targetG, _targetB;
    uint8_t _forcedR, _forcedG, _forcedB;
    bool _forceMode;

    void _writePWM(int channel, uint8_t value);
};

#endif