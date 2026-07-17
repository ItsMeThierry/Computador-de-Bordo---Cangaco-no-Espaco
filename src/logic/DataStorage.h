#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <esp_partition.h>
#include <math.h>
#include <stdlib.h>
#include "../utils/CircularQueue.h"
#include "../utils/Timer.h"
#include "../hardwareParts/Accelerometer.h"
#include "../logic/FlightStateMachine.h"
#include "../config/Constants.h"

struct EepromFlightSample {
    uint16_t timeCs;    // tempo de voo em centesimos de segundo
    uint16_t altitudeDm; // altitude em decimetros, sempre positiva
    int16_t accXCms2;    // aceleracao X em centesimos de m/s2
    int16_t accYCms2;    // aceleracao Y em centesimos de m/s2
    int16_t accZCms2;    // aceleracao Z em centesimos de m/s2
} __attribute__((packed));

static_assert(sizeof(EepromFlightSample) == EEPROM_FLIGHT_SAMPLE_BYTES,
              "EepromFlightSample precisa manter o tamanho compacto de 10 bytes");

class DataStorage {
public:
    DataStorage()
        : _recordCount(0),
          _decimationFactor(1),
          _overwriteIndex(0),
          _rawSampleCounter(0),
          _storageSizeBytes(EEPROM_FALLBACK_SIZE_BYTES),
          _maxFlightDataPoints((EEPROM_FALLBACK_SIZE_BYTES - EEPROM_METADATA_BYTES) / EEPROM_FLIGHT_SAMPLE_BYTES),
          _pendingWriteCount(0),
          _enabled(true),
          _timer(EEPROM_INTERVAL_RECOVERY_MS) {}

    void begin() {
        if (!_beginEeprom()) {
            Serial.println(F("[DataStorage] Erro ao inicializar EEPROM"));
            _enabled = false;
            return;
        }

        _loadHeader();
        Serial.print(F("[DataStorage] EEPROM pronta. Registros: "));
        Serial.print(_recordCount);
        Serial.print(F("/"));
        Serial.print(_maxFlightDataPoints);
        Serial.print(F(" em "));
        Serial.print(_storageSizeBytes);
        Serial.println(F(" bytes"));
    }

    void clearFlightLog() {
        flushPendingWrites();
        _recordCount = 0;
        _decimationFactor = 1;
        _overwriteIndex = 0;
        _rawSampleCounter = 0;
        _pendingWriteCount = 0;
        _writeHeader();
        EEPROM.commit();
        _timer.reset();
        Serial.println(F("[DataStorage] Backup EEPROM limpo"));
    }

    void update(unsigned long flightTimeMs, double altitude, const AccelData& accel, FlightState state) {
        if (!_enabled) {
            return;
        }

        if (state == FlightState::LANDED) {
            flushPendingWrites();
            return;
        }

        if (state == FlightState::PRE_FLIGHT) {
            return;
        }

        const unsigned long interval = (state == FlightState::ASCENT)
            ? EEPROM_INTERVAL_ASCENT_MS
            : EEPROM_INTERVAL_RECOVERY_MS;
        _timer.setInterval(interval);

        if (!_timer.isReady()) {
            return;
        }

        saveFlightSample(flightTimeMs, altitude, accel);
    }

    void saveFlightSample(unsigned long flightTimeMs, double altitude, const AccelData& accel) {
        if (!_enabled) {
            return;
        }

        const unsigned long sampleIndex = _rawSampleCounter++;

        if (_recordCount < _maxFlightDataPoints) {
            const EepromFlightSample sample = _packSample(flightTimeMs, altitude, accel);
            EEPROM.put(_getSampleAddress(_recordCount), sample);
            _recordCount++;
            _writeHeader();
            _markPendingSampleWrite();
            return;
        }

        if (_decimationFactor == 1 || _overwriteIndex >= _maxFlightDataPoints) {
            _startNextOverwriteLevel();
        }

        if ((sampleIndex % _decimationFactor) != 0) {
            return;
        }

        const EepromFlightSample sample = _packSample(flightTimeMs, altitude, accel);
        EEPROM.put(_getSampleAddress(_overwriteIndex), sample);
        _overwriteIndex += _decimationFactor;
        _writeHeader();
        _markPendingSampleWrite();
    }

