#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "../hardware/SDCard.h"
#include "../hardware/Accelerometer.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"

class DataLogger {
public:
    DataLogger(SDCard* sdCard);
    void begin();

    void update(unsigned long flightTimeMs, double altitude, const AccelData& accel, FlightState state);

    void setEnabled(bool enabled);

private:
    SDCard* _sdCard;
    bool _enabled;
    Timer _timer;

    String _formatLine(unsigned long flightTimeMs, double altitude, const AccelData& accel, FlightState state);
};

#endif
