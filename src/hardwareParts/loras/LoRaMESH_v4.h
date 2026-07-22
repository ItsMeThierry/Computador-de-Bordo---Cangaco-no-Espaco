#ifndef LoRaMESH_v4_h
#define LoRaMESH_v4_h

#include <Stream.h>

#define MAX_PAYLOAD_SIZE 232
#define MAX_BUFFER_SIZE 237

#define BW125                     0x00
#define BW250                     0x01
#define BW500                     0x02

#define SF_LoRa_7                 0x07
#define SF_LoRa_8                 0x08
#define SF_LoRa_9                 0x09
#define SF_LoRa_10                0x0A
#define SF_LoRa_11                0x0B
#define SF_LoRa_12                0x0C
#define SF_FSK                    0x00

#define CR4_5                     0x01
#define CR4_6                     0x02
#define CR4_7                     0x03
#define CR4_8                     0x04

#define LoRa_CLASS_A              0x00
#define LoRa_CLASS_C              0x02

#define LoRa_WINDOW_5s            0x00
#define LoRa_WINDOW_10s           0x01
#define LoRa_WINDOW_15s           0x02

#define LoRa_GPIO_MODE_READ       0x00
#define LoRa_GPIO_MODE_CONFIG     0x02
#define LoRa_GPIO_MODE_WRITE      0x01

#define LoRa_GPIO0                0x00
#define LoRa_GPIO1                0x01 
#define LoRa_GPIO2                0x02 
#define LoRa_GPIO3                0x03
#define LoRa_GPIO4                0x04
#define LoRa_GPIO5                0x05
#define LoRa_GPIO6                0x06
#define LoRa_GPIO7                0x07 

#define LoRa_NOT_PULL             0x00
#define LoRa_PULLUP               0x01
#define LoRa_PULLDOWN             0x02

#define LoRa_INOUT_DIGITAL_INPUT  0x00
#define LoRa_INOUT_DIGITAL_OUTPUT 0x01
#define LoRa_INOUT_ANALOG_INPUT   0x03

#define LoRa_LOGICAL_LEVEL_LOW    0x00
#define LoRa_LOGICAL_LEVEL_HIGH   0x01

class LoRaMESH {
public:
    bool debug_serial = false;
    typedef struct {
        uint8_t buffer[MAX_BUFFER_SIZE];
        uint8_t size;
        bool command;
    } Frame_Typedef;

    uint8_t bufferPayload[MAX_PAYLOAD_SIZE] = {0};
    uint8_t payloadSize = 0;

    uint16_t localId = 0;
    uint32_t localUniqueId;
    uint8_t command;
    bool isMaster;

    Frame_Typedef frame;
    uint16_t deviceId = -1;
    uint16_t deviceNet = -1;
    uint32_t deviceUniqueId = -1;

    uint32_t registered_password;

    uint8_t BW, SF, CR, LoRa_class, LoRa_window;

    Stream* SerialLoRa;
    Stream* SerialLoRat;

    LoRaMESH(Stream *_SerialLoRa, Stream *_SerialLoRat = NULL) {
        SerialLoRa = _SerialLoRa;
        SerialLoRat = _SerialLoRat;
    }

    uint16_t ComputeCRC(uint8_t* data_in, uint16_t length) {
        uint16_t i;
        uint8_t bitbang, j;
        uint16_t crc_calc;

        crc_calc = 0xC181;
        for(i=0; i<length; i++) {
            crc_calc ^= (((uint16_t)data_in[i]) & 0x00FF);
            for(j=0; j<8; j++) {
                bitbang = crc_calc;
                crc_calc >>= 1;
                if(bitbang & 1) {
                    crc_calc ^= 0xA001;
                }
            }
        }
        return (crc_calc & 0xFFFF);
    }

    bool PrepareFrameCommand(uint16_t id, uint8_t command, uint8_t* payload, uint8_t payloadSize) {
        if(command == 0) return false;
        if(payload == NULL) return false;
        if(payloadSize == 0) return false;
        
        uint16_t crc = 0;
        frame.size = payloadSize + 5;
        
        frame.buffer[0] = id & 0xFF;
        frame.buffer[1] = (id >> 8) & 0xFF;
        frame.buffer[2] = command;
        
        if((payloadSize >= 0) && (payloadSize < MAX_PAYLOAD_SIZE)) {
            memcpy(&(frame.buffer[3]), payload, payloadSize);
            crc = ComputeCRC((&frame.buffer[0]), payloadSize+3);
            frame.buffer[payloadSize+3] = crc & 0xFF;
            frame.buffer[payloadSize+4] = (crc >> 8) & 0xFF;
        } else {
            memset(&frame.buffer[0], 0, MAX_BUFFER_SIZE);
            return false;
        }

        frame.command = true;
        return true;
    }

