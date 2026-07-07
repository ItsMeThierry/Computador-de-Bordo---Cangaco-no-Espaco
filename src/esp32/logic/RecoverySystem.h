#ifndef RECOVERY_SYSTEM_H
#define RECOVERY_SYSTEM_H

#include <Arduino.h>
#include "../hardwareParts/Buzzer.h"
#include "../config/Constants.h"

/**
 * RecoverySystem — Gerencia dois SKIBs (separadores pirotécnicos):
 *   SKIB 1: Paraquedas drogue (abertura no apogeu)
 *   SKIB 2: Paraquedas principal (abertura em altitude segura)
 *
 * A ativação é controlada pela FlightStateMachine.
 * O main loop chama activate/deactivate conforme transições de estado.
 */
class RecoverySystem {
public:
    RecoverySystem(int skibPin1, int skibPin2, Buzzer* buzzer)
        : _skibPin1(skibPin1),
          _skibPin2(skibPin2),
          _buzzer(buzzer),
          _skib1Active(false),
          _skib2Active(false) {}

    // DEVE ser a primeira instrução do setup() — garante SKIBs desligados antes de tudo
    void begin() {
        pinMode(_skibPin1, OUTPUT);
        pinMode(_skibPin2, OUTPUT);
        digitalWrite(_skibPin1, LOW);
        digitalWrite(_skibPin2, LOW);

        Serial.println(F("[Recovery] SKIBs inicializados (LOW)"));
        Serial.print(F("[Recovery] SKIB 1 pino: ")); Serial.println(_skibPin1);
        Serial.print(F("[Recovery] SKIB 2 pino: ")); Serial.println(_skibPin2);
    }

    // ===== SKIB 1 (Drogue) =====

    void activateSkibOne() {
        if (_skib1Active) return;
        digitalWrite(_skibPin1, HIGH);
        _skib1Active = true;
        Serial.println(F("[Recovery] SKIB 1 ATIVADO (drogue)"));

        if (_buzzer) {
            _buzzer->playEmergency(BEEP_FREQ_ACTIVATE_SKIB, SKIB_BUZZER_DURATION_MS);
        }
    }

    void deactivateSkibOne() {
        digitalWrite(_skibPin1, LOW);
        _skib1Active = false;
        Serial.println(F("[Recovery] SKIB 1 desativado"));
    }

    // ===== SKIB 2 (Principal) =====

    void activateSkibTwo() {
        if (_skib2Active) return;
        digitalWrite(_skibPin2, HIGH);
        _skib2Active = true;
        Serial.println(F("[Recovery] SKIB 2 ATIVADO (principal)"));

        if (_buzzer) {
            _buzzer->playEmergency(BEEP_FREQ_ACTIVATE_SKIB, SKIB_BUZZER_DURATION_MS);
        }
    }

    void deactivateSkibTwo() {
        digitalWrite(_skibPin2, LOW);
        _skib2Active = false;
        Serial.println(F("[Recovery] SKIB 2 desativado"));
    }

    // ===== Status =====

    bool isSkib1Active() const { return _skib1Active; }
    bool isSkib2Active() const { return _skib2Active; }

private:
    int _skibPin1;
    int _skibPin2;
    Buzzer* _buzzer;
    bool _skib1Active;
    bool _skib2Active;
};

#endif