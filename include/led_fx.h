#ifndef LED_FX_H
#define LED_FX_H

#include <Adafruit_NeoPixel.h>

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
    // Animation State
    enum AnimState { IDLE, BOOT, PULSE_FADE, STATIC };

    Adafruit_NeoPixel strip;
    bool ledEnabled;
    uint8_t targetBrightness;
    AnimState state;
    unsigned long animStart;
    uint8_t static_r;
    uint8_t static_g;
    uint8_t static_b;
};

#endif