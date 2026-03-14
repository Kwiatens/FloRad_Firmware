#include "display.h"
#include "pins.h"
#include "state.h"

// Software SPI constructor (since we are defining specific pins)
// Using Hardware SPI is possible but requires passing the SPI object.
// Adafruit_PCD8544(int8_t sclk, int8_t din, int8_t dc, int8_t cs, int8_t rst);
DisplayManager::DisplayManager() 
    : lcd(PIN_LCD_SCLK, PIN_LCD_DIN, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_RST) {}

void DisplayManager::begin() {
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH); // Turn on backlight

    // Manual Reset to ensure display wakes up
    pinMode(PIN_LCD_RST, OUTPUT);
    digitalWrite(PIN_LCD_RST, LOW);
    delay(20);
    digitalWrite(PIN_LCD_RST, HIGH);

    lcd.begin();
    setContrast(55);
    lcd.clearDisplay();
    lcd.display();
}

void DisplayManager::setContrast(uint8_t val) {
    lcd.setContrast(val);
}

void DisplayManager::setBacklight(bool on) {
    digitalWrite(PIN_LCD_BL, on ? HIGH : LOW);
}

void DisplayManager::showBootScreen() {
    lcd.clearDisplay();
    lcd.setTextSize(1);
    lcd.setTextColor(BLACK);
    
    lcd.setTextSize(2);
    lcd.setCursor(12, 0);
    lcd.print("FloRad");
    
    lcd.setTextSize(1);
    lcd.setCursor(0, 18);
    lcd.print("by Kwiatens");
    
    lcd.setCursor(0, 28);
    lcd.print("HW: R1G");

    lcd.setCursor(0, 38);
    lcd.print("FW: v1.4");
    lcd.display();
}

void DisplayManager::updateMainScreen(int cpm, float usv, float voltage, bool charging, float remainingHours, const DeviceSettings& settings, bool tickerOn, const uint16_t* history, uint8_t historyIndex, bool alternateDisplay) {
    lcd.clearDisplay();
    lcd.setTextSize(1);
    lcd.setTextColor(BLACK);

    // --- Top Bar ---
    // Ticker Status (Left)
    // Bell icon
    lcd.drawLine(2, 2, 5, 2, BLACK); // Top of bell
    lcd.drawLine(1, 3, 6, 3, BLACK);
    lcd.drawLine(1, 4, 6, 4, BLACK);
    lcd.drawLine(0, 5, 7, 5, BLACK); // Flared bottom
    lcd.drawPixel(3, 6, BLACK); // Clapper
    lcd.drawPixel(4, 6, BLACK);
    if (tickerOn) {
        // Draw sound lines
        lcd.drawLine(9, 3, 11, 1, BLACK);
        lcd.drawLine(9, 4, 11, 4, BLACK);
        lcd.drawLine(9, 5, 11, 7, BLACK);
    }

    // Battery (Right)
    if (charging) {
        lcd.setCursor(48, 0);
        lcd.print("CHRG");
    } else {
        // Draw battery icon first, then text to its left
        // Icon outline
        lcd.drawRect(70, 0, 12, 7, BLACK);
        lcd.fillRect(82, 2, 2, 3, BLACK);

        // Determine number of bars from voltage
        int bars = 0;
        if (voltage > 4.0) bars = 4;
        else if (voltage > 3.8) bars = 3;
        else if (voltage > 3.6) bars = 2;
        else if (voltage > 3.4) bars = 1;

        // Draw bars inside
        if (bars > 0) lcd.fillRect(71, 2, 2, 3, BLACK);
        if (bars > 1) lcd.fillRect(74, 2, 2, 3, BLACK);
        if (bars > 2) lcd.fillRect(77, 2, 2, 3, BLACK);
        if (bars > 3) lcd.fillRect(80, 2, 2, 3, BLACK);

        bool shouldShowVoltage = settings.topBarShowVoltage;
        bool shouldShowRuntime = settings.topBarShowRuntime;

        if (shouldShowVoltage && shouldShowRuntime) { // Alternating
            if (alternateDisplay) shouldShowVoltage = false;
            else shouldShowRuntime = false;
        }

        if (shouldShowVoltage) {
            lcd.setCursor(38, 0); // Moved left to fit 0.00V
            lcd.print(voltage, 2);
            lcd.print("V");
        } else if (shouldShowRuntime) {
            lcd.setCursor(26, 0); // Moved left to fit label "T:"
            lcd.print("T:");
            int h = (int)remainingHours;
            int m = (int)((remainingHours - h) * 60);
            if(h > 99) h = 99;
            lcd.print(h);
            lcd.print(":");
            if(m < 10) lcd.print("0");
            lcd.print(m);
        }
    }
    lcd.drawLine(0, 8, 84, 8, BLACK);

    // --- Main Measurements ---
    lcd.setTextSize(2);
    lcd.setCursor(0, 10);
    if (settings.countUnit == UNIT_CPS) lcd.print(cpm / 60);
    else lcd.print(cpm);
    
    lcd.setTextSize(1);
    lcd.setCursor(54, 10); // Moved up to make room for uncertainty
    if (settings.countUnit == UNIT_CPS) lcd.print("CPS");
    else lcd.print("CPM");

    // Uncertainty (1/sqrt(N))
    lcd.setCursor(54, 18);
    if (cpm > 0) {
        int pct = (int)(100.0f / sqrt((float)cpm));
        if (pct > 99) pct = 99; // Cap at 99%
        lcd.write(0xF1); // Compact +/- symbol (ASCII 241 '±' in CP437)
        lcd.print(pct);
        lcd.print("%");
    } else {
        lcd.print("---");
    }

    lcd.setCursor(0, 26);
    // Conversion if needed
    float doseVal = usv;
    const char* doseLabel = " uSv/h";
    
    if (settings.doseUnit == UNIT_URH) {
        doseVal = usv * 100.0f; // Approx conversion
        doseLabel = " uR/h";
    }
    
    lcd.print(doseVal, 2);
    lcd.print(doseLabel);

    // --- Bar Graph ---
    int graphY = 36;
    int graphHeight = 11;
    int maxCount = 0;
    // Find max value in history for scaling
    for(int i=0; i<60; i++) {
        if(history[i] > maxCount) maxCount = history[i];
    }
    if (maxCount < 5) maxCount = 5; // Minimum scale for visibility
    if (maxCount > 100) maxCount = 100; // Cap scaling to avoid making background counts invisible

    for(int i=0; i<60; i++) {
        // Draw from oldest to newest. `historyIndex` is now the oldest data point.
        int readIdx = (historyIndex + i) % 60;
        int barHeight = map(history[readIdx], 0, maxCount, 0, graphHeight);
        if (barHeight > 0) {
            lcd.drawLine(12 + i, (graphY + graphHeight) - barHeight, 12 + i, graphY + graphHeight - 1, BLACK);
        }
    }

    lcd.display();
}

