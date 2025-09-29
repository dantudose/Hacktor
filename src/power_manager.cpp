#include "power_manager.h"

#include <Arduino.h>
#include "esp_sleep.h"

#include "app_state.h"
#include "backlight.h"
#include "hardware_pins.h"
#include "imu.h"
#include "watchface.h"

namespace {

void lcdBusTriState() {
  backlight::prepareForSleep();

  pinMode(pins::LCD_SCK,  INPUT);
  pinMode(pins::LCD_MOSI, INPUT);
  pinMode(pins::LCD_CS,   INPUT);
  pinMode(pins::LCD_DC,   INPUT);
}

void lcdBusRestore() {
  pinMode(pins::LCD_SCK,  OUTPUT);
  pinMode(pins::LCD_MOSI, OUTPUT);
  pinMode(pins::LCD_CS,   OUTPUT);
  pinMode(pins::LCD_DC,   OUTPUT);

  backlight::restoreAfterSleep();
}

}  // namespace

namespace power_manager {

void panelSleep(Arduino_GFX &display, bool on) {
  auto &state = app_state::get();
  if (on) {
    backlight::startFade(0, 1000);
    state.pendingPanelOff = true;
  } else {
    display.displayOn();
    backlight::startFade(255, 50);
  }
}

void sleepUntilTilt(Arduino_GFX &display) {
  auto &state = app_state::get();

  esp_sleep_enable_ext1_wakeup(1ULL << pins::IMU_INT2, ESP_EXT1_WAKEUP_ANY_HIGH);

  (void)imu::read8(imu::REG_TILT_SRC);
  (void)imu::read8(imu::REG_FUNC_SRC);

  lcdBusTriState();
  delay(2);
  digitalWrite(pins::LCD_PWR, LOW);

  state.rtcBaseMs = millis();
  setCpuFrequencyMhz(20);
  esp_light_sleep_start();

  setCpuFrequencyMhz(160);
  digitalWrite(pins::LCD_PWR, HIGH);
  delay(5);

  lcdBusRestore();

  display.begin(80000000ul);
  display.fillScreen(watchface::COLOR_BG);

  (void)imu::read8(imu::REG_TILT_SRC);
  (void)imu::read8(imu::REG_FUNC_SRC);
  state.tiltIrqFlag = false;
}

void serviceTiltIRQ() {
  auto &state = app_state::get();
  if (!state.tiltIrqFlag) {
    return;
  }
  state.tiltIrqFlag = false;
  (void)imu::read8(imu::REG_TILT_SRC);
  (void)imu::read8(imu::REG_FUNC_SRC);
  if (state.displayOn) {
    state.displayExpireMs = millis() + DISPLAY_ON_TIMEOUT_MS;
  }
}

}  // namespace power_manager
