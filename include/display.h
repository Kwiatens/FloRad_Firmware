#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_PCD8544.h>
#include "settings.h"
#include "state.h"

class DisplayManager {
public:
    DisplayManager();
    void begin();
    void setContrast(uint8_t val);
    void setBacklight(bool on);
    
    void showBootScreen();
    void updateMainScreen(int cpm, float usv, float voltage, bool charging, float remainingHours, const DeviceSettings& settings, bool tickerOn, const uint16_t* history, uint8_t historyIndex, bool alternateDisplay);
    
    void drawMenu(const char* title, const char* const* items, int itemCount, int selectedIndex, const DeviceSettings& settings, SystemState state);
    void drawSlider(const char* title, int value, int min, int max);
    void drawOptionSelector(const char* title, const char* const* options, int optionCount, int selectedIndex);
    void drawSystemInfo(const char* fw_ver, uint32_t uptime_s, uint32_t total_counts, float voltage, float remainingHours, int scrollOffset);
private:
    Adafruit_PCD8544 lcd;
};

#endif