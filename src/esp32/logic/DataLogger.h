#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "../hardwareParts/SDCard.h"
#include "../hardwareParts/Accelerometer.h"
#include "../hardwareParts/GPS.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"

class DataLogger {
public:
    DataLogger(SDCard* sdCard);
    void begin();

    /**
     * Atualiza gravação no SD Card conforme estado de voo.
     * Estados sem gravação: PRE_FLIGHT, LANDED
     * Todos os outros estados: gravação a 100Hz (10ms)
     */
    void update(unsigned long flightTimeMs, double altitude,
                float verticalVelocity, const AccelData& accel,
                const GPSData& gpsData, FlightState state);

    void setEnabled(bool enabled);

private:
    SDCard* _sdCard;
    bool _enabled;
    Timer _timer;

    String _formatLine(unsigned long flightTimeMs, double altitude,
                       float verticalVelocity, const AccelData& accel,
                       const GPSData& gpsData, FlightState state);
};

#endif
