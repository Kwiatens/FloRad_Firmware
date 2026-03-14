#ifndef LED_FX_H
#define LED_FX_H

#include <Arduino.h>

class LedFX {
public:
    LedFX();
    void begin();
    void update();
    void triggerBoot();
    void triggerPulse();
    void setStaticColor(uint8_t r, uint8_t g, uint8_t b);
    void stopStaticColor();
    void setBrightness(uint8_t b);
    void setEnabled(bool e);

private:
    void show(); // Internal helper to send data to the LED
    
    enum AnimState { IDLE, BOOT, PULSE_FADE, STATIC };

    bool ledEnabled;
    uint8_t targetBrightness;
    AnimState state;
    unsigned long animStart;
    
    // Track current colors manually
    uint8_t curr_r, curr_g, curr_b;
    uint8_t static_r, static_g, static_b;
};

#endif