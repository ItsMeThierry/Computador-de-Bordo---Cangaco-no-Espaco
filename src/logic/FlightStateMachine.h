/**
* FlightStateMachine.cpp
*
* Máquina de 7 estados com transições unidirecionais:
*
*   PRE_FLIGHT → ASCENT → ACTIVATE_SKIB_ONE → RECOVERY_STAGE_ONE
*   → ACTIVATE_SKIB_TWO → RECOVERY_STAGE_TWO → LANDED
*
* Transições:
*   PRE_FLIGHT → ASCENT:            accVert >= 2.5g  E  altitude >= 5m
*   ASCENT → ACTIVATE_SKIB_ONE:     velVert <= 2.5m/s  E  -1g <= accVert <= 1g
*   ACTIVATE_SKIB_ONE → REC_ONE:    2s de ativação de SKIB
*   REC_ONE → ACTIVATE_SKIB_TWO:    velVert <= -8m/s  OU  altitude <= threshold
*   ACTIVATE_SKIB_TWO → REC_TWO:    2s de ativação de SKIB
*   REC_TWO → LANDED:               altitude <= 5m  E  |velVert| <= 0.5m/s
*   LANDED:                         Estado terminal — sem transição
*/


#ifndef FLIGHT_STATE_MACHINE_H
#define FLIGHT_STATE_MACHINE_H

#include <Arduino.h>
#include "../config/Constants.h"

/**
* Enum de estados de voo — 7 estados com transições unidirecionais:
*
*   PRE_FLIGHT → ASCENT → ACTIVATE_SKIB_ONE → RECOVERY_STAGE_ONE
*   → ACTIVATE_SKIB_TWO → RECOVERY_STAGE_TWO → LANDED
*/
enum class FlightState : uint8_t {
    PRE_FLIGHT         = 0,
    ASCENT             = 1,
    ACTIVATE_SKIB_ONE  = 2,
    RECOVERY_STAGE_ONE = 3,
    ACTIVATE_SKIB_TWO  = 4,
    RECOVERY_STAGE_TWO = 5,
    LANDED             = 6
};

class FlightStateMachine {
public:
    FlightStateMachine() 
        : _state(FlightState::PRE_FLIGHT),
        _previousState(FlightState::PRE_FLIGHT),
        _stateChanged(true),  // true no boot para configurar periféricos iniciais
        _flightStartTime(0),
        _stateEntryTime(0) {}

