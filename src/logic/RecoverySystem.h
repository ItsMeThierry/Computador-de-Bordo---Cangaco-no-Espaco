#ifndef RECOVERY_SYSTEM_H
#define RECOVERY_SYSTEM_H

#include <Arduino.h>
#include "../hardwareParts/Buzzer.h"
#include "../utils/Timer.h"

class RecoverySystem {
public:
    RecoverySystem(unsigned long skibActivationTime, int skibPin, Buzzer* buzzer) 
        : _skibPin(skibPin),
        _buzzer(buzzer),
        _activated(false),
        _skibActive(false),
        _activationTimer(skibActivationTime) {}    

    // Primeira instrução do setup() — GPIO LOW antes de tudo
    void begin() {
        pinMode(_skibPin, OUTPUT);
        digitalWrite(_skibPin, LOW);
    }

    void activate() {
        _activateSkib();
        _activationTimer.reset();

        _buzzer->playEmergency(3136, skibActivationTime); // TODO: Implementar constantes

        _activated = true;
    }

    void update() {
        if(_activationTimer.isReady()) {
            _deactivateSkib();
        }
    }

    bool isActivated() const {
        return _activated;
    }

    bool isSkibActive() const {
        return _skibActive;
    }

private:
    int _skibPin;
    Buzzer* _buzzer;
    Timer _activationTimer;
    bool _activated;
    bool _skibActive;

    void _activateSkib() {
        digitalWrite(_skibPin, HIGH);
        _skibActive = true;
    }

    void _deactivateSkib() {
        digitalWrite(_skibPin, LOW);
        _skibActive = false;
    }
};

#endif