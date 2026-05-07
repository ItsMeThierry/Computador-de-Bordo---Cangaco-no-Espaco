#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

struct AccelData {
    float accX;
    float accY;
    float accZ;
    float gyrX;
    float gyrY;
    float gyrZ;
    float temp;
    bool valid;
};

class Accelerometer {
public:
    Accelerometer() {}
    
    bool begin() {
        if (!_mpu.begin()) {
            return false;
        }
        _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        return true;
    }
    
    AccelData readAcceleration() {
        sensors_event_t acc, gyr, temp;
        bool isEventValid = _mpu.getEvent(&acc, &gyr, &temp);
        
        AccelData accData;
        accData.accX = acc.acceleration.x;
        accData.accY = acc.acceleration.y;
        accData.accZ = acc.acceleration.z;
        accData.gyrX = gyr.gyro.x;
        accData.gyrY = gyr.gyro.y;
        accData.gyrZ = gyr.gyro.z;
        accData.temp = temp.temperature;
        accData.valid = isEventValid;
        
        return accData;
    }

    void printData(AccelData &data) {
        Serial.print("X: "); Serial.print(data.accX);
        Serial.print(", Y: "); Serial.print(data.accY);
        Serial.print(", Z: "); Serial.print(data.accZ);
        Serial.print(", GYRX: "); Serial.print(data.gyrX);
        Serial.print(", GYRY: "); Serial.print(data.gyrY);
        Serial.print(", GYRZ: "); Serial.print(data.gyrZ);
        Serial.print(", TEMP: "); Serial.println(data.temp);
    }

private:
    Adafruit_MPU6050 _mpu;
};

#endif