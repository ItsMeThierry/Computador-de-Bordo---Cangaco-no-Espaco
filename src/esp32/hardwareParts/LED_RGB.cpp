#include "LED_RGB.h"

#define PWM_FREQ 5000
#define PWM_RESOLUTION 8 

LED_RGB :: LED_RGB(int pinRed, int pinBlue, int pinGreen, int channelR, int channelG, int channelB){
    _pinR = pinRed;
    _pinB = pinBlue;
    _pinG = pintGreen;

    _channelR = channelR;
    _channelB = channelB;
    _channelG = channelG;

    _blinkInterval = 1000;
    _lastToggle = 0;
    _blinkEnabled = false;
    _state = false;
    _forceMode = false;
    
    _targetR = 0;
    _targetG = 0;
    _targetB = 0;

    _forcedR = 0;
    _forcedG = 0;
    _forcedB = 0;

}

void LED_RGB :: begin(){

    ledcSetup(_channelR, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(_channelG, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(_channelB, PWM_FREQ, PWM_RESOLUTION);

    ledcAttachPin(_pinR, _channelR);
    ledcAttachPin(_pinG, _channelG);
    ledcAttachPin(_pinB, _channelB);

    setColor(0, 0, 0);
}

void LED_RGB::_writePWM(int pin, int channel, uint8_t value) {
 
    ledcWrite(channel, value);
}

void LED_RGB::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    _targetR = red;
    _targetG = green;
    _targetB = blue;

    if (!_blinkEnabled && !_forceMode) {
        _writePWM(_pinR, _channelR, _targetR);
        _writePWM(_pinG, _channelG, _targetG);
        _writePWM(_pinB, _channelB, _targetB);
        _state = true;
    }
}

void LED_RGB::setBlinkInterval(unsigned long intervalMs) {
    _blinkInterval = intervalMs;
}

void LED_RGB::setBlinkEnabled(bool enabled) {
    _blinkEnabled = enabled;
    if (!enabled && !_forceMode) {
        
        _writePWM(_pinR, _channelR, _targetR);
        _writePWM(_pinG, _channelG, _targetG);
        _writePWM(_pinB, _channelB, _targetB);
        _state = true;
    }
}

void LED_RGB::forceColor(bool force, uint8_t r, uint8_t g, uint8_t b) {
    _forceMode = force;
    if (force) {
        _forcedR = r;
        _forcedG = g;
        _forcedB = b;
              
        _writePWM(_pinR, _channelR, _forcedR);
        _writePWM(_pinG, _channelG, _forcedG);
        _writePWM(_pinB, _channelB, _forcedB);
    } else {
        
        _state = false; 
    }
}

void LED_RGB::update() {

    if (_forceMode) return;

    if (!_blinkEnabled) return;

    unsigned long currentMillis = millis();

    if (currentMillis - _lastToggle >= _blinkInterval) {
        _lastToggle = currentMillis;
        _state = !_state; 

        if (_state) {
            _writePWM(_pinR, _channelR, _targetR);
            _writePWM(_pinG, _channelG, _targetG);
            _writePWM(_pinB, _channelB, _targetB);
        } else {
            _writePWM(_pinR, _channelR, 0);
            _writePWM(_pinG, _channelG, 0);
            _writePWM(_pinB, _channelB, 0);
        }
    }
}