    /**
     * Atualiza a máquina de estados com os dados de voo atuais.
     *
     * @param altitude          Altitude suavizada relativa ao solo (m)
     * @param verticalVelocity  Velocidade vertical derivada da altitude (m/s, positivo = subindo)
     * @param verticalAccelMs2  Aceleração no eixo vertical do MPU6050 (m/s²)
     */
    void update(double altitude, float verticalVelocity, float verticalAccelMs2) {
        switch (_state) {
            // ------------------------------------------------------------------
            // PRE_FLIGHT: Na rampa de lançamento, aguardando decolagem
            //   - LED Ciano, Buzzer 1500Hz/3s, GPS desativado, sem gravação
            //   - Transição: accVert >= 2.5g E altitude >= 5m
            // ------------------------------------------------------------------
            case FlightState::PRE_FLIGHT:
                if ((verticalAccelMs2 >= ASCENT_ACCEL_THRESHOLD_MS2) && 
                    (altitude >= ASCENT_ALTITUDE_THRESHOLD_M)) {
                    _flightStartTime = millis();
                    _transitionTo(FlightState::ASCENT);
                }
                break;

            // ------------------------------------------------------------------
            // ASCENT: Foguete em subida ativa
            //   - LED Branco, Buzzer 2093Hz/1s, EEPROM 20Hz, SD 100Hz, GPS ativo
            //   - Transição: velVert <= 2.5m/s E -1g <= accVert <= 1g (apogeu)
            // ------------------------------------------------------------------
            case FlightState::ASCENT:
                if ((verticalVelocity <= APOGEE_VERT_VEL_THRESHOLD_MS) && 
                    ((verticalAccelMs2 >= APOGEE_ACCEL_MIN_MS2) && 
                    (verticalAccelMs2 <= APOGEE_ACCEL_MAX_MS2))) {
                    _transitionTo(FlightState::ACTIVATE_SKIB_ONE);
                }
                break;

            // ------------------------------------------------------------------
            // ACTIVATE_SKIB_ONE: Ativação do paraquedas drogue
            //   - LED Vermelho, Buzzer 3136Hz contínuo, EEPROM 5Hz, SD 100Hz
            //   - SKIB 1 ativado (GPIO HIGH)
            //   - Transição: 2s de ativação concluída
            // ------------------------------------------------------------------
            case FlightState::ACTIVATE_SKIB_ONE:
                if ((millis() - _stateEntryTime) >= SKIB_ACTIVATION_DURATION_MS) {
                    _transitionTo(FlightState::RECOVERY_STAGE_ONE);
                }
                break;

            // ------------------------------------------------------------------
            // RECOVERY_STAGE_ONE: Descida com drogue aberto
            //   - LED Laranja, sem buzzer, EEPROM 5Hz, SD 100Hz
            //   - Transição: velVert <= -8m/s OU altitude <= threshold
            // ------------------------------------------------------------------
            case FlightState::RECOVERY_STAGE_ONE:
                if ((verticalVelocity <= STAGE_TWO_VERT_VEL_THRESHOLD_MS) ||
                    (altitude <= STAGE_TWO_ALT_THRESHOLD_M)) {
                    _transitionTo(FlightState::ACTIVATE_SKIB_TWO);
                }
                break;

            // ------------------------------------------------------------------
            // ACTIVATE_SKIB_TWO: Ativação do paraquedas principal
            //   - LED Vermelho, Buzzer 3136Hz contínuo, EEPROM 5Hz, SD 100Hz
            //   - SKIB 2 ativado (GPIO HIGH)
            //   - Transição: 2s de ativação concluída
            // ------------------------------------------------------------------
            case FlightState::ACTIVATE_SKIB_TWO:
                if ((millis() - _stateEntryTime) >= SKIB_ACTIVATION_DURATION_MS) {
                    _transitionTo(FlightState::RECOVERY_STAGE_TWO);
                }
                break;

            // ------------------------------------------------------------------
            // RECOVERY_STAGE_TWO: Descida com paraquedas principal aberto
            //   - LED Laranja, sem buzzer, EEPROM 5Hz, SD 100Hz, GPS ativo
            //   - Transição: altitude <= 5m E |velVert| <= 0.5m/s
            // ------------------------------------------------------------------
            case FlightState::RECOVERY_STAGE_TWO:
                if ((altitude <= LANDED_ALT_THRESHOLD_M) &&
                    (fabs(verticalVelocity) <= LANDED_VERT_VEL_THRESHOLD_MS)) {
                    _transitionTo(FlightState::LANDED);
                }
                break;

            // ------------------------------------------------------------------
            // LANDED: Estado terminal — foguete pousou
            //   - LED Verde, sem gravação, GPS ativo (para localização)
            // ------------------------------------------------------------------
            case FlightState::LANDED:
                // Estado terminal — nenhuma transição possível
                break;
        }
    }

    FlightState getCurrentState() const {
        return _state;
    }

    FlightState getPreviousState() const {
        return _previousState;
    }

    // Retorna true uma única vez após cada transição de estado
    bool hasStateChanged() {
        if (_stateChanged) {
            _stateChanged = false;
            return true;
        }
        return false;
    }

    void forceState(FlightState newState) {
        _transitionTo(newState);
        Serial.print(F("[FSM] Estado forçado para: "));
        Serial.println(static_cast<uint8_t>(newState));
    }

    unsigned long getFlightStartTime() const {
        return _flightStartTime;
    }

    unsigned long getStateEntryTime() const {
        return _stateEntryTime;
    }

private:
    FlightState _state;
    FlightState _previousState;
    bool _stateChanged;
    unsigned long _flightStartTime;
    unsigned long _stateEntryTime;  // millis() da entrada no estado atual

    void _transitionTo(FlightState newState) {
        _previousState = _state;
        _state = newState;
        _stateChanged = true;
        _stateEntryTime = millis();

        Serial.print(F("[FSM] Transição: "));
        Serial.print(static_cast<uint8_t>(_previousState));
        Serial.print(F(" → "));
        Serial.println(static_cast<uint8_t>(newState));
    }

};

#endif
