#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

// Canal LEDC dedicado ao buzzer (canais 0-2 reservados para LED RGB)
static constexpr int BUZZER_LEDC_CHANNEL = 3;
static constexpr int BUZZER_LEDC_RESOLUTION = 10;  // 10 bits

class Buzzer {
public:
    Buzzer(int pin)
        : _pin(pin),
          _channel(BUZZER_LEDC_CHANNEL),
          _beepPeriod(0),
          _beepDuration(0),
          _beepFreq(0),
          _lastBeepStart(0),
          _isBeeping(false),
          _emergencyActive(false),
          _emergencyEndTime(0) {}

    /**
    * @brief Configura canal LEDC e anexa o pino do buzzer.
    */
    void begin() {
        ledcAttach(_pin, 2000, BUZZER_LEDC_RESOLUTION);
        _stopTone();  // Garante buzzer desligado ao iniciar
    }

    /**
    * @brief Liga buzzer na frequência especificada (via LEDC — ESP32 não tem tone() nativo).
    * 
    * @param frequency  Frequência de som
    */
    void tone(unsigned int frequency) {
        _startTone(frequency);
    }

    /**
    * @brief Desliga buzzer.
    */
    void noTone() {
        _stopTone();
    }

    /**
    * @brief Define padrão de beep.
    * 
    * @param periodMs   Período de cada beep (ms)
    * @param durationMs Duração do beep (ms)
    * @param frequency  Frequência do som
    * 
    * @warning Só atualiza se os parâmetros mudaram, evitando reset desnecessário do timer.
    */
    void setBeepPattern(unsigned long periodMs, unsigned long durationMs, unsigned int frequency) {
        if (_beepPeriod == periodMs && _beepDuration == durationMs && _beepFreq == frequency) {
            return;  // Mesmo padrão — não resetar temporização
        }
        _beepPeriod = periodMs;
        _beepDuration = durationMs;
        _beepFreq = frequency;
    }

    /**
    * @brief Gerencia temporização do padrão de beep — chamar no loop() a cada iteração.
    * 
    * @code 
    * Lógica de operação:
    *   1. Se buzzer de emergência (SKIB) ativo → não interferir
    *   2. Se não está bipando E tempo desde último beep >= período → iniciar tone
    *   3. Se está bipando E tempo desde início do beep >= duração → parar tone @endcode
    *
    */
    void update() {
        unsigned long now = millis();

        // Prioridade máxima: buzzer de emergência (SKIB — 4s contínuos)
        if (_emergencyActive) {
            if (now >= _emergencyEndTime) {     //TODO: Atualizar a lógica de tempo do buzzer utilizando Timer.h
                _emergencyActive = false;
                _stopTone();
                // Resetar temporização do beep pattern para transição suave
                _lastBeepStart = now;
                _isBeeping = false;
            }
            return;  // Não interferir com emergência
        }

        // Padrão de beep normal — não operar se período é zero (sem padrão definido)
        if (_beepPeriod == 0) {
            return;
        }

        if (!_isBeeping) {
            // Tempo desde o último beep >= período → iniciar novo beep
            if (now - (_lastBeepStart + _beepDuration) >= _beepPeriod) {      //TODO: Atualizar a lógica de tempo do buzzer utilizando Timer.h
                _startTone(_beepFreq);
                _lastBeepStart = now;
                _isBeeping = true;
            }
        } else {
            // Beep ativo — verificar se duração expirou
            if (now - _lastBeepStart >= _beepDuration) {    //TODO: Atualizar a lógica de tempo do buzzer utilizando Timer.h
                _stopTone();
                _isBeeping = false;
            }
        }
    }

    /**
    * @return true se buzzer está emitindo som (beep normal ou emergência).
    */
    bool isPlaying() const {
        return _isBeeping || _emergencyActive;
    }

    /**
    * @brief Toca tom contínuo por uma duração definida — prioridade máxima (cancela padrão de beep).
    * 
    * @param frequency  Frequência do som
    * @param durationMs Duração do som
    * 
    * @warning Usado para buzzer de emergência do SKIB (3136Hz por 4000ms).
    */
    void playEmergency(unsigned int frequency, unsigned long durationMs) {
        _emergencyActive = true;
        _emergencyEndTime = millis() + durationMs;
        _startTone(frequency);
    }

private:
    int _pin;
    int _channel; // TODO: Talvez remover variável (Core 3.x não usa channel)
    unsigned long _beepPeriod;      // Intervalo entre beeps (ms)
    unsigned long _beepDuration;    // Duração de cada beep (ms)
    unsigned int _beepFreq;         // Frequência do beep (Hz)
    unsigned long _lastBeepStart;   // Timestamp do início do último beep/ciclo
    bool _isBeeping;                // true se beep está ativo no momento
    bool _emergencyActive;          // true se buzzer de emergência (SKIB) está ativo
    unsigned long _emergencyEndTime; // Timestamp de fim do buzzer de emergência

    /**
    * @brief Inicia tom na frequência especificada via LEDC.
    * 
    * @param freq  Frequência do som
    */
    void _startTone(unsigned int freq) {
        ledcWriteTone(_pin, freq);
    }

    /**
    * @brief Para o tom — duty cycle zero.
    */
    void _stopTone() {
        ledcWriteTone(_pin, 0);
    }
};

#endif