    void flushPendingWrites() {
        if (_pendingWriteCount == 0) {
            return;
        }

        EEPROM.commit();
        _pendingWriteCount = 0;
    }

    bool readFlightSample(unsigned int index, EepromFlightSample& sample) const {
        if (index >= _recordCount || index >= _maxFlightDataPoints) {
            return false;
        }

        EEPROM.get(_getSampleAddress(index), sample);
        return true;
    }

    void printFlightLogCsv(Stream& output) const {
        output.println(F("Timestamp,AltBaro,AccX,AccY,AccZ"));

        if (_recordCount == 0) {
            return;
        }

        EepromFlightSample* samples = static_cast<EepromFlightSample*>(
            malloc(_recordCount * sizeof(EepromFlightSample))
        );

        if (samples == nullptr) {
            output.println(F("[DataStorage] Erro: RAM insuficiente para ordenar CSV"));
            return;
        }

        unsigned int validCount = 0;
        for (unsigned int i = 0; i < _recordCount; i++) {
            if (readFlightSample(i, samples[validCount])) {
                validCount++;
            }
        }

        _sortSamplesByTime(samples, validCount);

        for (unsigned int i = 0; i < validCount; i++) {
            output.print(samples[i].timeCs / 100.0f, 2);
            output.print(',');
            output.print(samples[i].altitudeDm / ALTITUDE_SCALE_FACTOR, 1);
            output.print(',');
            output.print(samples[i].accXCms2 / ACCEL_SCALE_FACTOR, 2);
            output.print(',');
            output.print(samples[i].accYCms2 / ACCEL_SCALE_FACTOR, 2);
            output.print(',');
            output.println(samples[i].accZCms2 / ACCEL_SCALE_FACTOR, 2);
        }

        free(samples);
    }

    void savePreFlightPoints(
        const CircularQueue<double, PRE_FLIGHT_QUEUE_SIZE>& altQueue,
        const CircularQueue<AccelData, PRE_FLIGHT_QUEUE_SIZE>& accelQueue
    ) {
        const int points = (altQueue.size() < accelQueue.size())
            ? altQueue.size()
            : accelQueue.size();

        for (int i = 0; i < points; i++) {
            const AccelData accel = accelQueue.get(i);
            saveFlightSample(0, altQueue.get(i), accel);
        }
    }

    unsigned int getRecordCount() const {
        return _recordCount;
    }

    unsigned int getDecimationFactor() const {
        return _decimationFactor;
    }

    unsigned int getOverwriteIndex() const {
        return _overwriteIndex;
    }

    unsigned int getStorageSizeBytes() const {
        return _storageSizeBytes;
    }

    unsigned int getMaxFlightDataPoints() const {
        return _maxFlightDataPoints;
    }

    unsigned int getPendingWriteCount() const {
        return _pendingWriteCount;
    }

    void setRecordCount(unsigned int count) {
        _recordCount = (count > _maxFlightDataPoints)
            ? _maxFlightDataPoints
            : count;
        _pendingWriteCount = 0;
        _writeHeader();
        EEPROM.commit();
    }

private:
    static constexpr uint32_t EEPROM_MAGIC = 0x434E4543; // "CNEC"
    static constexpr uint8_t EEPROM_VERSION = 4;
    static constexpr int HEADER_MAGIC_ADDR = 0;
    static constexpr int HEADER_VERSION_ADDR = 4;
    static constexpr int HEADER_RECORD_COUNT_ADDR = 6;
    static constexpr int HEADER_DECIMATION_FACTOR_ADDR = 8;
    static constexpr int HEADER_RAW_SAMPLE_COUNTER_ADDR = 10;
    static constexpr int HEADER_OVERWRITE_INDEX_ADDR = 14;

