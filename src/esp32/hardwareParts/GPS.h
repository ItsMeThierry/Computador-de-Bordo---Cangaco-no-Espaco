#ifndef GPS_SENSOR_H
#define GPS_SENSOR_H

#include <Arduino.h>
#include <TinyGPS++.h>

#include <HardwareSerial.h>

struct GPSData {
    double latitude;
    double longitude;
    double altitude;   // metros
    double speed;      // km/h
    uint8_t fixQuality;
    bool isValid;
};

class GPSSensor {
public:
    GPSSensor(int rxPin, int txPin)
        : _rxPin(rxPin),
          _txPin(txPin),
          _gpsSerial(1),
          _fixQualityGPGGA(_gps, "GPGGA", 6),
          _fixQualityGNGGA(_gps, "GNGGA", 6),
          _lastValidTime(0)
    {
        _latestData.latitude = 0.0;
        _latestData.longitude = 0.0;
        _latestData.altitude = 0.0;
        _latestData.speed = 0.0;
        _latestData.fixQuality = 0;
        _latestData.isValid = false;
    }

    void begin(unsigned long baudRate = 9600);
    void update(); // Chamar no loop() a cada iteração, sem delay grande

    GPSData getLatestData() const;
    bool hasFix() const;

private:
    int _rxPin;
    int _txPin;
    
    HardwareSerial _gpsSerial;

    TinyGPSPlus _gps;

    TinyGPSCustom _fixQualityGPGGA;
    TinyGPSCustom _fixQualityGNGGA;

    GPSData _latestData;
    unsigned long _lastValidTime;

    void _parseData();
};

#endif