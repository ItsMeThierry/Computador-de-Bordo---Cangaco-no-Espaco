#ifndef ALTIMETER_H
#define ALTIMETER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

class Altimeter {
public:
    Altimeter();
    bool begin();
    bool isConnected() const;
    double readPressure();
    double calculateAltitude(double pressure, double baseline);
    double getBaseline() const;
    void resetBaseline();

private:
    Adafruit_BMP280 _sensor;
    bool _connected;
    double _baseline;
};

#endif
