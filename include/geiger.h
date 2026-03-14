#ifndef GEIGER_H
#define GEIGER_H

#include <Arduino.h>

class Geiger {
public:
    void begin();
    void update(); // Call in loop
    void setTicker(bool enabled);
    bool isTickerEnabled();
    bool getAndClearPulseFlag();
    
    int getCPM();
    float getuSv();
    uint32_t getTotalPulses();
    const uint16_t* getHistory() const;
    uint8_t getHistoryIndex() const;

    // Conversion factor: Standard SBM-20 is approx 175 CPM per uSv/h
    // Adjust this based on your specific tube
    static constexpr float CPM_TO_USV_FACTOR = 0.0057f; 
};

#endif