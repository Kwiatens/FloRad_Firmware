#include "inputs.h"
#include "pins.h"

void InputManager::begin() {
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
}

unsigned long pressTimeA = 0;
unsigned long pressTimeB = 0;
bool heldA = false;
bool heldB = false;
bool ignoreNextRelease = false; // If long press triggered, don't trigger short press on release

InputEvent InputManager::update() {
    bool readA = digitalRead(PIN_BTN_A);
    bool readB = digitalRead(PIN_BTN_B);
    unsigned long now = millis();
    InputEvent event = EVT_NONE;

    // Button A Logic (Active LOW)
    if (readA == LOW && !heldA) {
        heldA = true;
        pressTimeA = now;
    } else if (readA == HIGH && heldA) {
        heldA = false;
        if (!ignoreNextRelease && (now - pressTimeA > 50)) {
            event = EVT_BTN_A_SHORT;
        }
    }

    // Button B Logic (Active LOW)
    if (readB == LOW && !heldB) {
        heldB = true;
        pressTimeB = now;
    } else if (readB == HIGH && heldB) {
        heldB = false;
        if (!ignoreNextRelease && (now - pressTimeB > 50)) {
            event = EVT_BTN_B_SHORT;
        }
        // Reset flag when both are released
        if (!heldA && !heldB) ignoreNextRelease = false;
    }

    // Check for Dual Long Press
    if (heldA && heldB && !ignoreNextRelease) {
        // If both held for > 1000ms
        if ((now - pressTimeA > 1000) && (now - pressTimeB > 1000)) {
            event = EVT_BTN_BOTH_LONG;
            ignoreNextRelease = true; // Consumed
        }
    }

    // Safety cleanup if only one remains held after ignore
    if (!heldA && !heldB) ignoreNextRelease = false;
    
    return event;
}