#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <esp_partition.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../hardwareParts/Accelerometer.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/FlightStateUtils.h"
#include "../utils/Timer.h"
#include "../config/Constants.h"

struct EepromFlightSample {
    uint8_t bytes[FLIGHTLOG_FLIGHT_SAMPLE_BYTES];
} __attribute__((packed));

static_assert(sizeof(EepromFlightSample) == FLIGHTLOG_FLIGHT_SAMPLE_BYTES,
              "EepromFlightSample precisa manter o maior tamanho compacto configurado");

struct DecodedFlightSample {
    uint16_t timeCs;
    uint8_t state;
    uint16_t altitudeDm;
    int16_t angVelXCrads;
    int16_t angVelYCrads;
    int16_t angVelZCrads;
    int16_t accXScaled;
    int16_t accYScaled;
    int16_t accZScaled;
};

class DataStorage {
public:
    DataStorage()
        : _recordCount(0),
          _decimationFactor(1),
          _overwriteIndex(0),
          _rawSampleCounter(0),
          _storageSizeBytes(EEPROM_FALLBACK_SIZE_BYTES),
          _dataStartOffset(EEPROM_METADATA_BYTES),
          _sampleSizeBytes(EEPROM_FLIGHT_SAMPLE_BYTES),
          _maxFlightDataPoints(_calculateMaxPoints(EEPROM_FALLBACK_SIZE_BYTES, EEPROM_METADATA_BYTES, EEPROM_FLIGHT_SAMPLE_BYTES)),
          _pendingWriteCount(0),
          _flightLogPartition(nullptr),
          _usingFlightLogPartition(false),
          _headerDirty(false),
          _enabled(true),
          _timer(EEPROM_INTERVAL_RECOVERY_MS) {}

    void begin() {
        if (!_beginStorage()) {
            Serial.println(F("[DataStorage] Erro ao inicializar armazenamento"));
            _enabled = false;
            return;
        }

        _loadHeader();
        Serial.print(F("[DataStorage] Backend: "));
        Serial.println(_usingFlightLogPartition ? F("flightlog") : F("EEPROM/NVS"));
        Serial.print(F("[DataStorage] Armazenamento pronto. Registros: "));
        Serial.print(_recordCount);
        Serial.print(F("/"));
        Serial.print(_maxFlightDataPoints);
        Serial.print(F(" em "));
        Serial.print(_storageSizeBytes);
        Serial.print(F(" bytes | amostra: "));
        Serial.print(_sampleSizeBytes);
        Serial.println(F(" bytes"));
    }

    void clearFlightLog() {
        flushPendingWrites();
        _recordCount = 0;
        _decimationFactor = 1;
        _overwriteIndex = 0;
        _rawSampleCounter = 0;
        _pendingWriteCount = 0;
        _headerDirty = false;

        if (_usingFlightLogPartition) {
            _eraseAllStorage();
        }

        _writeHeaderToStorage();
        _commitStorage();
        _timer.reset();
        Serial.println(F("[DataStorage] Backup limpo"));
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

        const unsigned long interval = _isCriticalFlightState(state)
            ? EEPROM_INTERVAL_ASCENT_MS
            : EEPROM_INTERVAL_RECOVERY_MS;
        _timer.setInterval(interval);

        if (!_timer.isReady()) {
            return;
        }

        saveFlightSample(flightTimeMs, state, altitude, accel);
    }

