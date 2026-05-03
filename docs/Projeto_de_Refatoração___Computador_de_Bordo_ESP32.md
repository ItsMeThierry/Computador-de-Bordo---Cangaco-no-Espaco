# Projeto de Refatoração – Computador de Bordo ESP32

## Visão Geral da Arquitetura

O firmware será refatorado seguindo princípios de modularidade e orientação a objetos, respeitando as limitações do ESP32 (memória abundante, necessidade de `EEPROM.commit()`, uso de LEDC para PWM). A arquitetura separa hardware drivers da lógica de aplicação.

---

## Árvore do Projeto

```
src/
├── main.ino                      # Ponto de entrada, setup() e loop()
│
├── config/
│   └── Constants.h               # Todas as constantes configuráveis
│
├── hardware/
│   ├── LED_RGB.h / .cpp          # Controle do LED RGB (PWM via LEDC)
│   ├── Buzzer.h / .cpp           # Controle do buzzer (tone via LEDC)
│   ├── SDCard.h / .cpp           # Interface com cartão SD
│   ├── Altimeter.h / .cpp        # Sensor BMP180 ou BMP280
│   ├── Accelerometer.h / .cpp    # MPU6050 (apenas ESP32)
│   ├── LoRaRadio.h / .cpp        # Módulo Ebyte E220-900T22D (UART)
│   └── GPS.h / .cpp              # GPS NEO-M6N (UART + TinyGPS++)
│
├── logic/
│   ├── FlightStateMachine.h / .cpp   # Máquina de estados + enum FlightState
│   ├── RecoverySystem.h / .cpp       # Sistema de recuperação (SKIB)
│   ├── DataStorage.h / .cpp          # EEPROM + filas circulares pré-voo
│   ├── Telemetry.h / .cpp            # Transmissão LoRa 1Hz
│   └── DataLogger.h / .cpp           # Gravação no SD Card
│
└── utils/
    ├── MovingAverage.h            # Filtro de média móvel (template, sem alocação dinâmica)
    ├── CircularQueue.h            # Fila circular genérica (template)
    ├── Timer.h / .cpp             # Wrapper não-bloqueante para millis()
    └── FlightStateUtils.h         # Utilitário de conversão de FlightState para string
```

---

## Detalhamento dos Arquivos

### 1. `config/Constants.h`

**Responsabilidade:** Centralizar todas as constantes configuráveis do sistema. Não contém tipos de domínio (enums de estado pertencem a seus respectivos módulos).

```cpp
#ifndef CONSTANTS_H
#define CONSTANTS_H

// ========== PARÂMETROS DE VOO ==========
constexpr float ASCENT_THRESHOLD            = 5.0f;   // Altitude para detectar decolagem (m)
constexpr float DESCENT_DETECTION_THRESHOLD = 5.0f;   // Queda para detectar apogeu (m)
constexpr int   WINDOW_SIZE                 = 5;      // Janela da média móvel
constexpr int   ALTITUDE_HISTORY_SIZE       = 5;      // Tamanho do histórico de altitudes (detecção de apogeu)

// ========== PARÂMETROS DE TEMPORIZAÇÃO ==========
constexpr unsigned long SENSOR_READ_INTERVAL_MS      = 50;   // 20 Hz
constexpr unsigned long RECORD_INTERVAL_ASCENT_MS    = 50;   // 20 Hz na subida
constexpr unsigned long RECORD_INTERVAL_DESCENT_MS   = 200;  // 5 Hz na descida
constexpr unsigned long RECORD_INTERVAL_SD_MS        = 10;   // 100 Hz no SD
constexpr unsigned long LORA_TX_INTERVAL_MS          = 1000; // 1 Hz LoRa

// ========== PARÂMETROS DO SKIB ==========
// Nomenclatura alinhada com PRD RF-03: SKIB_DEACTIVATION_TIME
constexpr unsigned long SKIB_DEACTIVATION_TIME_MS = 2000;  // Tempo ativo antes de desligar (ms)
constexpr unsigned long SKIB_BUZZER_DURATION_MS   = 4000;  // Buzzer emergência (ms)

// ========== PARÂMETROS DE FEEDBACK ==========
// LED blink intervals (ms)
constexpr unsigned long LED_BLINK_PRE_FLIGHT_MS = 1000;
constexpr unsigned long LED_BLINK_ASCENT_MS     = 250;
constexpr unsigned long LED_BLINK_RECOVERY_MS   = 1500;

// Buzzer frequencies (Hz)
constexpr unsigned int BEEP_FREQ_PRE_FLIGHT = 1500;
constexpr unsigned int BEEP_FREQ_ASCENT     = 2093;
constexpr unsigned int BEEP_FREQ_RECOVERY   = 3136;
constexpr unsigned int BEEP_FREQ_SKIB       = 2637;

// Buzzer timing (ms)
constexpr unsigned long BEEP_PERIOD_PRE_FLIGHT_MS = 3000;
constexpr unsigned long BEEP_PERIOD_ASCENT_MS     = 1000;
constexpr unsigned long BEEP_PERIOD_RECOVERY_MS   = 500;
constexpr unsigned long BEEP_DURATION_MS          = 400;

// ========== PARÂMETROS DE ARMAZENAMENTO ==========
constexpr int   MAX_FLIGHT_DATA_POINTS  = 497;   // Pontos na EEPROM
constexpr int   PRE_FLIGHT_QUEUE_SIZE   = 4;     // Pontos pré-voo (altitude E aceleração)
constexpr float ALTITUDE_SCALE_FACTOR  = 10.0f;  // Para conversão short int

// ========== COMUNICAÇÃO SERIAL ==========
// Constantes alinhadas com PRD RF-12 e RF-13
constexpr unsigned long GPS_UART_BAUD  = 9600;   // Baudrate UART GPS NEO-M6N
constexpr unsigned long LORA_UART_BAUD = 9600;   // Baudrate UART E220-900T22D

// ========== PINAGEM (ESP32 DevKit V1) ==========
// SKIB e Feedback
constexpr int PIN_SKIB      = 4;
constexpr int PIN_BUZZER    = 13;
constexpr int PIN_LED_RED   = 26;
constexpr int PIN_LED_GREEN = 25;
constexpr int PIN_LED_BLUE  = 27;

// SD Card (SPI)
constexpr int PIN_SD_CS    = 5;
constexpr int PIN_SPI_MOSI = 23;
constexpr int PIN_SPI_MISO = 19;
constexpr int PIN_SPI_SCK  = 18;

// I2C (BMP180/280 + MPU6050)
constexpr int PIN_I2C_SDA = 21;
constexpr int PIN_I2C_SCL = 22;

// GPS (UART2)
constexpr int PIN_GPS_RX = 16;
constexpr int PIN_GPS_TX = 17;

// LoRa E220 (UART1)
// ATENÇÃO: GPIO12 (PIN_LORA_RX) é pino strapping — pull-down 10kΩ obrigatório
constexpr int PIN_LORA_RX = 14;   // TX do módulo → RX do ESP
constexpr int PIN_LORA_TX = 12;   // RX do módulo → TX do ESP (strapping!)
constexpr int PIN_LORA_M0 = 32;
constexpr int PIN_LORA_M1 = 33;

#endif
```

