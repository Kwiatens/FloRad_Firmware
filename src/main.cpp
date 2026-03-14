#include <Arduino.h>
#include "pins.h"
#include "display.h"
#include "geiger.h"
#include "power.h"
#include "inputs.h"
#include "led_fx.h"
#include "settings.h"
#include "state.h"
#include <EEPROM.h>

DisplayManager displayMgr;
Geiger geiger;
Power power;
InputManager inputs;
LedFX led;
DeviceSettings settings;

SystemState currentState = STATE_MAIN;
int menuIndex = 0;
unsigned long lastInputTime = 0;

// Menu Items
const char* rootMenu[] = { "Device", "Measurement", "System Info", "Exit" };
const char* deviceMenu[] = { "Backlight", "LED", "Contrast", "LED Level", "Auto Off", "Batt Cap", "Top Bar V", "Top Bar T", "Back" };
const char* msrmtMenu[] = { "Count Unit", "Dose Unit", "Back" };
const char* countUnitOptions[] = { "CPM", "CPS" };
const char* doseUnitOptions[] = { "uSv/h", "uR/h" };
const char* autoOffOptionsLabels[] = { "Off", "10s", "30s", "1m", "5m", "10m" };
const uint32_t autoOffOptionsValues[] = { 0, 10000, 30000, 60000, 300000, 600000 };
const int autoOffOptionsCount = 6;

// --- Helper Functions for Optimization ---

/**
 * Calculates battery percentage using a Cubic Hermite S-Curve (3x^2 - 2x^3).
 * This approximates the Li-Ion discharge curve without the memory overhead of <math.h> (cos/sin).
 */
float calculateBatteryPercentage(float voltage) {
    // 3.3V is a safe '0%' for most STM32 LDOs to maintain regulation
    if (voltage >= 4.2f) return 1.0f;
    if (voltage <= 3.3f) return 0.0f;
    
    // Normalize voltage between 0.0 (3.3V) and 1.0 (4.2V)
    float x = (voltage - 3.3f) / 0.9f;
    
    // S-curve polynomial: 3x^2 - 2x^3
    // This mimics the flat plateau of Li-ion followed by the rapid drop
    return x * x * (3.0f - 2.0f * x);
}

/**
 * Disables clocks to unused peripherals to save power.
 */
void disableUnusedPeripherals() {
    // Disable clocks to unused timers and I2C peripherals
    RCC->APB1ENR &= ~(RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN | RCC_APB1ENR_I2C1EN | RCC_APB1ENR_I2C2EN);
    // ADC2 is usually not needed if we only use ADC1 for battery
    RCC->APB2ENR &= ~(RCC_APB2ENR_ADC2EN);
}

void saveSettings() {
    EEPROM.put(0, settings);
}

void loadSettings() {
    EEPROM.get(0, settings);
    if (settings.lcdContrast < 30 || settings.lcdContrast > 80 || settings.countUnit > 1) {
        settings = DeviceSettings();
    }
    if (settings.batteryCapacity < 100 || settings.batteryCapacity > 10000) {
        settings.batteryCapacity = 1000;
    }

    displayMgr.setContrast(settings.lcdContrast);
    displayMgr.setBacklight(settings.backlightOn);
    led.setBrightness(settings.ledBrightness);
    led.setEnabled(settings.ledEnabled);
}

void setup() {
  // Optimization: Shut down unused internal hardware clocks
  disableUnusedPeripherals();

  // Initialize Subsystems
  inputs.begin();
  power.begin();
  geiger.begin();
  displayMgr.begin();
  led.begin();

  loadSettings();
  
  led.triggerBoot();
  tone(PIN_TICKER_OUT, 1200, 100);
  delay(120);
  tone(PIN_TICKER_OUT, 1000, 100);
  delay(120);
  tone(PIN_TICKER_OUT, 1200, 150);

  displayMgr.showBootScreen();

  unsigned long bootStartTime = millis();
  while(millis() - bootStartTime < 2000) {
    led.update();
    delay(10); 
  }
  lastInputTime = bootStartTime + 2000;
}

