#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

/// @brief Estrutura de dados para o acelerômetro.
struct AccelData {
    float accX;   ///< Aceleração no eixo X (m/s² em g)
    float accY;   ///< Aceleração no eixo Y (m/s² em g)
    float accZ;   ///< Aceleração no eixo Z (m/s² em g)
    float gyrX;   ///< Velocidade angular no eixo X (dps)
    float gyrY;   ///< Velocidade angular no eixo Y (dps)
    float gyrZ;   ///< Velocidade angular no eixo Z (dps)
    float temp;   ///< Temperatura em graus Celsius (°C)
    bool valid;   ///< true se os dados são válidos, false se falha na leitura
};

class Accelerometer {
public:
    Accelerometer() {}
    
    /**
    * @brief Inicializa e configura o acelerômetro.
    * 
    * @return true se o sensor foi inicializado e configurado com sucesso, false caso contrário (ex: sensor não encontrado ou falha na comunicação).
    */
    bool begin() {
        if (!_mpu.begin()) {
            return false;
        }
        _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        return true;
    }
    
    /**
    * @brief Lê os dados atuais do acelerômetro (aceleração, giroscópio e temperatura).
    * 
    * @return AccelData contendo:
    *   - accX, accY, accZ: Aceleração nos eixos X, Y e Z (em g)
    *   - gyrX, gyrZ, gyrZ: Velocidade angular nos eixos X, Y e Z (dps)
    *   - temp: Temperatura (°C)
    *   - valid: true se a leitura foi bem sucedida, false caso contrário 
    */
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

    /**
    * @brief Exibe os dados da estrutura AccelData.
    * 
    * @warning Usado apenas para debug.
    */
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