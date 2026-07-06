#ifndef FLIGHT_STATE_UTILS_H
#define FLIGHT_STATE_UTILS_H

#include "../logic/FlightStateMachine.h"

namespace FlightStateUtils {

    // Para CSV do SD Card e Serial debug
    inline const char* toString(FlightState state) {
        switch(state) {
            case FlightState::PRE_FLIGHT:         return "PRE_FLIGHT";
            case FlightState::ASCENT:             return "ASCENT";
            case FlightState::ACTIVATE_SKIB_ONE:  return "SKIB_ONE";
            case FlightState::RECOVERY_STAGE_ONE: return "RECOVERY_1";
            case FlightState::ACTIVATE_SKIB_TWO:  return "SKIB_TWO";
            case FlightState::RECOVERY_STAGE_TWO: return "RECOVERY_2";
            case FlightState::LANDED:             return "LANDED";
            default:                              return "UNKNOWN";
        }
    }

    // Para campo numérico do pacote LoRa: 0–6
    inline uint8_t toInt(FlightState state) {
        return static_cast<uint8_t>(state);
    }
}

#endif