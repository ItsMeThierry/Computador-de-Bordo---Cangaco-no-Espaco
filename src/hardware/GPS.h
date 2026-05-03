#ifndef GPS_SENSOR_H
#define GPS_SENSOR_H

#include <Arduino.h>
#include <TinyGPS++.h>

struct GPSData {
    double latitude;
    double longitude;
    double altitude;
    double speed;
    uint8_t fixQuality;
    bool isValid;
};

class GPSSensor {
public:
    GPSSensor(int rxPin, int txPin);
    void begin();
    void update();                 // Chamar no loop() a cada iteração (não condicional)
    GPSData getLatestData() const;
    bool hasFix() const;

private:
    int _rxPin, _txPin;
    TinyGPSPlus _gps;
    GPSData _latestData;
    unsigned long _lastValidTime;

    void _parseData();
};

#endif