void loop() {
  geiger.update();
  led.update();
  power.update();

  if (geiger.getAndClearPulseFlag()) {
      if (settings.ledEnabled) led.triggerPulse();
      if (geiger.isTickerEnabled()) {
          digitalWrite(PIN_TICKER_OUT, HIGH);
          delayMicroseconds(150);
          digitalWrite(PIN_TICKER_OUT, LOW);
      }
  }

  bool isBacklightActive = settings.backlightOn;
  if (settings.autoBacklightTimeout > 0 && (millis() - lastInputTime > settings.autoBacklightTimeout)) {
      isBacklightActive = false;
  }
  displayMgr.setBacklight(isBacklightActive);

  InputEvent evt = inputs.update();
  if (evt != EVT_NONE) {
      lastInputTime = millis();
      if (settings.backlightOn) displayMgr.setBacklight(true);
  }

  if (currentState == STATE_MAIN) {
      if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_ROOT; menuIndex = 0; }
      else if (evt == EVT_BTN_B_SHORT) geiger.setTicker(!geiger.isTickerEnabled());
      
      static unsigned long lastDraw = 0;
      static unsigned long lastAlternate = 0;
      static bool alternateDisplay = false;
      static float filteredHours = -1.0f; 
      
      if (millis() - lastDraw > 200) {
        lastDraw = millis();
        if (millis() - lastAlternate > 2000) {
            lastAlternate = millis();
            alternateDisplay = !alternateDisplay;
        }
        
        float v = power.getBatteryVoltage();
        float remainingHours = -1.0f; 

        // --- NEW PLACEMENT START ---
        if (millis() > 60000) { 
            float pct = calculateBatteryPercentage(v);
            
            float currentMa;
            // Use 35mA if in any menu, otherwise 20mA/23mA based on backlight
            if (currentState != STATE_MAIN) {
                currentMa = 35.0f; 
            } else {
                currentMa = isBacklightActive ? 23.0f : 20.0f; 
            }

            float remainingCapacity = settings.batteryCapacity * pct;
            remainingHours = remainingCapacity / currentMa;

            if (filteredHours < 0) filteredHours = remainingHours;
            else filteredHours = (filteredHours * 0.95f) + (remainingHours * 0.05f);
        } else {
            filteredHours = -1.0f; 
        }
        // --- NEW PLACEMENT END ---

        displayMgr.updateMainScreen(geiger.getCPM(), geiger.getuSv(), v, power.isCharging(), filteredHours, settings, geiger.isTickerEnabled(), geiger.getHistory(), geiger.getHistoryIndex(), alternateDisplay);
      }
  }
  else if (currentState == STATE_MENU_ROOT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 4;
      else if (evt == EVT_BTN_B_SHORT) {
          if (menuIndex == 0) { currentState = STATE_MENU_DEVICE; menuIndex = 0; }
          else if (menuIndex == 1) { currentState = STATE_MENU_MSRMT; menuIndex = 0; }
          else if (menuIndex == 2) { currentState = STATE_MENU_SYSTEM; menuIndex = 0; }
          else if (menuIndex == 3) currentState = STATE_MAIN;
      }
      else if (evt == EVT_BTN_BOTH_LONG) currentState = STATE_MAIN;
      displayMgr.drawMenu("Settings", rootMenu, 4, menuIndex, settings, currentState);
  }
  else if (currentState == STATE_MENU_DEVICE) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 9;
      else if (evt == EVT_BTN_B_SHORT) {
          switch(menuIndex) {
              case 0: settings.backlightOn = !settings.backlightOn; displayMgr.setBacklight(settings.backlightOn); saveSettings(); break;
              case 1: settings.ledEnabled = !settings.ledEnabled; led.setEnabled(settings.ledEnabled); saveSettings(); break;
              case 2: currentState = STATE_EDIT_CONTRAST; break;
              case 3: currentState = STATE_EDIT_BRIGHTNESS; led.setEnabled(true); led.setStaticColor(0, 0, 255); break;
              case 4: currentState = STATE_EDIT_AUTO_OFF_TIMEOUT; menuIndex = 0; break;
              case 5: currentState = STATE_EDIT_BATTERY_CAPACITY; break;
              case 6: settings.topBarShowVoltage = !settings.topBarShowVoltage; saveSettings(); break;
              case 7: settings.topBarShowRuntime = !settings.topBarShowRuntime; saveSettings(); break;
              case 8: currentState = STATE_MENU_ROOT; menuIndex = 0; break;
          }
      }
      else if (evt == EVT_BTN_BOTH_LONG) currentState = STATE_MAIN;
      displayMgr.drawMenu("Device", deviceMenu, 9, menuIndex, settings, currentState);
  }
  else if (currentState == STATE_MENU_MSRMT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 3;
      else if (evt == EVT_BTN_B_SHORT) {
          if (menuIndex == 0) { currentState = STATE_EDIT_COUNT_UNIT; menuIndex = settings.countUnit; } 
          else if (menuIndex == 1) { currentState = STATE_EDIT_DOSE_UNIT; menuIndex = settings.doseUnit; }
          else if (menuIndex == 2) { currentState = STATE_MENU_ROOT; menuIndex = 1; }
      }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MAIN; menuIndex = 0; }
      displayMgr.drawMenu("Measurement", msrmtMenu, 3, menuIndex, settings, currentState);
  }
  else if (currentState == STATE_EDIT_CONTRAST) {
      if (evt == EVT_BTN_A_SHORT) { if(settings.lcdContrast < 80) settings.lcdContrast += 5; }
      else if (evt == EVT_BTN_B_SHORT) { if(settings.lcdContrast > 30) settings.lcdContrast -= 5; }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_DEVICE; saveSettings(); }
      displayMgr.setContrast(settings.lcdContrast);
      displayMgr.drawSlider("Contrast", settings.lcdContrast, 30, 80);
  }
  else if (currentState == STATE_EDIT_BRIGHTNESS) {
      if (evt == EVT_BTN_A_SHORT) { if(settings.ledBrightness <= 245) settings.ledBrightness += 10; else settings.ledBrightness = 255; }
      else if (evt == EVT_BTN_B_SHORT) { if(settings.ledBrightness >= 10) settings.ledBrightness -= 10; else settings.ledBrightness = 0; }
      else if (evt == EVT_BTN_BOTH_LONG) { 
          currentState = STATE_MENU_DEVICE; 
          saveSettings();
          led.stopStaticColor();
          led.setEnabled(settings.ledEnabled);
      }
      led.setBrightness(settings.ledBrightness);
      displayMgr.drawSlider("LED Level", settings.ledBrightness, 0, 255);
  }
  else if (currentState == STATE_EDIT_BATTERY_CAPACITY) {
      if (evt == EVT_BTN_A_SHORT) { if(settings.batteryCapacity < 5000) settings.batteryCapacity += 50; }
      else if (evt == EVT_BTN_B_SHORT) { if(settings.batteryCapacity > 100) settings.batteryCapacity -= 50; }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_DEVICE; saveSettings(); }
      displayMgr.drawSlider("Batt Cap", settings.batteryCapacity, 100, 3000);
  }
  else if (currentState == STATE_EDIT_AUTO_OFF_TIMEOUT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % autoOffOptionsCount;
      else if (evt == EVT_BTN_B_SHORT) {
          settings.autoBacklightTimeout = autoOffOptionsValues[menuIndex];
          currentState = STATE_MENU_DEVICE;
          saveSettings();
          menuIndex = 4;
      }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_DEVICE; menuIndex = 4; }
      displayMgr.drawOptionSelector("Auto Off Time", autoOffOptionsLabels, autoOffOptionsCount, menuIndex);
  }
  else if (currentState == STATE_MENU_SYSTEM) {
      static int sysInfoScroll = 0;
      const int totalSysInfoLines = 7;
      const int visibleSysInfoLines = 4;

      if (evt == EVT_BTN_A_SHORT) { if(sysInfoScroll < (totalSysInfoLines - visibleSysInfoLines)) sysInfoScroll++; else sysInfoScroll = 0; }
      else if (evt == EVT_BTN_B_SHORT || evt == EVT_BTN_BOTH_LONG) { 
          currentState = STATE_MENU_ROOT;
          menuIndex = 2;
          sysInfoScroll = 0; 
      }
      
      float v = power.getBatteryVoltage();
      float remainingHours = -1.0f;
      // Consistent math and settling delay for System Info screen
      if (millis() > 60000) {
          float pct = calculateBatteryPercentage(v);
          float currentMa = isBacklightActive ? 38.7f : 36.0f;
          remainingHours = (settings.batteryCapacity * pct) / currentMa;
      }
      displayMgr.drawSystemInfo("v1.4", millis()/1000, geiger.getTotalPulses(), v, remainingHours, sysInfoScroll);
  }
  else if (currentState == STATE_EDIT_COUNT_UNIT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 2;
      else if (evt == EVT_BTN_B_SHORT) {
          settings.countUnit = (CountUnit)menuIndex;
          currentState = STATE_MENU_MSRMT;
          saveSettings();
          menuIndex = 0;
      }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_MSRMT; menuIndex = 0; }
      displayMgr.drawOptionSelector("Count Unit", countUnitOptions, 2, menuIndex);
  }
  else if (currentState == STATE_EDIT_DOSE_UNIT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 2;
      else if (evt == EVT_BTN_B_SHORT) {
          settings.doseUnit = (DoseUnit)menuIndex;
          currentState = STATE_MENU_MSRMT;
          saveSettings();
          menuIndex = 1;
      }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_MSRMT; menuIndex = 1; }
      displayMgr.drawOptionSelector("Dose Unit", doseUnitOptions, 2, menuIndex);
  }

  // Put the CPU to sleep until the next interrupt (Ticker or Timer)
  __WFI(); 
}