#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

class Power {
public:
    void begin();
    void update();
    float getBatteryVoltage();
    bool isCharging();

private:
    float filteredVoltage = 0.0f;
    float readRawVoltage();
};

#endif