> **Nota de design:** O `Constants.h` deve conter apenas valores de configuração numéricos e de pinagem, não tipos de domínio (Ex: O enum `FlightState`).

---

### 2. `hardware/LED_RGB.h` / `.cpp`

**Responsabilidade:** Controlar o LED RGB via PWM (LEDC no ESP32).

```cpp
// LED_RGB.h
#ifndef LED_RGB_H
#define LED_RGB_H

#include <Arduino.h>

class LED_RGB {
public:
    LED_RGB(int pinRed, int pinGreen, int pinBlue);
    void begin();
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    void setBlinkInterval(unsigned long intervalMs);
    void update();
    void setBlinkEnabled(bool enabled);
    void forceColor(bool force, uint8_t r, uint8_t g, uint8_t b);

private:
    int _pinR, _pinG, _pinB;
    int _channelR, _channelG, _channelB;
    unsigned long _blinkInterval;
    unsigned long _lastToggle;
    bool _blinkEnabled;
    bool _state;
    uint8_t _targetR, _targetG, _targetB;
    uint8_t _forcedR, _forcedG, _forcedB;
    bool _forceMode;

    void _writePWM(int channel, uint8_t value);
};

#endif
```

**Funções:**

| Função                       | Descrição                                                               |
| ---------------------------- | ----------------------------------------------------------------------- |
| `LED_RGB(pinR, pinG, pinB)`  | Construtor – armazena pinos                                             |
| `begin()`                    | Configura 3 canais LEDC (freq 5kHz, res 8 bits) e anexa os pinos        |
| `setColor(r, g, b)`          | Define cor alvo (0-255 cada canal)                                      |
| `setBlinkInterval(ms)`       | Define intervalo de piscagem (ex: 1000ms = 1Hz)                         |
| `update()`                   | **Chamar no loop()** – alterna LED com base em millis() se blinkEnabled |
| `setBlinkEnabled(bool)`      | Habilita/desabilita piscagem automática                                 |
| `forceColor(force, r, g, b)` | Modo força – ignora blink e fixa cor (ex: erro no boot)                 |
| `_writePWM(channel, value)`  | Interna – escreve valor PWM ao canal LEDC                               |

---

### 3. `hardware/Buzzer.h` / `.cpp`

**Responsabilidade:** Controlar o buzzer via LEDC (pois ESP32 não tem `tone()` nativo).

