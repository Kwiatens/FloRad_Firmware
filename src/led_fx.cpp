#include "led_fx.h"
#include "pins.h"

LedFX::LedFX() : 
    strip(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800),
    ledEnabled(true),
    targetBrightness(50),
    state(IDLE),
    animStart(0),
    static_r(0), static_g(0), static_b(0) {}

void LedFX::begin() {
    strip.begin();
    strip.setBrightness(255); // Set library brightness to max, we will scale manually
    strip.show(); // Initialize all pixels to 'off'
}

void LedFX::setEnabled(bool e) { ledEnabled = e; }
void LedFX::setBrightness(uint8_t b) { targetBrightness = b; }

void LedFX::triggerBoot() {
    state = BOOT;
    animStart = millis();
}

void LedFX::triggerPulse() {
    if (!ledEnabled) return;
    state = PULSE_FADE;
    animStart = millis();
    // Color will be scaled by brightness in update()
    strip.show();
}

void LedFX::setStaticColor(uint8_t r, uint8_t g, uint8_t b) {
    state = STATIC;
    static_r = r;
    static_g = g;
    static_b = b;
}

void LedFX::stopStaticColor() {
    if (state == STATIC) {
        state = IDLE;
        strip.clear();
        strip.show();
    }
}

void LedFX::update() {
    if (!ledEnabled) {
        strip.clear();
        strip.show();
        return;
    }

    unsigned long now = millis();

    if (state == STATIC) {
        uint8_t r = (static_r * targetBrightness) >> 8;
        uint8_t g = (static_g * targetBrightness) >> 8;
        uint8_t b = (static_b * targetBrightness) >> 8;
        strip.setPixelColor(0, r, g, b);
        strip.show();
        return;
    }

    if (state == BOOT) {
        // Rainbow cycle over 1 second
        unsigned long progress = now - animStart;
        if (progress > 1000) {
            state = IDLE;
            strip.clear();
            strip.show();
        } else {
            // Hue cycle
            uint16_t hue = (progress * 65535) / 1000;
            uint32_t color = strip.ColorHSV(hue, 255, 255);
            // Manually scale brightness
            uint8_t r = ((uint8_t)(color >> 16) * targetBrightness) >> 8;
            uint8_t g = ((uint8_t)(color >> 8) * targetBrightness) >> 8;
            uint8_t b = ((uint8_t)color * targetBrightness) >> 8;
            strip.setPixelColor(0, r, g, b);
            strip.show();
        }
    }
    else if (state == PULSE_FADE) {
        // Fade out red pulse over 150ms
        unsigned long progress = now - animStart;
        if (progress > 150) {
            state = IDLE;
            strip.clear();
            strip.show();
        } else {
            // Manually scale brightness and apply fade
            float factor = 1.0f - (progress / 150.0f);
            uint8_t r = (255 * targetBrightness) >> 8; // Get base bright red
            r = r * factor; // Fade it out
            strip.setPixelColor(0, r, 0, 0);
            strip.show();
        }
    }
    else {
        // IDLE
    }
}