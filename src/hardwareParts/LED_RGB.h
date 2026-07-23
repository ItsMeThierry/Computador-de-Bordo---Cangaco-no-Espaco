#ifndef LED_RGB_H
#define LED_RGB_H

#include <Arduino.h>
#include "../../config/Constants.h"

class LED_RGB {
public:
    LED_RGB()
        : _blinkInterval(0),
          _lastToggle(0),
          _blinkEnabled(false),
          _state(false),
          _targetR(0),
          _targetG(0),
          _targetB(0) { }

    /**
    * @brief Realiza a configuração inicial do LED RGB.
    */
    void begin(){
        ledcAttach(PIN_LED_RED, 5000, LED_RGB_LEDC_RESOLUTION);
        ledcAttach(PIN_LED_GREEN, 5000, LED_RGB_LEDC_RESOLUTION);
        ledcAttach(PIN_LED_BLUE, 5000, LED_RGB_LEDC_RESOLUTION);
    }

    /**
    * @brief Define o código de cor do LED.
    * 
    * @param red    Valor do canal vermelho
    * @param green  Valor do canal verde
    * @param blue   Valor do canal azul
    */
    void setColor(uint8_t red, uint8_t green, uint8_t blue){
        _targetR = red;
        _targetG = green;
        _targetB = blue;

        if (!_blinkEnabled && !_forceMode) {
            _writePWM(PIN_LED_RED, _targetR);
            _writePWM(PIN_LED_GREEN, _targetG);
            _writePWM(PIN_LED_BLUE, _targetB);
            _state = true;
        }
    }

    /**
    * @brief Define o intervalo de piscada do LED.
    * 
    * @param intervalMs  Intervalo entre piscadas
    */
    void setBlinkInterval(unsigned long intervalMs){
        _blinkInterval = intervalMs;
    }

    /**
    * @brief Atualiza o LED.
    */
    void update(){
        if (!_blinkEnabled || _forceMode) return;

        unsigned long currentMillis = millis();

        if (currentMillis - _lastToggle >= _blinkInterval) { // TODO: Talvez mudar pra Timer.h
            _lastToggle = currentMillis;
            _state = !_state; 

            _writePWM(PIN_LED_RED, (_state) ? _targetR : 0);
            _writePWM(PIN_LED_GREEN, (_state) ? _targetG : 0);
            _writePWM(PIN_LED_BLUE, (_state) ? _targetB : 0);
        }
    }

    /**
    * @brief Define se o LED está ligado ou não.
    * 
    * @param enabled    Estado do LED (on/off)
    */
    void setBlinkEnabled(bool enabled){
        _blinkEnabled = enabled;

        if (!enabled && !_forceMode) {
            _writePWM(PIN_LED_RED, _targetR);
            _writePWM(PIN_LED_GREEN, _targetG);
            _writePWM(PIN_LED_BLUE, _targetB);
            _state = true;
        }
    }

    // TODO: Rever funcionamento do forceColor para a ativação do SKIB
    /**
    * @brief Força o LED a ascender com uma cor definida, ignorando qualquer timer.
    * 
    * @param force  Modo de prioridade (true/false)
    * @param r      Valor do canal vermelho do LED
    * @param g      Valor do canal verde do LED
    * @param b      Valor do canal azul do LED
    */
    void forceColor(bool force, uint8_t r, uint8_t g, uint8_t b){ 
        _forceMode = force;

        if (_forceMode) {
            _forcedR = r;
            _forcedG = g;
            _forcedB = b;

            _writePWM(PIN_LED_RED, _forcedR);
            _writePWM(PIN_LED_GREEN, _forcedG);
            _writePWM(PIN_LED_BLUE, _forcedB);
            _state = true;
        }
    }

private:
    unsigned long _blinkInterval;
    unsigned long _lastToggle;
    bool _blinkEnabled;
    bool _state;
    uint8_t _targetR, _targetG, _targetB;
    uint8_t _forcedR, _forcedG, _forcedB;
    bool _forceMode;

    void _writePWM(int pin, uint8_t value){
        ledcWrite(pin, value);
    }
};

#endif