```cpp
// Buzzer.h
#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

class Buzzer {
public:
    Buzzer(int pin);
    void begin();
    void tone(unsigned int frequency);
    void noTone();
    void setBeepPattern(unsigned long periodMs, unsigned long durationMs, unsigned int frequency);
    void update();
    bool isPlaying() const;
    void playEmergency(unsigned int frequency, unsigned long durationMs);

private:
    int _pin;
    int _channel;
    unsigned long _beepPeriod;
    unsigned long _beepDuration;
    unsigned int _beepFreq;
    unsigned long _lastBeepStart;
    bool _isBeeping;
    bool _emergencyActive;
    unsigned long _emergencyEndTime;

    void _startTone(unsigned int freq);
    void _stopTone();
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `Buzzer(pin)` | Construtor – armazena pino |
| `begin()` | Configura canal LEDC (freq variável, resolução 10 bits) e anexa pino |
| `tone(freq)` | Liga buzzer na frequência especificada |
| `noTone()` | Desliga buzzer |
| `setBeepPattern(period, duration, freq)` | Define padrão: beep de 'duration' ms a cada 'period' ms |
| `update()` | Gerencia temporização do padrão de beep (não-bloqueante) |
| `isPlaying()` | Retorna true se buzzer está ativo |
| `playEmergency(freq, durationMs)` | Toca contínuo por X ms (prioridade máxima, cancela padrão) |

---

### 4. `hardware/SDCard.h` / `.cpp`

**Responsabilidade:** Inicializar e gravar dados no cartão SD.

```cpp
// SDCard.h
#ifndef SDCARD_H
#define SDCARD_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class SDCard {
public:
    SDCard(int csPin);
    bool begin();
    bool isReady() const;
    bool writeLine(const String& line);
    void flush();
    void setHeader(const String& header);
    void createNewLogFile();

private:
    int _csPin;
    File _logFile;
    bool _ready;
    String _header;
    String _currentFileName;

    String _findNextFileName();
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `SDCard(csPin)` | Construtor – armazena pino CS |
| `begin()` | Inicializa SPI e SD, cria arquivo LOG_XX.CSV, escreve cabeçalho. Retorna sucesso |
| `isReady()` | Retorna true se SD pronto para gravação |
| `writeLine(line)` | Grava linha no arquivo, faz flush imediato (segurança) |
| `flush()` | Força flush do buffer do arquivo |
| `setHeader(header)` | Define cabeçalho CSV (ex: "Timestamp,Altitude,AccelX,AccelY,AccelZ,State") |
| `createNewLogFile()` | Cria novo arquivo (auto-incremento LOG_00 a LOG_99) |
| `_findNextFileName()` | Interna – procura próximo nome de arquivo disponível |

---

### 5. `hardware/Altimeter.h` / `.cpp`

**Responsabilidade:** Interface com sensor barométrico (BMP180 ou BMP280).

```cpp
// Altimeter.h
#ifndef ALTIMETER_H
#define ALTIMETER_H

#include <Arduino.h>
#include <Wire.h>
#include <SFE_BMP180.h>  // ou Adafruit_BMP280.h

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
    SFE_BMP180 _sensor;
    bool _connected;
    double _baseline;
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `Altimeter()` | Construtor |
| `begin()` | Inicializa sensor I2C, retorna true se OK |
| `isConnected()` | Retorna status da conexão |
| `readPressure()` | Executa ciclo completo: temperatura → pressão. Retorna -1 em erro |
| `calculateAltitude(p, baseline)` | Aplica fórmula barométrica |
| `getBaseline()` | Retorna baseline atual |
| `resetBaseline()` | Mede pressão atual e define como nova baseline |

---

### 6. `hardware/Accelerometer.h` / `.cpp`

**Responsabilidade:** Interface com MPU6050 (apenas ESP32).

```cpp
// Accelerometer.h
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
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `Accelerometer()` | Construtor |
| `begin()` | Inicializa MPU6050 via I2C, configura range ±8G, filtro 21Hz |
| `isConnected()` | Retorna status |
| `readAcceleration()` | Lê aceleração nos 3 eixos; campo `valid` indica sucesso |

> **Nota:** O campo `valid` foi adicionado ao `AccelData` para que o consumidor (main.ino) possa ignorar leituras falhas sem travar o fluxo, alinhado com RNF-02.

---

### 7. `hardware/GPSSensor.h` / `.cpp`

**Responsabilidade:** Interface com GPS NEO-M6N via UART.

```cpp
// GPSSensor.h
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
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `GPSSensor(rx, tx)` | Construtor |
| `begin()` | `Serial2.begin(GPS_UART_BAUD, SERIAL_8N1, rx, tx)` |
| `update()` | Lê todos bytes disponíveis, alimenta TinyGPS++; **deve ser chamado em todo ciclo do loop(), não condicionado à leitura do barômetro** |
| `getLatestData()` | Retorna struct com lat, lon, alt, fixQuality |
| `hasFix()` | Retorna true se fixQuality ≥ 1 |
| `_parseData()` | Interna – extrai campos do TinyGPS++ |

---

### 8. `hardware/LoRaRadio.h` / `.cpp`

**Responsabilidade:** Interface com módulo Ebyte E220-900T22D via UART.

```cpp
// LoRaRadio.h
#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <Arduino.h>

class LoRaRadio {
public:
    LoRaRadio(int rxPin, int txPin, int m0Pin, int m1Pin);
    void begin();
    void setModeNormal();
    bool transmit(const String& message);
    bool isReady() const;

private:
    int _rxPin, _txPin, _m0Pin, _m1Pin;
    bool _initialized;