void DisplayManager::drawMenu(const char* title, const char* const* items, int itemCount, int selectedIndex, const DeviceSettings& settings, SystemState state) {
    lcd.clearDisplay();
    
    // Title Bar
    lcd.fillRect(0, 0, 84, 9, BLACK);
    lcd.setTextColor(WHITE);
    lcd.setCursor(2, 1);
    lcd.print(title);
    
    // Items
    int startY = 10;
    int lineHeight = 9;

    // Simple scrolling: keep selected item in view
    int viewportStart = 0;
    if (selectedIndex >= 4) viewportStart = selectedIndex - 3;

    for (int i = 0; i < 4 && (i + viewportStart) < itemCount; i++) {
        int idx = i + viewportStart;
        int currentY = startY + (i * lineHeight);
        
        bool isToggledOn = false;
        if (state == STATE_MENU_DEVICE) {
            if (idx == 0 && settings.backlightOn) isToggledOn = true;
            if (idx == 1 && settings.ledEnabled) isToggledOn = true;
            // Note: Auto-off now shows value instead of a toggle
            if (idx == 6 && settings.topBarShowVoltage) isToggledOn = true;
            if (idx == 7 && settings.topBarShowRuntime) isToggledOn = true;
        }
        
        if (isToggledOn) {
            // A toggled-on item gets an inverted background
            lcd.fillRect(0, currentY - 1, 84, lineHeight, BLACK);
            lcd.setTextColor(WHITE);
        } else {
            // Normal item
            lcd.setTextColor(BLACK);
        }
        
        lcd.setCursor(2, currentY);
        if (idx == selectedIndex) {
            // For selected item, print cursor
            lcd.print(">");
        } else {
            // Otherwise, print a space for alignment
            lcd.print(" ");
        }
        
        // Print the menu item text
        lcd.setCursor(8, currentY);
        lcd.print(items[idx]);

        // Special handling for Auto Off to show current value
        if (state == STATE_MENU_DEVICE && idx == 4) {
            lcd.print(": ");
            if (settings.autoBacklightTimeout == 0) {
                lcd.print("Off");
            } else if (settings.autoBacklightTimeout < 60000) {
                lcd.print(settings.autoBacklightTimeout / 1000);
                lcd.print("s");
            } else {
                lcd.print(settings.autoBacklightTimeout / 60000);
                lcd.print("m");
            }
        }
    }
    
    lcd.display();
}

