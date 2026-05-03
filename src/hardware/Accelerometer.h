#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

struct AccelData {
    float x;
    float y;
    float z;
    bool valid;  // false se leitura falhou
};

class Accelerometer {
public:
    Accelerometer();
    bool begin();
    bool isConnected() const;
    AccelData readAcceleration();

private:
    Adafruit_MPU6050 _mpu;
    bool _connected;
};

#endif