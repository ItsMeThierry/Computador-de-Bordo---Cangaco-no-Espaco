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