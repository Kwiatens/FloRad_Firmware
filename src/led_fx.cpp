#include "led_fx.h"
#include "pins.h"
#include "LightweightPixel.h"

// Constructor
LedFX::LedFX() : 
    ledEnabled(true),
    targetBrightness(50),
    state(IDLE),
    animStart(0),
    curr_r(0), curr_g(0), curr_b(0),
    static_r(0), static_g(0), static_b(0) {}

void LedFX::begin() {
    pinMode(PIN_NEOPIXEL, OUTPUT);
    digitalWrite(PIN_NEOPIXEL, LOW);
    show(); 
}

void LedFX::setEnabled(bool e) { 
    ledEnabled = e; 
    if(!e) { 
        curr_r = 0; curr_g = 0; curr_b = 0; 
        show(); 
    }
}

void LedFX::setBrightness(uint8_t b) { 
    targetBrightness = b; 
}

void LedFX::triggerBoot() {
    state = BOOT;
    animStart = millis();
}

void LedFX::triggerPulse() {
    if (!ledEnabled) return;
    state = PULSE_FADE;
    animStart = millis();
}

void LedFX::setStaticColor(uint8_t r, uint8_t g, uint8_t b) {
    state = STATIC;
    static_r = r; static_g = g; static_b = b;
}

void LedFX::stopStaticColor() {
    state = IDLE;
    curr_r = 0; curr_g = 0; curr_b = 0;
    show();
}

// Internal helper to send data to the LED
void LedFX::show() {
    if (!ledEnabled && state != BOOT) { // Allow boot animation even if "disabled" initially
        LightweightPixel::sendColor(0, 0, 0);
        return;
    }
    // Scale colors based on 0-255 brightness
    uint8_t r = (curr_r * targetBrightness) >> 8;
    uint8_t g = (curr_g * targetBrightness) >> 8;
    uint8_t b = (curr_b * targetBrightness) >> 8;
    LightweightPixel::sendColor(r, g, b);
}

void LedFX::update() {
    unsigned long now = millis();

    if (state == STATIC) {
        curr_r = static_r; curr_g = static_g; curr_b = static_b;
        show();
    } 
    else if (state == BOOT) {
        unsigned long progress = now - animStart;
        if (progress > 1000) {
            state = IDLE;
            curr_r = 0; curr_g = 0; curr_b = 0;
            show();
        } else {
            // Simple Blue-to-Green transition
            curr_r = 0;
            curr_g = (progress * 255) / 1000;
            curr_b = 255 - curr_g;
            show();
        }
    }
    else if (state == PULSE_FADE) {
        unsigned long progress = now - animStart;
        if (progress > 150) {
            state = IDLE;
            curr_r = 0; curr_g = 0; curr_b = 0;
            show();
        } else {
            // Fade out Red
            curr_r = 255 - ((progress * 255) / 150);
            curr_g = 0;
            curr_b = 0;
            show();
        }
    }
}