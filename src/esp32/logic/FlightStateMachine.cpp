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

#include "FlightStateMachine.h"

// ============================================================================
// Construtor
// ============================================================================

FlightStateMachine::FlightStateMachine()
    : _state(FlightState::PRE_FLIGHT),
      _previousState(FlightState::PRE_FLIGHT),
      _stateChanged(true),  // true no boot para configurar periféricos iniciais
      _flightStartTime(0),
      _stateEntryTime(0)
{
}

// ============================================================================
// Atualização principal — chamada a cada ciclo de leitura dos sensores
// ============================================================================

void FlightStateMachine::update(double altitude, float verticalVelocity,
                                 float verticalAccelMs2) {

    switch (_state) {

        // ------------------------------------------------------------------
        // PRE_FLIGHT: Na rampa de lançamento, aguardando decolagem
        //   - LED Azul, Buzzer 1500Hz/3s, GPS desativado, sem gravação
        //   - Transição: accVert >= 2.5g E altitude >= 5m
        // ------------------------------------------------------------------
        case FlightState::PRE_FLIGHT:
            if (_shouldTransitionToAscent(altitude, verticalAccelMs2)) {
                _flightStartTime = millis();
                _transitionTo(FlightState::ASCENT);
            }
            break;

        // ------------------------------------------------------------------
        // ASCENT: Foguete em subida ativa
        //   - LED Ciano, Buzzer 2093Hz/1s, EEPROM 20Hz, SD 100Hz, GPS ativo
        //   - Transição: velVert <= 2.5m/s E -1g <= accVert <= 1g (apogeu)
        // ------------------------------------------------------------------
        case FlightState::ASCENT:
            if (_shouldTransitionToSkibOne(verticalVelocity, verticalAccelMs2)) {
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
            if (_shouldTransitionToRecoveryOne()) {
                _transitionTo(FlightState::RECOVERY_STAGE_ONE);
            }
            break;

        // ------------------------------------------------------------------
        // RECOVERY_STAGE_ONE: Descida com drogue aberto
        //   - LED Laranja, sem buzzer, EEPROM 5Hz, SD 100Hz
        //   - Transição: velVert <= -8m/s OU altitude <= threshold
        // ------------------------------------------------------------------
        case FlightState::RECOVERY_STAGE_ONE:
            if (_shouldTransitionToSkibTwo(verticalVelocity, altitude)) {
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
            if (_shouldTransitionToRecoveryTwo()) {
                _transitionTo(FlightState::RECOVERY_STAGE_TWO);
            }
            break;

        // ------------------------------------------------------------------
        // RECOVERY_STAGE_TWO: Descida com paraquedas principal aberto
        //   - LED Amarelo, sem buzzer, EEPROM 5Hz, SD 100Hz, GPS ativo
        //   - Transição: altitude <= 5m E |velVert| <= 0.5m/s
        // ------------------------------------------------------------------
        case FlightState::RECOVERY_STAGE_TWO:
            if (_shouldTransitionToLanded(altitude, verticalVelocity)) {
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

// ============================================================================
// Getters públicos
// ============================================================================

FlightState FlightStateMachine::getCurrentState() const {
    return _state;
}

FlightState FlightStateMachine::getPreviousState() const {
    return _previousState;
}

bool FlightStateMachine::hasStateChanged() {
    if (_stateChanged) {
        _stateChanged = false;
        return true;
    }
    return false;
}

unsigned long FlightStateMachine::getFlightStartTime() const {
    return _flightStartTime;
}

unsigned long FlightStateMachine::getStateEntryTime() const {
    return _stateEntryTime;
}

// ============================================================================
// Forçar estado (testes, reset)
// ============================================================================

void FlightStateMachine::forceState(FlightState newState) {
    _transitionTo(newState);
    Serial.print(F("[FSM] Estado forçado para: "));
    Serial.println(static_cast<uint8_t>(newState));
}

// ============================================================================
// Transição de estado interna
// ============================================================================

void FlightStateMachine::_transitionTo(FlightState newState) {
    _previousState = _state;
    _state = newState;
    _stateChanged = true;
    _stateEntryTime = millis();

    Serial.print(F("[FSM] Transição: "));
    Serial.print(static_cast<uint8_t>(_previousState));
    Serial.print(F(" → "));
    Serial.println(static_cast<uint8_t>(newState));
}

// ============================================================================
// Condições de transição
// ============================================================================

/**
 * PRE_FLIGHT → ASCENT
 * Condição: aceleração vertical >= 2.5g E altitude >= 5m
 */
bool FlightStateMachine::_shouldTransitionToAscent(double altitude,
                                                    float verticalAccelMs2) const {
    return (verticalAccelMs2 >= ASCENT_ACCEL_THRESHOLD_MS2) &&
           (altitude >= ASCENT_ALTITUDE_THRESHOLD_M);
}

/**
 * ASCENT → ACTIVATE_SKIB_ONE
 * Condição: velocidade vertical <= 2.5 m/s E -1g <= aceleração <= 1g
 * Indica que o foguete atingiu o apogeu (velocidade ~0, aceleração ~0/queda livre)
 */
bool FlightStateMachine::_shouldTransitionToSkibOne(float verticalVelocity,
                                                     float verticalAccelMs2) const {
    bool velocityLow = (verticalVelocity <= APOGEE_VERT_VEL_THRESHOLD_MS);
    bool accelNearZero = (verticalAccelMs2 >= APOGEE_ACCEL_MIN_MS2) &&
                         (verticalAccelMs2 <= APOGEE_ACCEL_MAX_MS2);
    return velocityLow && accelNearZero;
}

/**
 * ACTIVATE_SKIB_ONE → RECOVERY_STAGE_ONE
 * Condição: 2 segundos de ativação do SKIB concluídos
 */
bool FlightStateMachine::_shouldTransitionToRecoveryOne() const {
    return (millis() - _stateEntryTime) >= SKIB_ACTIVATION_DURATION_MS;
}

/**
 * RECOVERY_STAGE_ONE → ACTIVATE_SKIB_TWO
 * Condição: velocidade vertical <= -8 m/s OU altitude <= threshold
 * Indica que é hora de abrir o paraquedas principal
 */
bool FlightStateMachine::_shouldTransitionToSkibTwo(float verticalVelocity,
                                                     double altitude) const {
    return (verticalVelocity <= STAGE_TWO_VERT_VEL_THRESHOLD_MS) ||
           (altitude <= STAGE_TWO_ALT_THRESHOLD_M);
}

/**
 * ACTIVATE_SKIB_TWO → RECOVERY_STAGE_TWO
 * Condição: 2 segundos de ativação do SKIB concluídos
 */
bool FlightStateMachine::_shouldTransitionToRecoveryTwo() const {
    return (millis() - _stateEntryTime) >= SKIB_ACTIVATION_DURATION_MS;
}

/**
 * RECOVERY_STAGE_TWO → LANDED
 * Condição: altitude <= 5m E |velocidade vertical| <= 0.5 m/s
 */
bool FlightStateMachine::_shouldTransitionToLanded(double altitude,
                                                    float verticalVelocity) const {
    return (altitude <= LANDED_ALT_THRESHOLD_M) &&
           (fabs(verticalVelocity) <= LANDED_VERT_VEL_THRESHOLD_MS);
}
