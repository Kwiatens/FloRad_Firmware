#ifndef LIGHTWEIGHT_PIXEL_H
#define LIGHTWEIGHT_PIXEL_H

#include <Arduino.h>

class LightweightPixel {
public:
    static void sendColor(uint8_t r, uint8_t g, uint8_t b) {
        // WS2812B expects data in G-R-B order
        uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
        
        // Get port and bit mask for direct register access
        GPIO_TypeDef *port = digitalPinToPort(PIN_NEOPIXEL);
        uint32_t pinMask = digitalPinToBitMask(PIN_NEOPIXEL);
        
        noInterrupts(); // Timing is critical
        
        for (int8_t i = 23; i >= 0; i--) {
            if (color & (1UL << i)) {
                // Logic '1': High for ~700ns, Low for ~600ns
                port->BSRR = pinMask; // HIGH
                asm volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
                port->BRR = pinMask;  // LOW
                asm volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
            } else {
                // Logic '0': High for ~350ns, Low for ~800ns
                port->BSRR = pinMask; // HIGH
                asm volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
                port->BRR = pinMask;  // LOW
                asm volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
            }
        }
        
        interrupts();
        delayMicroseconds(50); // Latch pulse (>50us)
    }
};

#endif