    void saveFlightSample(unsigned long flightTimeMs, FlightState state, double altitude, const AccelData& accel) {
        if (!_enabled) {
            return;
        }

        const unsigned long sampleIndex = _rawSampleCounter++;

        if (_recordCount < _maxFlightDataPoints) {
            const EepromFlightSample sample = _packSample(flightTimeMs, state, altitude, accel);
            if (!_writeSample(_recordCount, sample, false)) {
                Serial.println(F("[DataStorage] Erro ao gravar amostra"));
                return;
            }

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

        const EepromFlightSample sample = _packSample(flightTimeMs, state, altitude, accel);
        if (!_writeSample(_overwriteIndex, sample, true)) {
            Serial.println(F("[DataStorage] Erro ao sobrescrever amostra"));
            return;
        }

        if (_overwriteIndex >= _decimationFactor) {
            _overwriteIndex -= _decimationFactor;
        } else {
            _overwriteIndex = _maxFlightDataPoints;
        }

        _writeHeader();
        _markPendingSampleWrite();
    }

    void flushPendingWrites() {
        if (_pendingWriteCount == 0 && !_headerDirty) {
            return;
        }

        _commitStorage();
        _pendingWriteCount = 0;
    }

    bool readFlightSample(unsigned int index, EepromFlightSample& sample) const {
        if (index >= _recordCount || index >= _maxFlightDataPoints) {
            return false;
        }

        memset(&sample, 0, sizeof(sample));
        if (!_readBytes(_getSampleAddress(index), &sample, _sampleSizeBytes)) {
            return false;
        }

        return _decodeSample(sample).state <= static_cast<uint8_t>(FlightState::LANDED);
    }

    void printFlightLogCsv(Stream& output) const {
        if (_sampleHasAcceleration()) {
            output.println(F("Timestamp,StateCode,State,AltBaro,AngVelX,AngVelY,AngVelZ,AccX,AccY,AccZ"));
        } else {
            output.println(F("Timestamp,StateCode,State,AltBaro,AngVelX,AngVelY,AngVelZ"));
        }

        if (_recordCount == 0) {
            return;
        }

        uint16_t* sampleTimes = static_cast<uint16_t*>(malloc(_recordCount * sizeof(uint16_t)));
        uint16_t* sampleIndexes = static_cast<uint16_t*>(malloc(_recordCount * sizeof(uint16_t)));

        if (sampleTimes == nullptr || sampleIndexes == nullptr) {
            free(sampleTimes);
            free(sampleIndexes);
            output.println(F("[DataStorage] Erro: RAM insuficiente para ordenar CSV"));
            return;
        }

        unsigned int validCount = 0;
        for (unsigned int i = 0; i < _recordCount; i++) {
            EepromFlightSample sample;
            if (readFlightSample(i, sample)) {
                sampleTimes[validCount] = _decodeSample(sample).timeCs;
                sampleIndexes[validCount] = i;
                validCount++;
            }
        }

        _sortIndexesByTime(sampleTimes, sampleIndexes, validCount);

        for (unsigned int i = 0; i < validCount; i++) {
            EepromFlightSample sample;
            if (!readFlightSample(sampleIndexes[i], sample)) {
                continue;
            }

            const DecodedFlightSample decoded = _decodeSample(sample);
            const FlightState state = static_cast<FlightState>(decoded.state);

            output.print(decoded.timeCs / 100.0f, 2);
            output.print(',');
            output.print(decoded.state);
            output.print(',');
            output.print(FlightStateUtils::toString(state));
            output.print(',');
            output.print(decoded.altitudeDm / ALTITUDE_SCALE_FACTOR, 1);
            output.print(',');
            output.print(decoded.angVelXCrads / ANGULAR_VELOCITY_SCALE_FACTOR, 2);
            output.print(',');
            output.print(decoded.angVelYCrads / ANGULAR_VELOCITY_SCALE_FACTOR, 2);
            output.print(',');
            output.print(decoded.angVelZCrads / ANGULAR_VELOCITY_SCALE_FACTOR, 2);

            if (_sampleHasAcceleration()) {
                output.print(',');
                output.print(decoded.accXScaled / ACCEL_BACKUP_SCALE_FACTOR, 2);
                output.print(',');
                output.print(decoded.accYScaled / ACCEL_BACKUP_SCALE_FACTOR, 2);
                output.print(',');
                output.print(decoded.accZScaled / ACCEL_BACKUP_SCALE_FACTOR, 2);
            }

            output.println();
        }

        free(sampleTimes);
        free(sampleIndexes);
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

    unsigned int getDataStartOffset() const {
        return _dataStartOffset;
    }

    unsigned int getSampleSizeBytes() const {
        return _sampleSizeBytes;
    }

    unsigned int getMaxFlightDataPoints() const {
        return _maxFlightDataPoints;
    }

    unsigned int getPendingWriteCount() const {
        return _pendingWriteCount;
    }

    bool isUsingFlightLogPartition() const {
        return _usingFlightLogPartition;
    }

    void setRecordCount(unsigned int count) {
        _recordCount = (count > _maxFlightDataPoints)
            ? _maxFlightDataPoints
            : count;
        _pendingWriteCount = 0;
        _writeHeader();
        _commitStorage();
    }

private:
    static constexpr uint32_t EEPROM_MAGIC = 0x434E4543; // "CNEC"
    static constexpr uint8_t EEPROM_VERSION = 12;
    static constexpr unsigned int MAX_HEADER_RECORD_COUNT = 65535U;
    static constexpr int HEADER_MAGIC_ADDR = 0;
    static constexpr int HEADER_VERSION_ADDR = 4;
    static constexpr int HEADER_SAMPLE_SIZE_ADDR = 5;
    static constexpr int HEADER_RECORD_COUNT_ADDR = 6;
    static constexpr int HEADER_DECIMATION_FACTOR_ADDR = 8;
    static constexpr int HEADER_RAW_SAMPLE_COUNTER_ADDR = 10;
    static constexpr int HEADER_OVERWRITE_INDEX_ADDR = 14;

    unsigned int _recordCount;
    unsigned int _decimationFactor;
    unsigned int _overwriteIndex;
    unsigned long _rawSampleCounter;
    unsigned int _storageSizeBytes;
    unsigned int _dataStartOffset;
    unsigned int _sampleSizeBytes;
    unsigned int _maxFlightDataPoints;
    unsigned int _pendingWriteCount;
    const esp_partition_t* _flightLogPartition;
    bool _usingFlightLogPartition;
    bool _headerDirty;
    bool _enabled;
    Timer _timer;

    bool _beginStorage() {
        if (BACKUP_USE_FLIGHTLOG_PARTITION && _beginFlightLogPartition()) {
            return true;
        }

        if (BACKUP_USE_FLIGHTLOG_PARTITION) {
            Serial.println(F("[DataStorage] Particao flightlog nao encontrada; usando EEPROM/NVS"));
        }

        return _beginEeprom();
    }

    bool _isCriticalFlightState(FlightState state) const {
        return state == FlightState::ASCENT ||
               state == FlightState::ACTIVATE_SKIB_ONE;
    }

    bool _beginFlightLogPartition() {
        _flightLogPartition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            static_cast<esp_partition_subtype_t>(0xFF),
            FLIGHTLOG_PARTITION_LABEL
        );

        if (_flightLogPartition == nullptr ||
            _flightLogPartition->size <= static_cast<size_t>(FLIGHTLOG_METADATA_BYTES + FLIGHTLOG_FLIGHT_SAMPLE_BYTES)) {
            _flightLogPartition = nullptr;
            _usingFlightLogPartition = false;
            return false;
        }

        _usingFlightLogPartition = true;
        _storageSizeBytes = static_cast<unsigned int>(_flightLogPartition->size);
        _dataStartOffset = FLIGHTLOG_METADATA_BYTES;
        _sampleSizeBytes = FLIGHTLOG_FLIGHT_SAMPLE_BYTES;
        _maxFlightDataPoints = _calculateMaxPoints(_storageSizeBytes, _dataStartOffset, _sampleSizeBytes);
        return true;
    }

    bool _beginEeprom() {
        _flightLogPartition = nullptr;
        _usingFlightLogPartition = false;
        _dataStartOffset = EEPROM_METADATA_BYTES;
        _sampleSizeBytes = EEPROM_FLIGHT_SAMPLE_BYTES;

        unsigned int candidateSize = _detectUsableStorageBytes();

        while (candidateSize >= static_cast<unsigned int>(EEPROM_FALLBACK_SIZE_BYTES)) {
            if (EEPROM.begin(candidateSize)) {
                _storageSizeBytes = candidateSize;
                _maxFlightDataPoints = _calculateMaxPoints(candidateSize, _dataStartOffset, _sampleSizeBytes);
                return true;
            }

            if (candidateSize < static_cast<unsigned int>(EEPROM_FALLBACK_SIZE_BYTES + EEPROM_PROBE_STEP_BYTES)) {
                break;
            }

            candidateSize -= EEPROM_PROBE_STEP_BYTES;
        }

        if (EEPROM.begin(EEPROM_FALLBACK_SIZE_BYTES)) {
            _storageSizeBytes = EEPROM_FALLBACK_SIZE_BYTES;
            _maxFlightDataPoints = _calculateMaxPoints(EEPROM_FALLBACK_SIZE_BYTES, _dataStartOffset, _sampleSizeBytes);
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

    static constexpr unsigned int _calculateMaxPoints(unsigned int storageSizeBytes, unsigned int dataStartOffset, unsigned int sampleSizeBytes) {
        const unsigned int points = (storageSizeBytes > dataStartOffset)
            ? (storageSizeBytes - dataStartOffset) / sampleSizeBytes
            : 0;

        return (points > MAX_HEADER_RECORD_COUNT) ? MAX_HEADER_RECORD_COUNT : points;
    }

    void _loadHeader() {
        uint32_t magic = 0;
        uint8_t version = 0;
        uint8_t sampleSize = 0;
        uint16_t count = 0;
        uint16_t decimationFactor = 1;
        uint32_t rawSampleCounter = 0;
        uint16_t overwriteIndex = 0;

        _readBytes(HEADER_MAGIC_ADDR, &magic, sizeof(magic));
        _readBytes(HEADER_VERSION_ADDR, &version, sizeof(version));
        _readBytes(HEADER_SAMPLE_SIZE_ADDR, &sampleSize, sizeof(sampleSize));
        _readBytes(HEADER_RECORD_COUNT_ADDR, &count, sizeof(count));
        _readBytes(HEADER_DECIMATION_FACTOR_ADDR, &decimationFactor, sizeof(decimationFactor));
        _readBytes(HEADER_RAW_SAMPLE_COUNTER_ADDR, &rawSampleCounter, sizeof(rawSampleCounter));
        _readBytes(HEADER_OVERWRITE_INDEX_ADDR, &overwriteIndex, sizeof(overwriteIndex));

        if (magic != EEPROM_MAGIC || version != EEPROM_VERSION ||
            sampleSize != _sampleSizeBytes || count > _maxFlightDataPoints ||
            decimationFactor == 0 || overwriteIndex > _maxFlightDataPoints) {
            if (_usingFlightLogPartition) {
                _eraseAllStorage();
            }

            _recordCount = 0;
            _decimationFactor = 1;
            _overwriteIndex = 0;
            _rawSampleCounter = 0;
            _pendingWriteCount = 0;
            _headerDirty = false;
            _writeHeaderToStorage();
            _commitStorage();
            return;
        }

        _recordCount = count;
        _decimationFactor = decimationFactor;
        _overwriteIndex = overwriteIndex;
        _rawSampleCounter = rawSampleCounter;
        _pendingWriteCount = 0;
        _headerDirty = false;
    }

    void _writeHeader() {
        if (_usingFlightLogPartition) {
            _headerDirty = true;
            return;
        }

        _writeHeaderToStorage();
    }

    bool _writeHeaderToStorage() {
        const uint32_t magic = EEPROM_MAGIC;
        const uint8_t version = EEPROM_VERSION;
        const uint8_t sampleSize = _sampleSizeBytes;
        const uint16_t count = _recordCount;
        const uint16_t decimationFactor = _decimationFactor;
        const uint32_t rawSampleCounter = _rawSampleCounter;
        const uint16_t overwriteIndex = _overwriteIndex;

        uint8_t header[EEPROM_METADATA_BYTES];
        memset(header, 0, sizeof(header));
        memcpy(header + HEADER_MAGIC_ADDR, &magic, sizeof(magic));
        memcpy(header + HEADER_VERSION_ADDR, &version, sizeof(version));
        memcpy(header + HEADER_SAMPLE_SIZE_ADDR, &sampleSize, sizeof(sampleSize));
        memcpy(header + HEADER_RECORD_COUNT_ADDR, &count, sizeof(count));
        memcpy(header + HEADER_DECIMATION_FACTOR_ADDR, &decimationFactor, sizeof(decimationFactor));
        memcpy(header + HEADER_RAW_SAMPLE_COUNTER_ADDR, &rawSampleCounter, sizeof(rawSampleCounter));
        memcpy(header + HEADER_OVERWRITE_INDEX_ADDR, &overwriteIndex, sizeof(overwriteIndex));

        const bool ok = _writeBytesBuffered(0, header, sizeof(header));
        _headerDirty = !ok;
        return ok;
    }

    void _startNextOverwriteLevel() {
        if (_decimationFactor <= 32767U) {
            _decimationFactor *= 2U;
        }

        const unsigned int phase = _decimationFactor / 2U;
        const unsigned int lastIndex = _maxFlightDataPoints - 1U;
        const unsigned int distanceFromPhase = (lastIndex >= phase)
            ? ((lastIndex - phase) % _decimationFactor)
            : 0;

        _overwriteIndex = (lastIndex >= phase)
            ? (lastIndex - distanceFromPhase)
            : _maxFlightDataPoints;

        _writeHeader();
        _commitStorage();

        Serial.print(F("[DataStorage] Backup em sobrescrita espacada. Novo fator: 1/"));
        Serial.println(_decimationFactor);
    }

    void _markPendingSampleWrite() {
        _pendingWriteCount++;

        if (_pendingWriteCount >= EEPROM_COMMIT_BATCH_SIZE) {
            _commitStorage();
            _pendingWriteCount = 0;
        }
    }

    void _commitStorage() {
        if (_usingFlightLogPartition) {
            if (_headerDirty) {
                _writeHeaderToStorage();
            }
            return;
        }

        EEPROM.commit();
    }

    void _sortIndexesByTime(uint16_t* times, uint16_t* indexes, unsigned int count) const {
        for (unsigned int i = 1; i < count; i++) {
            const uint16_t currentTime = times[i];
            const uint16_t currentIndex = indexes[i];
            int j = i - 1;

            while (j >= 0 && times[j] > currentTime) {
                times[j + 1] = times[j];
                indexes[j + 1] = indexes[j];
                j--;
            }

            times[j + 1] = currentTime;
            indexes[j + 1] = currentIndex;
        }
    }

    int _getSampleAddress(unsigned int index) const {
        return _dataStartOffset + (index * _sampleSizeBytes);
    }

    bool _writeSample(unsigned int index, const EepromFlightSample& sample, bool overwrite) {
        const unsigned int address = _getSampleAddress(index);

        if (_usingFlightLogPartition && !overwrite && _isRangeErased(address, _sampleSizeBytes)) {
            return _writeBytesDirect(address, &sample, _sampleSizeBytes);
        }

        return _writeBytesBuffered(address, &sample, _sampleSizeBytes);
    }

    bool _isRangeErased(unsigned int address, size_t length) const {
        if (!_usingFlightLogPartition) {
            return true;
        }

        uint8_t* buffer = static_cast<uint8_t*>(malloc(length));
        if (buffer == nullptr) {
            return false;
        }

        const bool readOk = _readBytes(address, buffer, length);
        bool erased = readOk;

        for (size_t i = 0; erased && i < length; i++) {
            erased = buffer[i] == 0xFF;
        }

        free(buffer);
        return erased;
    }

    bool _readBytes(unsigned int address, void* data, size_t length) const {
        if (_usingFlightLogPartition) {
            if (_flightLogPartition == nullptr || (address + length) > _storageSizeBytes) {
                return false;
            }
            return esp_partition_read(_flightLogPartition, address, data, length) == ESP_OK;
        }

        uint8_t* bytes = static_cast<uint8_t*>(data);
        for (size_t i = 0; i < length; i++) {
            bytes[i] = EEPROM.read(address + i);
        }
        return true;
    }

    bool _writeBytesDirect(unsigned int address, const void* data, size_t length) {
        if (_usingFlightLogPartition) {
            if (_flightLogPartition == nullptr || (address + length) > _storageSizeBytes) {
                return false;
            }
            return esp_partition_write(_flightLogPartition, address, data, length) == ESP_OK;
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < length; i++) {
            EEPROM.write(address + i, bytes[i]);
        }
        return true;
    }

    bool _writeBytesBuffered(unsigned int address, const void* data, size_t length) {
        if (!_usingFlightLogPartition) {
            return _writeBytesDirect(address, data, length);
        }

        const uint8_t* source = static_cast<const uint8_t*>(data);
        size_t remaining = length;
        unsigned int currentAddress = address;

        while (remaining > 0) {
            const unsigned int sectorStart = (currentAddress / FLASH_ERASE_SECTOR_BYTES) * FLASH_ERASE_SECTOR_BYTES;
            const unsigned int sectorOffset = currentAddress - sectorStart;
            const size_t sectorAvailable = static_cast<size_t>(FLASH_ERASE_SECTOR_BYTES - sectorOffset);
            const size_t chunk = (remaining < sectorAvailable) ? remaining : sectorAvailable;

            uint8_t* sector = static_cast<uint8_t*>(malloc(FLASH_ERASE_SECTOR_BYTES));
            if (sector == nullptr) {
                return false;
            }

            bool ok = esp_partition_read(_flightLogPartition, sectorStart, sector, FLASH_ERASE_SECTOR_BYTES) == ESP_OK;
            if (ok) {
                memcpy(sector + sectorOffset, source, chunk);
                ok = esp_partition_erase_range(_flightLogPartition, sectorStart, FLASH_ERASE_SECTOR_BYTES) == ESP_OK;
            }
            if (ok) {
                ok = esp_partition_write(_flightLogPartition, sectorStart, sector, FLASH_ERASE_SECTOR_BYTES) == ESP_OK;
            }

            free(sector);

            if (!ok) {
                return false;
            }

            source += chunk;
            currentAddress += chunk;
            remaining -= chunk;
        }

        return true;
    }

    bool _eraseAllStorage() {
        if (!_usingFlightLogPartition || _flightLogPartition == nullptr) {
            return true;
        }

        return esp_partition_erase_range(_flightLogPartition, 0, _flightLogPartition->size) == ESP_OK;
    }

    EepromFlightSample _packSample(unsigned long flightTimeMs, FlightState state, double altitude,
                                   const AccelData& accel) const {
        EepromFlightSample sample;
        for (uint8_t i = 0; i < FLIGHTLOG_FLIGHT_SAMPLE_BYTES; i++) {
            sample.bytes[i] = 0;
        }

        const unsigned long timeCs = flightTimeMs / 10UL;
        const uint16_t packedTimeCs = static_cast<uint16_t>(timeCs > 65535UL ? 65535UL : timeCs);
        const uint8_t packedState = static_cast<uint8_t>(state) & 0x07;
        const uint16_t packedAltitudeDm = static_cast<uint16_t>(_clampScaledUnsigned(altitude, ALTITUDE_SCALE_FACTOR));
        const int16_t packedAngVelX = _clampScaledSigned(accel.valid ? accel.gyrX : 0.0f, ANGULAR_VELOCITY_SCALE_FACTOR, -2048L, 2047L);
        const int16_t packedAngVelY = _clampScaledSigned(accel.valid ? accel.gyrY : 0.0f, ANGULAR_VELOCITY_SCALE_FACTOR, -2048L, 2047L);
        const int16_t packedAngVelZ = _clampScaledSigned(accel.valid ? accel.gyrZ : 0.0f, ANGULAR_VELOCITY_SCALE_FACTOR, -2048L, 2047L);

        _writeBits(sample, 0, 16, packedTimeCs);
        _writeBits(sample, 16, 3, packedState);
        _writeBits(sample, 19, 16, packedAltitudeDm);
        _writeBits(sample, 35, 12, _encodeSigned12(packedAngVelX));
        _writeBits(sample, 47, 12, _encodeSigned12(packedAngVelY));
        _writeBits(sample, 59, 12, _encodeSigned12(packedAngVelZ));

        if (_sampleHasAcceleration()) {
            const int16_t packedAccX = _clampScaledSigned(accel.valid ? accel.accX : 0.0f, ACCEL_BACKUP_SCALE_FACTOR, -2048L, 2047L);
            const int16_t packedAccY = _clampScaledSigned(accel.valid ? accel.accY : 0.0f, ACCEL_BACKUP_SCALE_FACTOR, -2048L, 2047L);
            const int16_t packedAccZ = _clampScaledSigned(accel.valid ? accel.accZ : 0.0f, ACCEL_BACKUP_SCALE_FACTOR, -2048L, 2047L);

            _writeBits(sample, 71, 12, _encodeSigned12(packedAccX));
            _writeBits(sample, 83, 12, _encodeSigned12(packedAccY));
            _writeBits(sample, 95, 12, _encodeSigned12(packedAccZ));
        }

        return sample;
    }

    DecodedFlightSample _decodeSample(const EepromFlightSample& sample) const {
        DecodedFlightSample decoded;
        decoded.timeCs = static_cast<uint16_t>(_readBits(sample, 0, 16));
        decoded.state = static_cast<uint8_t>(_readBits(sample, 16, 3));
        decoded.altitudeDm = static_cast<uint16_t>(_readBits(sample, 19, 16));
        decoded.angVelXCrads = _signExtend12(static_cast<uint16_t>(_readBits(sample, 35, 12)));
        decoded.angVelYCrads = _signExtend12(static_cast<uint16_t>(_readBits(sample, 47, 12)));
        decoded.angVelZCrads = _signExtend12(static_cast<uint16_t>(_readBits(sample, 59, 12)));
        decoded.accXScaled = 0;
        decoded.accYScaled = 0;
        decoded.accZScaled = 0;

        if (_sampleHasAcceleration()) {
            decoded.accXScaled = _signExtend12(static_cast<uint16_t>(_readBits(sample, 71, 12)));
            decoded.accYScaled = _signExtend12(static_cast<uint16_t>(_readBits(sample, 83, 12)));
            decoded.accZScaled = _signExtend12(static_cast<uint16_t>(_readBits(sample, 95, 12)));
        }

        return decoded;
    }

    bool _sampleHasAcceleration() const {
        return _sampleSizeBytes >= FLIGHTLOG_FLIGHT_SAMPLE_BYTES;
    }

    void _writeBits(EepromFlightSample& sample, uint8_t startBit, uint8_t bitCount, uint32_t value) const {
        for (uint8_t i = 0; i < bitCount; i++) {
            const uint8_t bitIndex = startBit + i;
            const uint8_t byteIndex = bitIndex / 8;
            const uint8_t mask = static_cast<uint8_t>(1U << (bitIndex % 8));

            if ((value & (1UL << i)) != 0) {
                sample.bytes[byteIndex] |= mask;
            } else {
                sample.bytes[byteIndex] &= static_cast<uint8_t>(~mask);
            }
        }
    }

    uint32_t _readBits(const EepromFlightSample& sample, uint8_t startBit, uint8_t bitCount) const {
        uint32_t value = 0;

        for (uint8_t i = 0; i < bitCount; i++) {
            const uint8_t bitIndex = startBit + i;
            const uint8_t byteIndex = bitIndex / 8;
            const uint8_t mask = static_cast<uint8_t>(1U << (bitIndex % 8));

            if ((sample.bytes[byteIndex] & mask) != 0) {
                value |= (1UL << i);
            }
        }

        return value;
    }

    uint16_t _encodeSigned12(int16_t value) const {
        return static_cast<uint16_t>(value) & 0x0FFF;
    }

    int16_t _signExtend12(uint16_t value) const {
        value &= 0x0FFF;
        if ((value & 0x0800) != 0) {
            value |= 0xF000;
        }
        return static_cast<int16_t>(value);
    }

    unsigned long _clampScaledUnsigned(double value, float scale) const {
        const long scaled = lround(value * scale);

        if (scaled > 65535L) return 65535UL;
        if (scaled < 0L) return 0UL;
        return static_cast<unsigned long>(scaled);
    }

    int16_t _clampScaledSigned(double value, float scale, long minValue, long maxValue) const {
        const long scaled = lround(value * scale);

        if (scaled > maxValue) return static_cast<int16_t>(maxValue);
        if (scaled < minValue) return static_cast<int16_t>(minValue);
        return static_cast<int16_t>(scaled);
    }

};

#endif