void DisplayManager::drawOptionSelector(const char* title, const char* const* options, int optionCount, int selectedIndex) {
    lcd.clearDisplay();
    
    // Title Bar
    lcd.fillRect(0, 0, 84, 9, BLACK);
    lcd.setTextColor(WHITE);
    lcd.setCursor(2, 1);
    lcd.print(title);

    // Options
    int startY = 10;
    int lineHeight = 9;

    for (int i = 0; i < optionCount; i++) {
        int currentY = startY + (i * lineHeight);
        
        if (i == selectedIndex) {
            lcd.fillRect(0, currentY - 1, 84, lineHeight, BLACK);
            lcd.setTextColor(WHITE);
        } else {
            lcd.setTextColor(BLACK);
        }
        
        lcd.setCursor(8, currentY);
        lcd.print(options[i]);
    }

    lcd.display();
}

void DisplayManager::drawSlider(const char* title, int value, int min, int max) {
    lcd.clearDisplay();
    lcd.setTextColor(BLACK, WHITE);

    // Title
    lcd.setCursor(0, 0);
    lcd.print(title);

    // Value
    lcd.setTextSize(2);
    lcd.setCursor(28, 12); // Centered-ish
    lcd.print(value);
    lcd.setTextSize(1);

    // Slider bar
    int barWidth = map(value, min, max, 0, 78);
    lcd.drawRect(2, 30, 80, 8, BLACK);
    lcd.fillRect(3, 31, barWidth, 6, BLACK);

    // Instructions
    lcd.setCursor(0, 40);
    lcd.print("-");
    lcd.setCursor(78, 40);
    lcd.print("+");
    lcd.setCursor(5, 40);
    lcd.print("Hold to save");

    lcd.display();
}

void DisplayManager::drawSystemInfo(const char* fw_ver, uint32_t uptime_s, uint32_t total_counts, float voltage, float remainingHours, int scrollOffset) {
    lcd.clearDisplay();

    // Title Bar
    lcd.fillRect(0, 0, 84, 9, BLACK);
    lcd.setTextColor(WHITE);
    lcd.setCursor(2, 1);
    lcd.print("System Info");
    lcd.setTextWrap(false); // Disable wrap to prevent layout overlaps

    char lineBuffer[7][22];
    snprintf(lineBuffer[0], 22, "FW ver: %s", fw_ver);
    snprintf(lineBuffer[1], 22, "Build: %s", __DATE__);
    snprintf(lineBuffer[2], 22, "MCU: STM32F103");
    snprintf(lineBuffer[3], 22, "Uptime: %lu s", uptime_s);
    snprintf(lineBuffer[4], 22, "Counts: %lu", total_counts);
    snprintf(lineBuffer[5], 22, "Batt V: %.2fV", voltage);

    int hours = (int)remainingHours;
    int minutes = (int)((remainingHours - hours) * 60);
    snprintf(lineBuffer[6], 22, "Est Time: %dh%02dm", hours, minutes);

    const int totalLines = 7;
    const int visibleLines = 3; // Reduced to 3 to prevent overlap with footer

    lcd.setTextColor(BLACK);
    for (int i = 0; i < visibleLines; i++) {
        int lineIdx = i + scrollOffset;
        if (lineIdx >= totalLines) break;
        lcd.setCursor(2, 10 + i * 8);
        lcd.print(lineBuffer[lineIdx]);
    }

    // Scroll indicators
    if (scrollOffset > 0) {
        lcd.fillTriangle(80, 12, 78, 16, 82, 16, BLACK); // Up arrow
    }
    if (scrollOffset + visibleLines < totalLines) {
        lcd.fillTriangle(80, 42, 78, 38, 82, 38, BLACK); // Down arrow
    }

    // Footer
    lcd.setCursor(2, 41);
    lcd.print("< Back");
    lcd.setCursor(34, 41);
    lcd.print("Scroll >");

    lcd.display();
}