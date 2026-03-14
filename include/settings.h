#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

enum CountUnit { UNIT_CPM, UNIT_CPS };
enum DoseUnit { UNIT_USV, UNIT_URH };

struct DeviceSettings {
    // Device
    bool backlightOn = true;
    bool ledEnabled = true;
    uint8_t lcdContrast = 55;
    uint8_t ledBrightness = 50;
    uint32_t autoBacklightTimeout = 30000; // ms, 0 = off
    uint16_t batteryCapacity = 1000; // mAh
    bool topBarShowVoltage = false;
    bool topBarShowRuntime = true;

    // Measurement
    CountUnit countUnit = UNIT_CPM;
    DoseUnit doseUnit = UNIT_USV;
    
    // Factory defaults helper could go here
};

#endif