    unsigned int _recordCount;
    unsigned int _decimationFactor;
    unsigned int _overwriteIndex;
    unsigned long _rawSampleCounter;
    unsigned int _storageSizeBytes;
    unsigned int _maxFlightDataPoints;
    unsigned int _pendingWriteCount;
    bool _enabled;
    Timer _timer;

    bool _beginEeprom() {
        unsigned int candidateSize = _detectUsableStorageBytes();

        while (candidateSize >= static_cast<unsigned int>(EEPROM_FALLBACK_SIZE_BYTES)) {
            if (EEPROM.begin(candidateSize)) {
                _storageSizeBytes = candidateSize;
                _maxFlightDataPoints = _calculateMaxPoints(candidateSize);
                return true;
            }

            if (candidateSize < static_cast<unsigned int>(EEPROM_FALLBACK_SIZE_BYTES + EEPROM_PROBE_STEP_BYTES)) {
                break;
            }

            candidateSize -= EEPROM_PROBE_STEP_BYTES;
        }

        if (EEPROM.begin(EEPROM_FALLBACK_SIZE_BYTES)) {
            _storageSizeBytes = EEPROM_FALLBACK_SIZE_BYTES;
            _maxFlightDataPoints = _calculateMaxPoints(EEPROM_FALLBACK_SIZE_BYTES);
            return true;
        }

        return false;
    }

    unsigned int _detectUsableStorageBytes() const {
        const esp_partition_t* nvsPartition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_NVS,
            nullptr
        );

        if (nvsPartition == nullptr ||
            nvsPartition->size <= static_cast<size_t>(EEPROM_NVS_RESERVE_BYTES + EEPROM_FALLBACK_SIZE_BYTES)) {
            return EEPROM_FALLBACK_SIZE_BYTES;
        }

        unsigned int usableBytes = static_cast<unsigned int>(nvsPartition->size - EEPROM_NVS_RESERVE_BYTES);
        usableBytes -= usableBytes % EEPROM_PROBE_STEP_BYTES;

