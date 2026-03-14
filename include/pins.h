#ifndef PINS_H
#define PINS_H

#include <Arduino.h>

// LCD Nokia 5110
#define PIN_LCD_SCLK    PB13
#define PIN_LCD_DIN     PB15 // MOSI
#define PIN_LCD_DC      PB1
#define PIN_LCD_CS      PB9
#define PIN_LCD_RST     PA2
#define PIN_LCD_BL      PA3  // Backlight

// Geiger
#define PIN_GEIGER_IN   PA7  // Input pulse
#define PIN_TICKER_OUT  PA8  // Speaker/Buzzer output

// User Input
#define PIN_BTN_A       PB7  // Right button
#define PIN_BTN_B       PB5  // Left button

// Power
#define PIN_BATT_SENSE  PB0
#define PIN_CHARGING    PB8

// Accessories
#define PIN_NEOPIXEL    PC15

#endif