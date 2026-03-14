#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

class Power {
public:
    void begin();
    void update();
    float getBatteryVoltage();
    bool isCharging();

    // --- Updated Power Constants ---
    const float I_BASE_OFF = 20.0f;    // Main screen, Backlight OFF
    const float I_BASE_ON  = 23.0f;    // Main screen, Backlight ON
    const float I_MENU_ACTIVE = 35.0f; // Any menu state

private:
    float filteredVoltage = 0.0f;
    float readRawVoltage();
};

#endif