        return (usableBytes >= static_cast<unsigned int>(EEPROM_FALLBACK_SIZE_BYTES))
            ? usableBytes
            : EEPROM_FALLBACK_SIZE_BYTES;
    }

    static constexpr unsigned int _calculateMaxPoints(unsigned int storageSizeBytes) {
        return (storageSizeBytes > static_cast<unsigned int>(EEPROM_METADATA_BYTES))
            ? (storageSizeBytes - EEPROM_METADATA_BYTES) / EEPROM_FLIGHT_SAMPLE_BYTES
            : 0;
    }

    void _loadHeader() {
        uint32_t magic = 0;
        uint8_t version = 0;
        uint16_t count = 0;
        uint16_t decimationFactor = 1;
        uint32_t rawSampleCounter = 0;
        uint16_t overwriteIndex = 0;

        EEPROM.get(HEADER_MAGIC_ADDR, magic);
        EEPROM.get(HEADER_VERSION_ADDR, version);
        EEPROM.get(HEADER_RECORD_COUNT_ADDR, count);
        EEPROM.get(HEADER_DECIMATION_FACTOR_ADDR, decimationFactor);
        EEPROM.get(HEADER_RAW_SAMPLE_COUNTER_ADDR, rawSampleCounter);
        EEPROM.get(HEADER_OVERWRITE_INDEX_ADDR, overwriteIndex);

        if (magic != EEPROM_MAGIC || version != EEPROM_VERSION ||
            count > _maxFlightDataPoints || decimationFactor == 0 ||
            overwriteIndex > _maxFlightDataPoints) {
            _recordCount = 0;
            _decimationFactor = 1;
            _overwriteIndex = 0;
            _rawSampleCounter = 0;
            _pendingWriteCount = 0;
            _writeHeader();
            EEPROM.commit();
            return;
        }

        _recordCount = count;
        _decimationFactor = decimationFactor;
        _overwriteIndex = overwriteIndex;
        _rawSampleCounter = rawSampleCounter;
        _pendingWriteCount = 0;
    }

    void _writeHeader() const {
        const uint32_t magic = EEPROM_MAGIC;
        const uint8_t version = EEPROM_VERSION;
        const uint8_t sampleSize = sizeof(EepromFlightSample);
        const uint16_t count = _recordCount;
        const uint16_t decimationFactor = _decimationFactor;
        const uint32_t rawSampleCounter = _rawSampleCounter;
        const uint16_t overwriteIndex = _overwriteIndex;

        EEPROM.put(HEADER_MAGIC_ADDR, magic);
        EEPROM.put(HEADER_VERSION_ADDR, version);
        EEPROM.put(5, sampleSize);
        EEPROM.put(HEADER_RECORD_COUNT_ADDR, count);
        EEPROM.put(HEADER_DECIMATION_FACTOR_ADDR, decimationFactor);
        EEPROM.put(HEADER_RAW_SAMPLE_COUNTER_ADDR, rawSampleCounter);
        EEPROM.put(HEADER_OVERWRITE_INDEX_ADDR, overwriteIndex);
    }

    void _startNextOverwriteLevel() {
        if (_decimationFactor <= 32767U) {
            _decimationFactor *= 2U;
        }

        _overwriteIndex = _decimationFactor / 2U;
        _writeHeader();
        EEPROM.commit();

        Serial.print(F("[DataStorage] EEPROM em sobrescrita espacada. Novo fator: 1/"));
        Serial.println(_decimationFactor);
    }

    void _markPendingSampleWrite() {
        _pendingWriteCount++;

        if (_pendingWriteCount >= EEPROM_COMMIT_BATCH_SIZE) {
            EEPROM.commit();
            _pendingWriteCount = 0;
        }
    }

    void _sortSamplesByTime(EepromFlightSample* samples, unsigned int count) const {
        for (unsigned int i = 1; i < count; i++) {
            EepromFlightSample current = samples[i];
            int j = i - 1;

            while (j >= 0 && samples[j].timeCs > current.timeCs) {
                samples[j + 1] = samples[j];
                j--;
            }

            samples[j + 1] = current;
        }
    }

    int _getSampleAddress(unsigned int index) const {
        return EEPROM_METADATA_BYTES + (index * sizeof(EepromFlightSample));
    }

    EepromFlightSample _packSample(unsigned long flightTimeMs, double altitude, const AccelData& accel) const {
        EepromFlightSample sample;
        const unsigned long timeCs = flightTimeMs / 10UL;
        sample.timeCs = static_cast<uint16_t>(timeCs > 65535UL ? 65535UL : timeCs);
        sample.altitudeDm = static_cast<uint16_t>(_clampScaledUnsigned(altitude, ALTITUDE_SCALE_FACTOR));
        sample.accXCms2 = static_cast<int16_t>(_clampScaled(accel.valid ? accel.accX : 0.0f, ACCEL_SCALE_FACTOR));
        sample.accYCms2 = static_cast<int16_t>(_clampScaled(accel.valid ? accel.accY : 0.0f, ACCEL_SCALE_FACTOR));
        sample.accZCms2 = static_cast<int16_t>(_clampScaled(accel.valid ? accel.accZ : 0.0f, ACCEL_SCALE_FACTOR));
        return sample;
    }

    long _clampScaled(double value, float scale) const {
        const long scaled = lround(value * scale);

        if (scaled > 32767L) return 32767L;
        if (scaled < -32768L) return -32768L;
        return scaled;
    }

    unsigned long _clampScaledUnsigned(double value, float scale) const {
        const long scaled = lround(value * scale);

        if (scaled > 65535L) return 65535UL;
        if (scaled < 0L) return 0UL;
        return static_cast<unsigned long>(scaled);
    }

};

#endif
