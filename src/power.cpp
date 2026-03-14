#include "power.h"
#include "pins.h"

// Voltage Divider Config
// R_top = 1M (Connected to Vbat)
// R_bot = 3M (Connected to GND)
// Vout = Vbat * (R_bot / (R_top + R_bot))
//
// NOTE: Adjusted to 2.874f to compensate for voltage sag caused by 
// high impedance (1M/3M) resistors loading the ADC.
const float VOLTAGE_DIVIDER_RATIO = 3.021f;
const float REF_VOLTAGE = 3.3;

void Power::begin() {
    pinMode(PIN_BATT_SENSE, INPUT_ANALOG);
    pinMode(PIN_CHARGING, INPUT_PULLUP); // Assuming open-drain or logic signal
    
    // STM32 specific: set ADC resolution if needed (default 10 or 12 bit)
    analogReadResolution(12); // 0-4095

    // Seed the filter with an initial reading
    filteredVoltage = readRawVoltage();
}

void Power::update() {
    static unsigned long lastUpdate = 0;
    // Update at 10Hz (every 100ms)
    if (millis() - lastUpdate >= 100) {
        lastUpdate = millis();
        float raw = readRawVoltage();
        // Exponential Moving Average (Alpha = 0.1 for smooth 1s response)
        filteredVoltage = (filteredVoltage * 0.9f) + (raw * 0.1f);
    }
}

float Power::readRawVoltage() {
    // Double read to help settle the ADC capacitor with high impedance inputs
    analogRead(PIN_BATT_SENSE);
    delay(2); 
    int raw = analogRead(PIN_BATT_SENSE);
    // Convert to voltage at pin
    float vPin = (raw / 4095.0f) * REF_VOLTAGE;
    // Convert to battery voltage
    return vPin * VOLTAGE_DIVIDER_RATIO;
}

float Power::getBatteryVoltage() {
    return filteredVoltage;
}

bool Power::isCharging() {
    // Active LOW or HIGH? Assuming Active LOW for many charger ICs (STAT pin)
    // Adjust ! if Active HIGH
    return (digitalRead(PIN_CHARGING) == LOW); 
}