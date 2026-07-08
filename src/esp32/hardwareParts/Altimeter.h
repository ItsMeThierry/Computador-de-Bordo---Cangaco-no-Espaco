//TODO: documentar o código direito

#ifndef ALTIMETER_H
#define ALTIMETER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h> 

#define BMP280_SDA_PIN 21
#define BMP280_SCL_PIN 22

#define BMP280_ADDRESS 0x76

class Altimeter {
public:

    Altimeter() 
        : _connected(false), 
          _baseline(1013.25) {}

    /**
    * @brief Inicializa e configura o altímetro.
    * 
    * @return true se o sensor foi inicializado e configurado com sucesso, false caso contrário (ex: sensor não encontrado ou falha na comunicação).
    */
    bool begin() {
        Wire.begin(BMP280_SDA_PIN, BMP280_SCL_PIN);
        _connected = _sensor.begin(BMP280_ADDRESS); 
        
        if (_connected) {
            _sensor.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                                Adafruit_BMP280::SAMPLING_X2,     // Oversampling Temp
                                Adafruit_BMP280::SAMPLING_X16,    // Oversampling Pressão (máx precisão)
                                Adafruit_BMP280::FILTER_X16,      // Filtro anti-turbulência no hardware
                                Adafruit_BMP280::STANDBY_MS_1);   // Leituras super rápidas
            resetBaseline(); 
        }
        return _connected;
    }

    /**
    * @return true se o sensor está conectado, false caso contrário.
    */
    bool isConnected() const {
        return _connected;
    }

    /**
    * @brief Lê a medida de pressão do sensor.
    * 
    * @return O valor de pressão em hPa. 
    * @warning Retorna -1.0 caso o sensor não esteja conectado.
    */
    double readPressure() {
        if (!_connected) return -1.0;
        
        double p = _sensor.readPressure() / 100.0;
        return (p > 0) ? p : -1.0;
    }

    /**
    * @brief Lê o valor de altitude do sensor com base em um baseline.
    * 
    * @return Altitude em metros. 
    */
    double calculateAltitude() {
        float altitude = _sensor.readAltitude(_baseline);
        return altitude;
    }

    /**
    * @brief Reseta o valor do baseline de referência.
    */
    void resetBaseline() {
        double p = readPressure();
        if (p > 0) {
            _baseline = p;
        }
    }

    /**
    * @return Baseline de referência em metros. 
    */
    double getBaseline() const {
        return _baseline;
    }

private:
    Adafruit_BMP280 _sensor;
    bool _connected;
    double _baseline;
};

#endif