    bool PrepareFrameTransp(uint16_t id, uint8_t* payload, uint8_t payloadSize) {
        uint8_t i = 0;
        if(payload == NULL) return false;
        if(id > 1023) return false;
        if(deviceId == -1) return false;
        
        frame.size = payloadSize + 2;
        frame.buffer[i++] = id & 0xFF;
        frame.buffer[i++] = (id >> 8) & 0x03;
        
        if((payloadSize >= 0) && (payloadSize < MAX_PAYLOAD_SIZE)) {
            memcpy(&frame.buffer[i], payload, payloadSize);
        } else {
            memset(&frame.buffer[0], 0, MAX_BUFFER_SIZE);
            return false;
        }

        frame.command = false;
        return true;
    }

    bool SendPacket() {
        if(frame.size == 0) return false;
        if(debug_serial) {
            Serial.print("TX: ");
            printHex(frame.buffer, frame.size);
        }

        if(frame.command)
            SerialLoRa->write(frame.buffer, frame.size);
        else if(SerialLoRat != NULL)
            SerialLoRat->write(frame.buffer, frame.size);
        else
            return false;

        return true;
    }

    bool ReceivePacketCommand(uint16_t* id, uint8_t* command, uint8_t* payload, uint8_t* payloadSize, uint32_t timeout) {
        uint16_t waitNextByte = 500;
        uint8_t i = 0;
        uint16_t crc = 0;

        if(id == NULL) return false;
        if(command == NULL) return false;
        if(payload == NULL) return false;
        if(payloadSize == NULL) return false;

        while(((timeout > 0) || (i > 0)) && (waitNextByte > 0)) {
            if(SerialLoRa->available() > 0) {
                byte a = SerialLoRa->read();
                frame.buffer[i++] = a;
                waitNextByte = 200;
            }
            
            if(i > 0) {
                waitNextByte--;
            }
            timeout--;
            delay(1);
        }
        
        if(i > 0 && debug_serial) {
            Serial.print("RX: ");
            printHex(frame.buffer, i);
        }

        if((timeout == 0) && (i == 0)) return false;
        if(i < 5) return false;

        crc = (uint16_t)frame.buffer[i-2] | ((uint16_t)frame.buffer[i-1] << 8);
        if(ComputeCRC(&frame.buffer[0], i-2) != crc) return false;

        *id = (uint16_t)frame.buffer[0] | ((uint16_t)frame.buffer[1] << 8);
        *command = frame.buffer[2];
        *payloadSize = i - 5;
        memcpy(payload, &frame.buffer[3], i - 5);
        
        return true;
    }

    bool localread() {
        uint8_t b = 0;
        bufferPayload[b] = 0x00;
        bufferPayload[++b] = 0x00;
        bufferPayload[++b] = 0x00;

        PrepareFrameCommand(0, 0xE2, bufferPayload, b + 1);
        SendPacket();
        
        if(ReceivePacketCommand(&localId, &command, bufferPayload, &payloadSize, 1000)) {
            if(command == 0xE2) {
                registered_password = bufferPayload[1] << 8;
                registered_password += bufferPayload[0];
                
                localUniqueId = bufferPayload[2];
                localUniqueId = bufferPayload[3] + (localUniqueId << 8);
                localUniqueId = bufferPayload[4] + (localUniqueId << 8);
                localUniqueId = bufferPayload[5] + (localUniqueId << 8);
                
                return true;
            }
        } 
        return false;
    }

    void begin(bool _debug_serial = false) {
        debug_serial = _debug_serial;
        localread();
        localId = (uint16_t)frame.buffer[0] | ((uint16_t)frame.buffer[1] << 8);
    }

    void printHex(uint8_t* num, uint8_t tam) {
        char hexCar[4];
        uint8_t index;
        for(index = 0; index < tam - 1; index++) {
            sprintf(hexCar, "%02X", num[index]);
            Serial.print(hexCar);
        }
        sprintf(hexCar, "%02X", num[index]);
        Serial.println(hexCar);  
    }
};

#endif
