#include "geiger.h"
#include "pins.h"

volatile uint32_t totalPulses = 0;
volatile bool pulseDetected = false;
bool tickerActive = true; // Default ON

// Circular buffer for the last 60 seconds
uint16_t history[60] = {0};
uint8_t historyIndex = 0;
uint32_t lastHistoryUpdate = 0;
uint32_t lastPulseCountForCalc = 0;

// Forward declaration for the Interrupt Service Routine
void onGeigerPulse();

// Interrupt Service Routine
void onGeigerPulse() {
    totalPulses++;
    pulseDetected = true; // Set flag for main loop to process
}

void Geiger::begin() {
    pinMode(PIN_GEIGER_IN, INPUT);
    pinMode(PIN_TICKER_OUT, OUTPUT);
    digitalWrite(PIN_TICKER_OUT, LOW);

    attachInterrupt(digitalPinToInterrupt(PIN_GEIGER_IN), onGeigerPulse, RISING);
    
    lastHistoryUpdate = millis();
}

void Geiger::update() {
    uint32_t now = millis();

    // Every 1 second, slide the window
    if (now - lastHistoryUpdate >= 1000) {
        lastHistoryUpdate = now;

        // Calculate pulses occurring in the last second
        // We need to disable interrupts briefly to read volatile securely if it changes fast,
        // but for <10000 CPM atomic read of 32bit on 32bit MCU is usually fine.
        // Safest approach:
        noInterrupts();
        uint32_t currentTotal = totalPulses;
        interrupts();

        uint16_t pulsesThisSecond = currentTotal - lastPulseCountForCalc;
        lastPulseCountForCalc = currentTotal;

        // Add to history
        history[historyIndex] = pulsesThisSecond;
        historyIndex = (historyIndex + 1) % 60;
    }
}

int Geiger::getCPM() {
    int cpm = 0;
    for (int i = 0; i < 60; i++) {
        cpm += history[i];
    }
    return cpm;
}

float Geiger::getuSv() {
    return getCPM() * CPM_TO_USV_FACTOR;
}

void Geiger::setTicker(bool enabled) { tickerActive = enabled; }
bool Geiger::isTickerEnabled() { return tickerActive; }

bool Geiger::getAndClearPulseFlag() {
    if (pulseDetected) {
        noInterrupts();
        pulseDetected = false;
        interrupts();
        return true;
    }
    return false;
}

uint32_t Geiger::getTotalPulses() {
    return totalPulses;
}

const uint16_t* Geiger::getHistory() const {
    return history;
}

uint8_t Geiger::getHistoryIndex() const {
    return historyIndex;
}