    void _setMode(uint8_t m0, uint8_t m1);
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `LoRaRadio(rx, tx, m0, m1)` | Construtor |
| `begin()` | Configura M0/M1 como OUTPUT, setModeNormal(), `Serial1.begin(LORA_UART_BAUD)` — usa constante de `Constants.h` |
| `setModeNormal()` | M0=LOW, M1=LOW – modo transparente |
| `transmit(message)` | `Serial1.println(message)`, retorna true |
| `isReady()` | Retorna true se inicializado |
| `_setMode(m0, m1)` | Interna – escreve nos pinos de modo |

---

## 9. `utils/MovingAverage.h`

**Responsabilidade:** Filtro de média móvel simples.

```cpp
// MovingAverage.h
#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include "../config/Constants.h"

// Template com tamanho fixo em tempo de compilação — sem new/delete no heap do ESP32
template<int N>
class MovingAverage {
public:
    MovingAverage() : _index(0), _count(0), _sum(0.0) {
        for (int i = 0; i < N; i++) _buffer[i] = 0.0;
    }

    double filter(double input) {
        _sum -= _buffer[_index];
        _buffer[_index] = input;
        _sum += input;
        _index = (_index + 1) % N;
        if (_count < N) _count++;
        return _sum / _count;
    }

    void reset() {
        _index = 0;
        _count = 0;
        _sum   = 0.0;
        for (int i = 0; i < N; i++) _buffer[i] = 0.0;
    }

private:
    double _buffer[N];
    int _index;
    int _count;
    double _sum;
};

#endif
```

> **Nota de design:** Em microcontroladores, alocação dinâmica frequente pode causar fragmentação de heap. Com `WINDOW_SIZE = 5` constante em compilação, o template elimina esse risco e melhora a performance.

**Uso:**
```cpp
MovingAverage<WINDOW_SIZE> altitudeFilter;
```

---

## 10. `utils/CircularQueue.h`

**Responsabilidade:** Fila circular genérica para dados pré-voo (altitude e aceleração).

```cpp
// CircularQueue.h
#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

template<typename T, int N>
class CircularQueue {
public:
    CircularQueue() : _head(0), _size(0) {}

    void push(const T& value) {
        _buffer[(_head + _size) % N] = value;
        if (_size < N) _size++;
        else _head = (_head + 1) % N;  // sobrescreve o mais antigo
    }

    T get(int index) const {
        return _buffer[(_head + index) % N];
    }

    int size() const { return _size; }
    bool isFull() const { return _size == N; }
    void clear() { _head = 0; _size = 0; }

private:
    T _buffer[N];
    int _head;
    int _size;
};

#endif
```

---

## 11. `utils/Timer.h` / `.cpp`

**Responsabilidade:** Encapsular padrão `millis()` não-bloqueante, eliminando a repetição de `if (now - lastX >= intervalX)` espalhada pelo código.

```cpp
// Timer.h
#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

class Timer {
public:
    Timer(unsigned long intervalMs);
    void setInterval(unsigned long intervalMs);
    bool isReady();           // Retorna true se o intervalo passou; reinicia automaticamente
    bool isReadyOnce();       // Retorna true apenas uma vez; não reinicia até reset()
    void reset();
    unsigned long elapsed() const;

private:
    unsigned long _interval;
    unsigned long _lastTick;
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `Timer(intervalMs)` | Construtor – define intervalo e inicializa `_lastTick = millis()` |
| `setInterval(ms)` | Altera intervalo em tempo de execução |
| `isReady()` | Retorna true se `millis() - _lastTick >= _interval`; atualiza `_lastTick` automaticamente |
| `isReadyOnce()` | Como `isReady()` mas não reinicia — para eventos únicos |
| `reset()` | Reinicia o timer |
| `elapsed()` | Retorna ms desde o último tick |

> **Uso nos subsistemas:** `DataLogger`, `Telemetry`, e o bloco de leitura do sensor em `main.ino` devem usar instâncias de `Timer` em vez de pares `lastX` + `if (now - lastX >= interval)`. Isso reduz variáveis globais e uniformiza o padrão de temporização.

---

## 12. `utils/FlightStateUtils.h`

**Responsabilidade:** Conversão de `FlightState` para representações textuais.

```cpp
// FlightStateUtils.h
#ifndef FLIGHT_STATE_UTILS_H
#define FLIGHT_STATE_UTILS_H

#include "../logic/FlightStateMachine.h"

namespace FlightStateUtils {
    // Para CSV do SD Card: "PRE_FLIGHT", "ASCENT", "RECOVERY"
    inline const char* toString(FlightState state) {
        switch(state) {
            case FlightState::PRE_FLIGHT: return "PRE_FLIGHT";
            case FlightState::ASCENT:     return "ASCENT";
            case FlightState::RECOVERY:   return "RECOVERY";
            default:                      return "UNKNOWN";
        }
    }

    // Para campo numérico do pacote LoRa: 0, 1, 2
    inline uint8_t toInt(FlightState state) {
        return static_cast<uint8_t>(state);
    }
}

#endif
```

---

### 13. `logic/FlightStateMachine.h` / `.cpp`

**Responsabilidade:** Máquina de estados do voo. **Este é o arquivo que define o enum `FlightState`**, pois é um tipo de domínio da lógica de voo, não uma constante de configuração.

```cpp
// FlightStateMachine.h
#ifndef FLIGHT_STATE_MACHINE_H
#define FLIGHT_STATE_MACHINE_H

#include <Arduino.h>
#include "../config/Constants.h"

// Enum de domínio: pertence à lógica de voo, não a Constants.h
enum class FlightState : uint8_t {
    PRE_FLIGHT = 0,
    ASCENT     = 1,
    RECOVERY   = 2
};

class FlightStateMachine {
public:
    FlightStateMachine();

