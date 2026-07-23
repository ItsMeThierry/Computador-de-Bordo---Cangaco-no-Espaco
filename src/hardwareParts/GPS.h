#ifndef GPS_SENSOR_H
#define GPS_SENSOR_H

#include <Arduino.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "../../config/Constants.h"

/// @brief Estrutura de dados para o GPS.
struct GPSData {
    double latitude;    ///< Latitude (º)
    double longitude;   ///< Longitude (º)
    double altitude;    ///< Altitude (m)
    double speed;       ///< Velocidade (km/h)
    uint8_t fixQuality; ///< Fix
    bool isValid;       ///< Valido
};

class GPSSensor {
public:
    GPSSensor()
        : _gpsSerial(GPS_UART),
          _fixQualityGPGGA(_gps, "GPGGA", 6),
          _fixQualityGNGGA(_gps, "GNGGA", 6),
          _lastValidTime(0),
          _enabled(true)
    {
        _latestData.latitude = 0.0;
        _latestData.longitude = 0.0;
        _latestData.altitude = 0.0;
        _latestData.speed = 0.0;
        _latestData.fixQuality = 0;
        _latestData.isValid = false;
    }

    /**
    * @brief Inicializa a porta Serial do GPS com as configurações definidas.
    * 
    * @param baudeRate  Bauderate serial do GPS. DEFAULT = 9600.
    */
    void begin(unsigned long baudRate = 9600) {
        _gpsSerial.begin(baudRate, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

        _latestData.latitude = 0.0;
        _latestData.longitude = 0.0;
        _latestData.altitude = 0.0;
        _latestData.speed = 0.0;
        _latestData.fixQuality = 0;
        _latestData.isValid = false;
        _lastValidTime = 0;

        // Pequeno delay para o GPS iniciar a UART
        delay(200);

        // Configurar taxa de atualização para 5Hz (200ms)
        _configureUpdateRate(5);

        // Desabilitar sentenças NMEA desnecessárias (manter GGA + RMC)
        // Necessário a 9600 baud para suportar 5Hz sem overflow
        _disableUnneededNMEA();

        Serial.println(F("[GPS] Inicializado — 5Hz configurado"));
    }

    /**
    * @brief Lê a porta Serial, verificando se há dados do GPS.
    */
    void update() {
        // Se desativado, não processa dados (economiza CPU)
        if (!_enabled) return;

        while (_gpsSerial.available() > 0) {
            char c = _gpsSerial.read();
            
            if (_gps.encode(c)) {
                _parseData();
            }
        }

        // Se passar muito tempo sem localização válida, considera sem fix
        if (_lastValidTime > 0 && millis() - _lastValidTime > 5000) {
            _latestData.isValid = false;
            _latestData.fixQuality = 0;
        }
    }

    /**
    * @return Os últimos dados lidos, no formato GPSData.
    */
    GPSData getLatestData() const {
        return _latestData;
    }

    /**
    * @return True se o último dado lido for válido, false caso o contrário.
    */
    bool hasFix() const {
        return _latestData.isValid;
    }

    // Habilita/desabilita processamento do GPS
    // No PRE_FLIGHT o GPS fica desativado para economizar energia
    void setEnabled(bool enabled) {
        _enabled = enabled;
        if (enabled) {
            Serial.println(F("[GPS] Ativado"));
        } else {
            Serial.println(F("[GPS] Desativado (economia de energia)"));
        }
    }

    bool isEnabled() const {
        return _enabled;
    }

private:
    HardwareSerial _gpsSerial;
    TinyGPSPlus _gps;
    TinyGPSCustom _fixQualityGPGGA;
    TinyGPSCustom _fixQualityGNGGA;
    GPSData _latestData;
    unsigned long _lastValidTime;
    bool _enabled;

    /**
    * @brief Transforma os dados vindos do GPS em GPSData para salvá-los como os últimos dados.
    */
    void _parseData() {
        if (_fixQualityGPGGA.isValid()) {
            _latestData.fixQuality = atoi(_fixQualityGPGGA.value());
        } else if (_fixQualityGNGGA.isValid()) {
            _latestData.fixQuality = atoi(_fixQualityGNGGA.value());
        }

        if (_gps.location.isValid()) {
            _latestData.latitude = _gps.location.lat();
            _latestData.longitude = _gps.location.lng();
        }

        if (_gps.altitude.isValid()) {
            _latestData.altitude = _gps.altitude.meters();
        }

        if (_gps.speed.isValid()) {
            _latestData.speed = _gps.speed.kmph();
        }

        bool locationOk = _gps.location.isValid() && _gps.location.age() < 3000;
        bool fixOk = _latestData.fixQuality > 0;

        if (locationOk && fixOk) {
            _latestData.isValid = true;
            _lastValidTime = millis();
        } else {
            _latestData.isValid = false;
        }
    }

    // ========== Configuração UBX do NEO-6M ==========

    /**
     * Envia comando UBX-CFG-RATE para configurar taxa de atualização.
     * @param rateHz Taxa desejada em Hz (1, 2, 5, 10)
     */
    void _configureUpdateRate(uint8_t rateHz) {
        uint16_t measRateMs = 1000 / rateHz;  // ex: 5Hz → 200ms

        // UBX-CFG-RATE: Class=0x06, ID=0x08, Payload=6 bytes
        // measRate(2) + navRate(2) + timeRef(2)
        uint8_t payload[] = {
            (uint8_t)(measRateMs & 0xFF), (uint8_t)(measRateMs >> 8),  // measRate LE
            0x01, 0x00,  // navRate = 1
            0x01, 0x00   // timeRef = GPS time
        };

        _sendUBX(0x06, 0x08, payload, sizeof(payload));
        Serial.print(F("[GPS] Taxa UBX configurada: "));
        Serial.print(rateHz);
        Serial.println(F("Hz"));
    }

    /**
     * Desabilita sentenças NMEA desnecessárias para economizar banda a 9600 baud.
     * Mantém apenas GGA (posição/altitude) e RMC (velocidade/data).
     */
    void _disableUnneededNMEA() {
        // NMEA message IDs: GGA=0x00, GLL=0x01, GSA=0x02, GSV=0x03, RMC=0x04, VTG=0x05
        _setNMEAMessageRate(0xF0, 0x01, 0);  // Desabilitar GLL
        _setNMEAMessageRate(0xF0, 0x02, 0);  // Desabilitar GSA
        _setNMEAMessageRate(0xF0, 0x03, 0);  // Desabilitar GSV
        _setNMEAMessageRate(0xF0, 0x05, 0);  // Desabilitar VTG
        Serial.println(F("[GPS] NMEA otimizado: apenas GGA + RMC"));
    }

    /**
     * UBX-CFG-MSG: configura taxa de uma sentença NMEA.
     * @param msgClass Classe da mensagem (0xF0 para NMEA)
     * @param msgId    ID da mensagem
     * @param rate     0 = desabilitado, 1 = a cada fix
     */
    void _setNMEAMessageRate(uint8_t msgClass, uint8_t msgId, uint8_t rate) {
        uint8_t payload[] = { msgClass, msgId, rate };
        _sendUBX(0x06, 0x01, payload, sizeof(payload));
    }

    /**
     * Envia um comando UBX genérico com cálculo automático de checksum.
     * Protocolo UBX: SYNC1(0xB5) SYNC2(0x62) CLASS ID LEN(2) PAYLOAD CK_A CK_B
     */
    void _sendUBX(uint8_t msgClass, uint8_t msgId,
                  const uint8_t* payload, uint8_t payloadLen) {
        // Header
        _gpsSerial.write(0xB5);
        _gpsSerial.write(0x62);

        // Class + ID + Length
        _gpsSerial.write(msgClass);
        _gpsSerial.write(msgId);
        _gpsSerial.write(payloadLen);
        _gpsSerial.write((uint8_t)0x00);  // Length high byte (sempre 0 para payloads curtos)

        // Checksum calculado sobre: class, id, length(2 bytes), payload
        uint8_t ckA = 0, ckB = 0;

        ckA += msgClass;  ckB += ckA;
        ckA += msgId;     ckB += ckA;
        ckA += payloadLen; ckB += ckA;
        ckA += 0x00;      ckB += ckA;  // Length high byte

        // Payload
        for (uint8_t i = 0; i < payloadLen; i++) {
            _gpsSerial.write(payload[i]);
            ckA += payload[i];
            ckB += ckA;
        }

        // Checksum
        _gpsSerial.write(ckA);
        _gpsSerial.write(ckB);

        // Pequeno delay para o módulo processar
        delay(10);
    }
};

#endif