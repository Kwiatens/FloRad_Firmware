#ifndef INPUTS_H
#define INPUTS_H

#include <Arduino.h>

enum InputEvent {
    EVT_NONE,
    EVT_BTN_A_SHORT, // Right
    EVT_BTN_B_SHORT, // Left
    EVT_BTN_BOTH_LONG
};

class InputManager {
public:
    void begin();
    // Returns the latest event
    InputEvent update();
};

#endif