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

void saveSettings() {
    EEPROM.put(0, settings);
}

void loadSettings() {
    EEPROM.get(0, settings);
    // Sanity check for uninitialized EEPROM or invalid values
    if (settings.lcdContrast < 30 || settings.lcdContrast > 80 || settings.countUnit > 1) {
        settings = DeviceSettings(); // Reset to defaults
    }
    if (settings.batteryCapacity < 100 || settings.batteryCapacity > 10000) {
        settings.batteryCapacity = 1000;
    }

    displayMgr.setContrast(settings.lcdContrast);
    displayMgr.setBacklight(settings.backlightOn);
    led.setBrightness(settings.ledBrightness);
    led.setEnabled(settings.ledEnabled);
}

// Helper to calculate Li-Ion SoC using an S-curve approximation
float calculateBatteryPercentage(float voltage) {
    // Constrain to standard 14500/AA Li-Ion limits
    float v_clamped = constrain(voltage, 3.4f, 4.2f);
    // Normalize voltage between 0.0 and 1.0
    float x = (v_clamped - 3.4f) / (4.2f - 3.4f);
    // Map to an S-Curve using cosine to match the typical Li-Ion discharge plateau
    return 0.5f - 0.5f * cos(x * PI);
}

void setup() {
  // USB Re-enum (Keep this as it helps with flashing)
  pinMode(PA12, OUTPUT);
  digitalWrite(PA12, LOW);
  delay(250);
  pinMode(PA12, INPUT);

  // Initialize Subsystems
  inputs.begin();
  power.begin();
  geiger.begin();
  displayMgr.begin();
  led.begin();

  loadSettings();
  
  // Startup sound and animation, always runs regardless of settings
  led.triggerBoot();
  // "Military/Lab" boot sound
  tone(PIN_TICKER_OUT, 1200, 100);
  delay(120);
  tone(PIN_TICKER_OUT, 1000, 100);
  delay(120);
  tone(PIN_TICKER_OUT, 1200, 150);

  displayMgr.showBootScreen(); // Show boot screen text

  // Run boot animation for 2 seconds while showing the boot screen
  unsigned long bootStartTime = millis();
  while(millis() - bootStartTime < 2000) {
    led.update();
    delay(10); // Small delay to prevent busy-waiting
  }
  lastInputTime = bootStartTime + 2000;
}

