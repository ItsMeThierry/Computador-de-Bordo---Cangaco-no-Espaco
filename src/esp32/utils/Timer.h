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