    // altitudeHistory: array circular com ALTITUDE_HISTORY_SIZE elementos
    // gerenciado internamente — não mais exposto no main.ino
    void update(double currentAltitude, double maxAltitude);

    FlightState getCurrentState() const;
    void forceState(FlightState newState);
    bool wasApogeeDetected() const;
    unsigned long getFlightStartTime() const;   // millis() da transição para ASCENT

    // Acesso ao histórico para DataStorage/DataLogger se necessário
    double getHistory(int index) const;

private:
    FlightState _state;
    bool _apogeeDetected;
    bool _armed;
    unsigned long _flightStartTime;

    // Histórico de altitudes suavizadas — gerenciado internamente
    double _altitudeHistory[ALTITUDE_HISTORY_SIZE];

    void _pushHistory(double altitude);
    bool _shouldTransitionToAscent(double altitude) const;
    bool _shouldTransitionToRecovery(double currentAlt, double maxAlt) const;
};

#endif
```

**Funções:**

| Função                          | Descrição                                                                     |
| ------------------------------- | ----------------------------------------------------------------------------- |
| `update(alt, maxAlt)`           | Atualiza histórico interno e avalia transições                                |
| `getCurrentState()`             | Retorna estado atual                                                          |
| `forceState(newState)`          | Força estado (ex: reset após pouso)                                           |
| `wasApogeeDetected()`           | Retorna true se apogeu já foi detectado                                       |
| `getFlightStartTime()`          | Fonte única de verdade para timestamp de início de voo                        |
| `getHistory(index)`             | Acessa histórico de altitude (0 = mais recente)                               |
| `_pushHistory(altitude)`        | Interna – shift circular do histórico                                         |
| `_shouldTransitionToAscent()`   | Interna – altitude ≥ ASCENT_THRESHOLD                                         |
| `_shouldTransitionToRecovery()` | Interna – altitude caiu DESCENT_DETECTION_THRESHOLD abaixo da máxima E armado |


---

### 14. `logic/RecoverySystem.h` / `.cpp`

**Responsabilidade:** Acionamento do SKIB e buzzer de emergência.

```cpp
// RecoverySystem.h
#ifndef RECOVERY_SYSTEM_H
#define RECOVERY_SYSTEM_H

#include <Arduino.h>
#include "../hardware/Buzzer.h"

class RecoverySystem {
public:
    RecoverySystem(int skibPin, Buzzer* buzzer);
    void begin();                      // Primeira instrução do setup() — GPIO LOW antes de tudo
    void activate(double maxAltitude);
    void update();
    bool isActivated() const;
    bool isSkibActive() const;

private:
    int _skibPin;
    Buzzer* _buzzer;
    bool _activated;
    bool _skibActive;
    unsigned long _skibActivationTime;
    double _maxAltitude;

    void _activateSkib();
    void _deactivateSkib();
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `RecoverySystem(pin, buzzer)` | Construtor |
| `begin()` | `pinMode OUTPUT` + `digitalWrite LOW` — deve ser a primeira instrução do `setup()` |
| `activate(maxAltitude)` | Ativa SKIB, dispara buzzer de emergência 4s, marca como ativado |
| `update()` | Desativa SKIB após `SKIB_DEACTIVATION_TIME_MS` |
| `isActivated()` | Retorna true se sistema foi acionado (mesmo após timeout) |
| `isSkibActive()` | Retorna true se SKIB ainda está HIGH |
| `_activateSkib()` | Interna – SKIB HIGH |
| `_deactivateSkib()` | Interna – SKIB LOW |

---

### 15. `logic/DataStorage.h` / `.cpp`

**Responsabilidade:** Gerenciamento da EEPROM, incluindo filas pré-voo de altitude e de aceleração.

```cpp
// DataStorage.h
#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "../utils/CircularQueue.h"
#include "../hardware/Accelerometer.h"
#include "../config/Constants.h"

class DataStorage {
public:
    DataStorage();
    void begin();   // EEPROM.begin(1000) — chamar ANTES de qualquer leitura

    // Fila pré-voo: altitude + aceleração (ESP32 grava ambas, PRD seção 2.3)
    void savePreFlightPoints(
        const CircularQueue<double, PRE_FLIGHT_QUEUE_SIZE>& altQueue,
        const CircularQueue<AccelData, PRE_FLIGHT_QUEUE_SIZE>& accelQueue
    );

    void saveAltitude(double altitude, unsigned int index);
    double readAltitude(unsigned int index);
    void saveMaxAltitude(double maxAltitude);
    double readMaxAltitude();
    void saveBaseline(double baseline);
    double readBaseline();
    unsigned int getRecordCount() const;
    void setRecordCount(unsigned int count);
    void incrementRecordCount();

private:
    unsigned int _recordCount;
    int _getAltitudeAddress(unsigned int index) const;
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `begin()` | `EEPROM.begin(1000)` — **deve ser chamado antes de qualquer operação de leitura/escrita** |
| `savePreFlightPoints(altQueue, accelQueue)` | Salva altitude E aceleração dos pontos pré-voo (ESP32) |
| `saveAltitude(alt, index)` | Converte para short (×10), salva 2 bytes, com commit() |
| `readAltitude(index)` | Lê short, converte para double (÷10) |
| `saveMaxAltitude(alt)` | Salva altitude máxima (2 bytes) no endereço 0x00 |
| `readMaxAltitude()` | Lê altitude máxima salva |
| `saveBaseline(pressure)` | Salva baseline (4 bytes) no endereço 0x02 |
| `readBaseline()` | Lê baseline |
| `getRecordCount()` | Retorna número de pontos salvos |
| `setRecordCount(count)` | Define contador |
| `incrementRecordCount()` | Incrementa contador |
| `_getAltitudeAddress(index)` | Interna – calcula endereço baseado no índice |

---

### 16. `logic/DataLogger.h` / `.cpp`

**Responsabilidade:** Gravação de dados no SD Card (alta frequência, 100Hz). Grava apenas após sair do estado PRE_FLIGHT.

```cpp
// DataLogger.h
#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "../hardware/SDCard.h"
#include "../hardware/Accelerometer.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"

class DataLogger {
public:
    DataLogger(SDCard* sdCard);
    void begin();