void loop() {
  geiger.update();
  led.update();
  power.update();

  // Handle Geiger Pulse event for Ticker and LED
  if (geiger.getAndClearPulseFlag()) {
      if (settings.ledEnabled) {
          led.triggerPulse();
      }
      if (geiger.isTickerEnabled()) {
          // A short, sharp pulse creates a more authentic analog "tick" sound
          digitalWrite(PIN_TICKER_OUT, HIGH);
          delayMicroseconds(150); // 150uS pulse for a sharp "tick"
          digitalWrite(PIN_TICKER_OUT, LOW);
      }
  }

  // Auto Backlight Logic
  bool isBacklightActive = settings.backlightOn;
  if (settings.autoBacklightTimeout > 0 && (millis() - lastInputTime > settings.autoBacklightTimeout)) {
      isBacklightActive = false;
  }
  displayMgr.setBacklight(isBacklightActive);

  InputEvent evt = inputs.update();
  if (evt != EVT_NONE) {
      lastInputTime = millis();
      // Wake up backlight if it was off
      if (settings.backlightOn) displayMgr.setBacklight(true);
  }

  if (currentState == STATE_MAIN) {
      if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MENU_ROOT; menuIndex = 0; }
      else if (evt == EVT_BTN_B_SHORT) geiger.setTicker(!geiger.isTickerEnabled());
      
      static unsigned long lastDraw = 0;
      static unsigned long lastAlternate = 0;
      static bool alternateDisplay = false;
      static float filteredHours = -1.0f; // Initialize as negative to seed first value
      
      if (millis() - lastDraw > 200) {
        lastDraw = millis();
        if (millis() - lastAlternate > 2000) { // Alternate every 2 seconds
            lastAlternate = millis();
            alternateDisplay = !alternateDisplay;
        }
        
        float v = power.getBatteryVoltage();
        float remainingHours = -1.0f; // Use -1.0 to flag "Wait/Calc" status
        
        // Let voltage settle for 1 minute (60,000 ms)
        if (millis() > 60000) {
            float pct = calculateBatteryPercentage(v);
            float currentMa = isBacklightActive ? 38.7f : 36.0f;
            remainingHours = (settings.batteryCapacity * pct) / currentMa;

            // Filter the remaining time to prevent "jumping minutes"
            if (filteredHours < 0) filteredHours = remainingHours; // Seed filter
            else filteredHours = (filteredHours * 0.98f) + (remainingHours * 0.02f); // Strong filter
        } else {
            filteredHours = -1.0f; // Reset filter if we're in the settling period
        }

        displayMgr.updateMainScreen(geiger.getCPM(), geiger.getuSv(), v, power.isCharging(), filteredHours, settings, geiger.isTickerEnabled(), geiger.getHistory(), geiger.getHistoryIndex(), alternateDisplay);
      }
  }

  else if (currentState == STATE_MENU_ROOT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 4; // 4 items now
      else if (evt == EVT_BTN_B_SHORT) {
          if (menuIndex == 0) { currentState = STATE_MENU_DEVICE; menuIndex = 0; }      // Device
          else if (menuIndex == 1) { currentState = STATE_MENU_MSRMT; menuIndex = 0; } // Measurement
          else if (menuIndex == 2) { currentState = STATE_MENU_SYSTEM; menuIndex = 0; } // System Info
          else if (menuIndex == 3) currentState = STATE_MAIN;                           // Exit
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
              case 8: currentState = STATE_MENU_ROOT; menuIndex = 0; break; // Back
          }
      }
      else if (evt == EVT_BTN_BOTH_LONG) currentState = STATE_MAIN;
      
      displayMgr.drawMenu("Device", deviceMenu, 9, menuIndex, settings, currentState);
  }
  else if (currentState == STATE_MENU_MSRMT) {
      if (evt == EVT_BTN_A_SHORT) menuIndex = (menuIndex + 1) % 3;
      else if (evt == EVT_BTN_B_SHORT) {
          if (menuIndex == 0) { // Count Unit
              currentState = STATE_EDIT_COUNT_UNIT;
              menuIndex = settings.countUnit; // Pre-select current setting
          } else if (menuIndex == 1) { // Dose Unit
              currentState = STATE_EDIT_DOSE_UNIT;
              menuIndex = settings.doseUnit; // Pre-select current setting
          }
          else if (menuIndex == 2) { currentState = STATE_MENU_ROOT; menuIndex = 1; } // Back
      }
      else if (evt == EVT_BTN_BOTH_LONG) { currentState = STATE_MAIN; menuIndex = 0; }
      displayMgr.drawMenu("Measurement", msrmtMenu, 3, menuIndex, settings, currentState);
  }
  else if (currentState == STATE_EDIT_CONTRAST) {
      if (evt == EVT_BTN_A_SHORT) { if(settings.lcdContrast < 80) settings.lcdContrast += 5; }      // +
      else if (evt == EVT_BTN_B_SHORT) { if(settings.lcdContrast > 30) settings.lcdContrast -= 5; } // -
      else if (evt == EVT_BTN_BOTH_LONG) { 
          currentState = STATE_MENU_DEVICE; 
          saveSettings();
      } // Save and exit
      
      displayMgr.setContrast(settings.lcdContrast); // Apply immediately for feedback
      displayMgr.drawSlider("Contrast", settings.lcdContrast, 30, 80);
  }
  else if (currentState == STATE_EDIT_BRIGHTNESS) {
      if (evt == EVT_BTN_A_SHORT) { if(settings.ledBrightness <= 245) settings.ledBrightness += 10; else settings.ledBrightness = 255; } // + (step 10)
      else if (evt == EVT_BTN_B_SHORT) { if(settings.ledBrightness >= 10) settings.ledBrightness -= 10; else settings.ledBrightness = 0; }  // -
      else if (evt == EVT_BTN_BOTH_LONG) { 
          currentState = STATE_MENU_DEVICE; 
          saveSettings();
          led.stopStaticColor();
          led.setEnabled(settings.ledEnabled);
      } // Save and exit

      led.setBrightness(settings.ledBrightness); // Apply immediately for feedback
      displayMgr.drawSlider("LED Level", settings.ledBrightness, 0, 255);
  }
  else if (currentState == STATE_EDIT_BATTERY_CAPACITY) {
      if (evt == EVT_BTN_A_SHORT) { if(settings.batteryCapacity < 5000) settings.batteryCapacity += 50; } // + 50mAh
      else if (evt == EVT_BTN_B_SHORT) { if(settings.batteryCapacity > 100) settings.batteryCapacity -= 50; } // - 50mAh
      else if (evt == EVT_BTN_BOTH_LONG) { 
          currentState = STATE_MENU_DEVICE; 
          saveSettings();
      }
      displayMgr.drawSlider("Batt Cap", settings.batteryCapacity, 100, 3000);
  }
  else if (currentState == STATE_EDIT_AUTO_OFF_TIMEOUT) {
      if (evt == EVT_BTN_A_SHORT) { // Cycle options
          menuIndex = (menuIndex + 1) % autoOffOptionsCount;
      }
      else if (evt == EVT_BTN_B_SHORT) { // Save and exit
          settings.autoBacklightTimeout = autoOffOptionsValues[menuIndex];
          currentState = STATE_MENU_DEVICE;
          saveSettings();
          menuIndex = 4; // Return to "Auto Off" item
      }
      else if (evt == EVT_BTN_BOTH_LONG) { // Exit without saving
          currentState = STATE_MENU_DEVICE;
          menuIndex = 4;
      }
      displayMgr.drawOptionSelector("Auto Off Time", autoOffOptionsLabels, autoOffOptionsCount, menuIndex);
  }
  else if (currentState == STATE_MENU_SYSTEM) {
      static int sysInfoScroll = 0;
      const int totalSysInfoLines = 7;
      const int visibleSysInfoLines = 4;

      if (evt == EVT_BTN_A_SHORT) { if(sysInfoScroll < (totalSysInfoLines - visibleSysInfoLines)) sysInfoScroll++; else sysInfoScroll = 0; }
      else if (evt == EVT_BTN_B_SHORT || evt == EVT_BTN_BOTH_LONG) { // Back
          currentState = STATE_MENU_ROOT;
          menuIndex = 2;
          sysInfoScroll = 0; // Reset scroll on exit
      }
      
      float v = power.getBatteryVoltage();
      float remainingHours = -1.0f;
      
      if (millis() > 60000) {
          float pct = calculateBatteryPercentage(v);
          float currentMa = isBacklightActive ? 38.7f : 36.0f;
          remainingHours = (settings.batteryCapacity * pct) / currentMa;
      }
      
      displayMgr.drawSystemInfo("v1.4", millis()/1000, geiger.getTotalPulses(), v, remainingHours, sysInfoScroll);
  }
  else if (currentState == STATE_EDIT_COUNT_UNIT) {
      if (evt == EVT_BTN_A_SHORT) { // Cycle options
          menuIndex = (menuIndex + 1) % 2;
      }
      else if (evt == EVT_BTN_B_SHORT) { // Save and exit
          settings.countUnit = (CountUnit)menuIndex;
          currentState = STATE_MENU_MSRMT;
          saveSettings();
          menuIndex = 0;
      }
      else if (evt == EVT_BTN_BOTH_LONG) { // Exit without saving
          currentState = STATE_MENU_MSRMT;
          menuIndex = 0;
      }
      displayMgr.drawOptionSelector("Count Unit", countUnitOptions, 2, menuIndex);
  }
  else if (currentState == STATE_EDIT_DOSE_UNIT) {
      if (evt == EVT_BTN_A_SHORT) { // Cycle options
          menuIndex = (menuIndex + 1) % 2;
      }
      else if (evt == EVT_BTN_B_SHORT) { // Save and exit
          settings.doseUnit = (DoseUnit)menuIndex;
          currentState = STATE_MENU_MSRMT;
          saveSettings();
          menuIndex = 1;
      }
      else if (evt == EVT_BTN_BOTH_LONG) { // Exit without saving
          currentState = STATE_MENU_MSRMT;
          menuIndex = 1;
      }
      displayMgr.drawOptionSelector("Dose Unit", doseUnitOptions, 2, menuIndex);
  }
}