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
    FlightStateMachine();

    /**
     * Atualiza a máquina de estados com os dados de voo atuais.
     *
     * @param altitude          Altitude suavizada relativa ao solo (m)
     * @param verticalVelocity  Velocidade vertical derivada da altitude (m/s, positivo = subindo)
     * @param verticalAccelMs2  Aceleração no eixo vertical do MPU6050 (m/s²)
     */
    void update(double altitude, float verticalVelocity, float verticalAccelMs2);

    FlightState getCurrentState() const;
    FlightState getPreviousState() const;

    // Retorna true uma única vez após cada transição de estado
    bool hasStateChanged();

    void forceState(FlightState newState);
    unsigned long getFlightStartTime() const;
    unsigned long getStateEntryTime() const;

private:
    FlightState _state;
    FlightState _previousState;
    bool _stateChanged;
    unsigned long _flightStartTime;
    unsigned long _stateEntryTime;  // millis() da entrada no estado atual

    void _transitionTo(FlightState newState);

    bool _shouldTransitionToAscent(double altitude, float verticalAccelMs2) const;
    bool _shouldTransitionToSkibOne(float verticalVelocity, float verticalAccelMs2) const;
    bool _shouldTransitionToRecoveryOne() const;
    bool _shouldTransitionToSkibTwo(float verticalVelocity, double altitude) const;
    bool _shouldTransitionToRecoveryTwo() const;
    bool _shouldTransitionToLanded(double altitude, float verticalVelocity) const;
};

#endif