    // update() recebe todos os dados necessários — sem dependência de variáveis globais
    void update(unsigned long flightTimeMs, double altitude,
                const AccelData& accel, FlightState state);

    void setEnabled(bool enabled);

private:
    SDCard* _sdCard;
    bool _enabled;
    Timer _timer;

    String _formatLine(unsigned long flightTimeMs, double altitude,
                       const AccelData& accel, FlightState state);
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `DataLogger(sdCard)` | Construtor – inicializa Timer com RECORD_INTERVAL_SD_MS |
| `begin()` | Define cabeçalho CSV: `Timestamp,Altitude,AccelX,AccelY,AccelZ,State` |
| `update(flightTimeMs, alt, accel, state)` | Se `state != PRE_FLIGHT` e timer pronto: grava linha no SD |
| `setEnabled(enabled)` | Habilita/desabilita gravação |
| `_formatLine(...)` | Interna – formata linha CSV usando `FlightStateUtils::toString()` |


---

### 17. `logic/Telemetry.h` / `.cpp`

**Responsabilidade:** Transmissão LoRa 1Hz.

```cpp
// Telemetry.h
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include "../hardware/LoRaRadio.h"
#include "../hardware/GPSSensor.h"
#include "../logic/FlightStateMachine.h"
#include "../utils/Timer.h"
#include "../utils/FlightStateUtils.h"

class Telemetry {
public:
    Telemetry(LoRaRadio* radio, GPSSensor* gps);
    void begin();
    void update(unsigned long flightTimeMs, double altitudeBaro, FlightState state);
    void setEnabled(bool enabled);

private:
    LoRaRadio* _radio;
    GPSSensor* _gps;
    bool _enabled;
    Timer _timer;

    String _buildPacket(unsigned long flightTimeMs, double altitudeBaro,
                        FlightState state, const GPSData& gpsData);
};

#endif
```

**Funções:**

| Função | Descrição |
|--------|-----------|
| `Telemetry(radio, gps)` | Construtor – inicializa Timer com LORA_TX_INTERVAL_MS |
| `begin()` | Verifica rádio inicializado |
| `update(flightTimeMs, altBaro, state)` | Se timer pronto: monta pacote e transmite |
| `setEnabled(enabled)` | Liga/desliga telemetria |
| `_buildPacket(...)` | Monta string: `CANGACO,timestamp,lat,lon,alt_gps,alt_baro,estado_int,fix` usando `FlightStateUtils::toInt()` |

---

### 18. `main.ino`

**Responsabilidade:** Ponto de entrada, inicialização e loop principal.

```cpp
// main.ino
#include "config/Constants.h"
#include "hardware/LED_RGB.h"
#include "hardware/Buzzer.h"
#include "hardware/SDCard.h"
#include "hardware/Altimeter.h"
#include "hardware/Accelerometer.h"
#include "hardware/GPSSensor.h"
#include "hardware/LoRaRadio.h"
#include "utils/MovingAverage.h"
#include "utils/CircularQueue.h"
#include "logic/FlightStateMachine.h"
#include "logic/RecoverySystem.h"
#include "logic/DataStorage.h"
#include "logic/DataLogger.h"
#include "logic/Telemetry.h"

// ===== INSTÂNCIAS GLOBAIS =====
LED_RGB      led(PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE);
Buzzer       buzzer(PIN_BUZZER);
SDCard       sdCard(PIN_SD_CS);
Altimeter    altimeter;
Accelerometer accelerometer;
GPSSensor    gps(PIN_GPS_RX, PIN_GPS_TX);
LoRaRadio    lora(PIN_LORA_RX, PIN_LORA_TX, PIN_LORA_M0, PIN_LORA_M1);

MovingAverage<WINDOW_SIZE> altitudeFilter;

// Filas pré-voo: altitude + aceleração (PRD seção 2.3)
CircularQueue<double, PRE_FLIGHT_QUEUE_SIZE>   preFlightAltQueue;
CircularQueue<AccelData, PRE_FLIGHT_QUEUE_SIZE> preFlightAccelQueue;

FlightStateMachine stateMachine;
RecoverySystem     recoverySystem(PIN_SKIB, &buzzer);
DataStorage        storage;
DataLogger         dataLogger(&sdCard);
Telemetry          telemetry(&lora, &gps);

// Variáveis de estado (apenas o que não pertence a nenhum subsistema)
double smoothedAltitude = 0.0;
double maxAltitude      = -999.0;
double baseline         = 1013.25;

Timer sensorTimer(SENSOR_READ_INTERVAL_MS);
Timer eepromTimer(RECORD_INTERVAL_ASCENT_MS);  // Intervalo ajustado dinamicamente

// ===== SETUP =====
void setup() {
    // PRIMEIRÍSSIMO: SKIB seguro antes de qualquer outra instrução
    recoverySystem.begin();

    Serial.begin(115200);
    Serial.println(F("Iniciando Computador de Bordo ESP32"));

    // Indicação de boot
    led.begin();
    led.setColor(0, 0, 255);  // Azul durante boot
    led.setBlinkEnabled(false);

    buzzer.begin();

    // *** CRÍTICO: EEPROM.begin() ANTES de qualquer leitura ***
    storage.begin();

    // Inicializar periféricos
    bool sdOk  = sdCard.begin();
    bool altOk = altimeter.begin();
    bool accOk = accelerometer.begin();  // Não-crítico
    gps.begin();
    lora.begin();
    telemetry.begin();

    // Verificação de erros críticos
    if (!sdOk || !altOk) {
        _errorBlink(!altOk && !sdOk, !sdOk, !altOk);
        while(1);
    }

    led.setColor(0, 255, 0);  // Verde — boot OK
    delay(1000);

    // Calibrar baseline (após storage.begin())
    baseline = storage.readBaseline();
    if (baseline <= 0) {
        baseline = altimeter.readPressure();
    }
    altimeter.resetBaseline();
    storage.saveBaseline(altimeter.getBaseline());

    dataLogger.begin();

    Serial.println(F("Sistema pronto!"));
}

// ===== LOOP PRINCIPAL =====
void loop() {
    // GPS atualizado em TODO ciclo — não condicionado ao sensor barométrico
    gps.update();

    // Leitura do barômetro a 20Hz
    if (sensorTimer.isReady()) {

        double pressure = altimeter.readPressure();
        if (pressure > 0) {
            double rawAltitude = altimeter.calculateAltitude(pressure, altimeter.getBaseline());
            smoothedAltitude   = altitudeFilter.filter(rawAltitude);

            if (smoothedAltitude > maxAltitude) {
                maxAltitude = smoothedAltitude;
            }

            // Leitura do acelerômetro (sincronizada com barômetro)
            AccelData accel = accelerometer.readAcceleration();

            // Máquina de estados — histórico gerenciado internamente pelo FSM
            FlightState oldState = stateMachine.getCurrentState();
            stateMachine.update(smoothedAltitude, maxAltitude);
            FlightState newState = stateMachine.getCurrentState();

            // Transição PRE_FLIGHT → ASCENT
            if (oldState == FlightState::PRE_FLIGHT && newState == FlightState::ASCENT) {
                storage.savePreFlightPoints(preFlightAltQueue, preFlightAccelQueue);
                eepromTimer.setInterval(RECORD_INTERVAL_ASCENT_MS);
            }

            // Transição ASCENT → RECOVERY
            if (oldState == FlightState::ASCENT && newState == FlightState::RECOVERY) {
                recoverySystem.activate(maxAltitude);
                storage.saveMaxAltitude(maxAltitude);
                eepromTimer.setInterval(RECORD_INTERVAL_DESCENT_MS);
            }

            // Fila pré-voo
            if (newState == FlightState::PRE_FLIGHT) {
                preFlightAltQueue.push(smoothedAltitude);
                if (accel.valid) preFlightAccelQueue.push(accel);
            }

            // Gravação EEPROM
            unsigned int recordCount = storage.getRecordCount();
            if (newState != FlightState::PRE_FLIGHT &&
                eepromTimer.isReady() &&
                recordCount < MAX_FLIGHT_DATA_POINTS) {

                storage.saveAltitude(smoothedAltitude, recordCount);
                storage.incrementRecordCount();
            }

            // Timestamp de voo — fonte única: FlightStateMachine
            unsigned long flightTime = (newState == FlightState::PRE_FLIGHT)
                ? 0
                : (millis() - stateMachine.getFlightStartTime());

            // SD Card e Telemetria recebem dados explicitamente
            dataLogger.update(flightTime, smoothedAltitude, accel, newState);
            telemetry.update(flightTime, smoothedAltitude, newState);
        }
    }

    // Subsistemas de feedback e recuperação
    led.update();
    buzzer.update();
    recoverySystem.update();

    // Feedback visual/sonoro por estado
    _updateStateFeedback(stateMachine.getCurrentState());
}

// ===== FUNÇÕES AUXILIARES =====

void _updateStateFeedback(FlightState state) {
    switch(state) {
        case FlightState::PRE_FLIGHT:
            led.setBlinkInterval(LED_BLINK_PRE_FLIGHT_MS);
            led.setColor(0, 0, 255);
            buzzer.setBeepPattern(BEEP_PERIOD_PRE_FLIGHT_MS, BEEP_DURATION_MS, BEEP_FREQ_PRE_FLIGHT);
            break;
        case FlightState::ASCENT:
            led.setBlinkInterval(LED_BLINK_ASCENT_MS);
            led.setColor(0, 255, 255);
            buzzer.setBeepPattern(BEEP_PERIOD_ASCENT_MS, BEEP_DURATION_MS, BEEP_FREQ_ASCENT);
            break;
        case FlightState::RECOVERY:
            led.setBlinkInterval(LED_BLINK_RECOVERY_MS);
            led.setColor(255, 165, 0);
            if (!recoverySystem.isActivated()) {
                buzzer.setBeepPattern(BEEP_PERIOD_RECOVERY_MS, BEEP_DURATION_MS, BEEP_FREQ_RECOVERY);
            } else {
                buzzer.setBeepPattern(BEEP_PERIOD_RECOVERY_MS, BEEP_DURATION_MS, BEEP_FREQ_SKIB);
            }
            break;
    }
    led.setBlinkEnabled(true);
}

void _errorBlink(bool bothFail, bool sdFail, bool altFail) {
    while(1) {
        if (bothFail)      led.setColor(255, 0, 0);
        else if (sdFail)   led.setColor(255, 255, 0);
        else if (altFail)  led.setColor(255, 0, 255);
        delay(500);
        led.setColor(0, 0, 0);
        delay(500);
    }
}
```

---

## Resumo da Estrutura de Arquivos

| Arquivo | Função Principal |
|---------|------------------|
| `main.ino` | Setup, loop, orquestração geral |
| `config/Constants.h` | Constantes, pinagem e baudrates (sem enums de domínio) |
| `hardware/LED_RGB.cpp` | Controle PWM do LED RGB |
| `hardware/Buzzer.cpp` | Controle PWM do buzzer |
| `hardware/SDCard.cpp` | Inicialização e gravação no SD |
| `hardware/Altimeter.cpp` | Leitura BMP180/280 |
| `hardware/Accelerometer.cpp` | Leitura MPU6050 (AccelData com campo valid) |
| `hardware/GPSSensor.cpp` | Leitura NMEA do GPS (update() em todo ciclo) |
| `hardware/LoRaRadio.cpp` | Transmissão via E220 (usa LORA_UART_BAUD) |
| `utils/MovingAverage.h` | Filtro média móvel (template, sem heap) |
| `utils/CircularQueue.h` | Fila circular genérica |
| `utils/Timer.h / .cpp` | Wrapper millis() não-bloqueante |
| `utils/FlightStateUtils.h` | toString() e toInt() para FlightState |
| `logic/FlightStateMachine.cpp` | Máquina de estados + enum FlightState + histórico interno |
| `logic/RecoverySystem.cpp` | Acionamento SKIB (usa SKIB_DEACTIVATION_TIME_MS) |
| `logic/DataStorage.cpp` | EEPROM com commit(); fila pré-voo dupla (alt + accel) |
| `logic/DataLogger.cpp` | Gravação SD 100Hz; recebe dados via parâmetros |
| `logic/Telemetry.cpp` | Transmissão LoRa 1Hz; usa FlightStateUtils::toInt() |

---

## Considerações Específicas do ESP32

1. **EEPROM**: `EEPROM.begin(1000)` deve ser chamado em `storage.begin()`, que por sua vez deve ser invocado **antes** de qualquer chamada a `readBaseline()` ou `readMaxAltitude()` no `setup()`.

2. **PWM (LEDC)**: LEDs RGB e buzzer usam `ledcSetup()` e `ledcAttachPin()` em vez de `analogWrite()` e `tone()`.

3. **Pinagem strapping**: GPIO12 (TX do LoRa) tem pull-down de 10kΩ obrigatório. GPIO5 (CS do SD) é strapping mas seguro após boot.

4. **Não-bloqueante**: Todo o sistema baseado em `millis()` via a classe `Timer` — nenhum `delay()` exceto na inicialização do sensor BMP180 (inerente à biblioteca) e no delay de boot do LED verde.

5. **Segurança SKIB**: `RecoverySystem::begin()` como **primeira instrução do `setup()`**, antes de qualquer serial ou outra inicialização. Nomenclatura alinhada com PRD: `SKIB_DEACTIVATION_TIME_MS`.

6. **GPS não-bloqueante**: `gps.update()` chamado em todo ciclo do `loop()`, **não** dentro do bloco condicional de leitura do barômetro. O GPS processa bytes UART independentemente da disponibilidade de dados do altímetro.

7. **Sem alocação dinâmica em runtime**: `MovingAverage` usa template; `CircularQueue` usa template. Nenhum `new`/`delete` fora do `setup()`.
