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
