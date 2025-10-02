#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <time.h>
#include "imu.h"
#include "fuel_gauge.h"
#include "backlight.h"
#include "watchface.h"
#include "steps.h"
#include "app_state.h"
#include "hardware_pins.h"
#include "time_keeper.h"
#include "power_manager.h"
#include "battery_monitor.h"
#include "display_manager.h"
#include "ble_time_sync.h"

/* -------- Backlight PWM ramp (non-blocking) -------- */

/* ISRs */
void IRAM_ATTR imuInt1ISR() { steps::flagInterrupt(); }
void IRAM_ATTR imuInt2ISR() { power_manager::flagTiltInterrupt(); }
//bla
/* ---------------- Setup ---------------- */
void setup() {

  auto &state = app_state::get();
  auto &displayState = state.display;
  auto &powerState = state.power;
  auto &batteryState = state.battery;

  setCpuFrequencyMhz(160);
  
  Serial.begin(115200);

  watchface::init();
  display_manager::init();
  
  pinMode(pins::LCD_PWR, OUTPUT); digitalWrite(pins::LCD_PWR, HIGH);
  pinMode(pins::LCD_BL,  OUTPUT); digitalWrite(pins::LCD_BL,  HIGH);

  backlight::init(pins::LCD_BL);

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL);
  Wire.setClock(100000);
  delay(150);

  display_manager::begin();
  display_manager::get().fillScreen(watchface::COLOR_BG);
  time_keeper::initializeFromCompileTime();
  ble_time_sync::init();
  ble_time_sync::requestImmediateSync();

  if (!imu::waitWhoAmI(400))
    Serial.println("IMU not ready (WHO_AM_I) — watchdog will poll");

  imu::softReset();
  uint16_t initialSteps = 0;
  bool pedo_ok = imu::enableHardwarePedometer(initialSteps);
  steps::init(initialSteps);
  Serial.printf("Hardware pedometer: %s\n", pedo_ok ? "OK" : "FAILED");
  bool tilt_ok = imu::enableTiltOnInt2();
  Serial.printf("Tilt on INT2: %s\n", tilt_ok ? "OK" : "FAILED");

  pinMode(pins::IMU_INT1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pins::IMU_INT1), imuInt1ISR, RISING);

  pinMode(pins::IMU_INT2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pins::IMU_INT2), imuInt2ISR, RISING);

  uint16_t s16;
  if (imu::read16(imu::REG_STEP_COUNTER_L, s16)) {
    steps::init(s16);
  }

  auto &display = display_manager::get();
  watchface::drawFullFaceAndHands(
    display,
    displayState.currentTime,
    steps::today(),
    batteryState.percent,
    displayState.prevHourX, displayState.prevHourY,
    displayState.prevMinuteX, displayState.prevMinuteY,
    displayState.prevSecondX, displayState.prevSecondY,
    displayState.prevSecondTailX, displayState.prevSecondTailY
  );

  displayState.lastTickMs     = millis();
  displayState.rtcBaseMs      = displayState.lastTickMs;
  batteryState.lastPollMs = millis() - 60000; // force an immediate first poll
  powerState.displayOn     = true;
  powerState.displayExpireMs = millis() + power_manager::DISPLAY_ON_TIMEOUT_MS;
  Wire.setClock(400000);
}

/* ---------------- Loop ---------------- */
namespace {
void processSteps() {
  steps::serviceInterrupt();
  steps::pollWatchdog(millis());
}

void handleDisplayTimeout() {
  auto &state = app_state::get();
  auto &powerState = state.power;
  if (!powerState.pendingSleep && millis() > powerState.displayExpireMs) {
    imu::setAccelODR(0x20);     // 52 Hz while off
    power_manager::panelSleep(true);         // begin fade-out; panelOff happens after fade
    powerState.pendingSleep = true;
  }
}

void refreshDisplayIfNeeded(Arduino_GFX &display) {
  auto &state = app_state::get();
  auto &displayState = state.display;
  auto &powerState = state.power;
  auto &batteryState = state.battery;

  if (!powerState.displayOn) {
    return;
  }

  unsigned long now = millis();
  unsigned long elapsed_s = (now > displayState.lastTickMs) ? ((now - displayState.lastTickMs) / 1000UL) : 0UL;
  if (elapsed_s == 0) {
    return;
  }

  time_keeper::applyElapsedWalltime();

  int nhx, nhy, nmx, nmy, nsx, nsy, ntx, nty;
  watchface::calcHourEnd(displayState.currentTime, nhx, nhy);
  watchface::calcMinuteEnd(displayState.currentTime, nmx, nmy);
  watchface::calcSecondEnds(displayState.currentTime, nsx, nsy, ntx, nty);

  bool needHour   = (nhx != displayState.prevHourX) || (nhy != displayState.prevHourY);
  bool needMinute = (nmx != displayState.prevMinuteX) || (nmy != displayState.prevMinuteY);

  display.drawLine(watchface::CENTER_X, watchface::CENTER_Y, displayState.prevSecondX, displayState.prevSecondY, watchface::COLOR_BG);
  display.drawLine(watchface::CENTER_X, watchface::CENTER_Y, displayState.prevSecondTailX, displayState.prevSecondTailY, watchface::COLOR_BG);
  display.fillCircle(watchface::CENTER_X, watchface::CENTER_Y, 6, watchface::COLOR_BG);

  watchface::drawDateRotatedCWRightOfCenter(display, displayState.currentTime);
  watchface::drawStepsBelowCenter(display, steps::today());
  watchface::drawBatteryRotatedCWLeftOfCenter(display, batteryState.percent);
  watchface::drawTicks(display);

  if (needHour) {
    watchface::drawThick3Line(display, watchface::CENTER_X, watchface::CENTER_Y, displayState.prevHourX, displayState.prevHourY, watchface::COLOR_BG);
    watchface::drawThick3Line(display, watchface::CENTER_X, watchface::CENTER_Y, nhx, nhy, watchface::COLOR_HOUR_HAND);
    displayState.prevHourX = nhx;
    displayState.prevHourY = nhy;
  } else {
    watchface::drawThick3Line(display, watchface::CENTER_X, watchface::CENTER_Y, displayState.prevHourX, displayState.prevHourY, watchface::COLOR_HOUR_HAND);
  }

  if (needMinute) {
    watchface::drawThick3Line(display, watchface::CENTER_X, watchface::CENTER_Y, displayState.prevMinuteX, displayState.prevMinuteY, watchface::COLOR_BG);
    watchface::drawThick3Line(display, watchface::CENTER_X, watchface::CENTER_Y, nmx, nmy, watchface::COLOR_MIN_HAND);
    displayState.prevMinuteX = nmx;
    displayState.prevMinuteY = nmy;
  } else {
    watchface::drawThick3Line(display, watchface::CENTER_X, watchface::CENTER_Y, displayState.prevMinuteX, displayState.prevMinuteY, watchface::COLOR_MIN_HAND);
  }

  display.drawLine(watchface::CENTER_X, watchface::CENTER_Y, nsx, nsy, watchface::COLOR_SEC_HAND);
  display.drawLine(watchface::CENTER_X, watchface::CENTER_Y, ntx, nty, watchface::COLOR_SEC_HAND);
  display.fillCircle(watchface::CENTER_X, watchface::CENTER_Y, 6, watchface::COLOR_FACE);
  display.fillCircle(watchface::CENTER_X, watchface::CENTER_Y, 3, watchface::COLOR_SEC_HAND);
  displayState.prevSecondX = nsx;
  displayState.prevSecondY = nsy;
  displayState.prevSecondTailX = ntx;
  displayState.prevSecondTailY = nty;

  displayState.lastTickMs += elapsed_s * 1000UL;
}

void handlePendingSleep(Arduino_GFX &display) {
  auto &state = app_state::get();
  auto &displayState = state.display;
  auto &powerState = state.power;
  auto &batteryState = state.battery;

  if (powerState.pendingPanelOff && backlight::isIdle()) {
    display.displayOff();
    powerState.pendingPanelOff = false;
  }

  if (!powerState.pendingSleep || powerState.pendingPanelOff || !backlight::isIdle()) {
    return;
  }

  powerState.displayOn = false;
  power_manager::sleepUntilTilt();
  power_manager::panelSleep(false);
  imu::setAccelODR(0x40);

  time_keeper::applyElapsedWalltime();
  watchface::drawFullFaceAndHands(
    display,
    displayState.currentTime,
    steps::today(),
    batteryState.percent,
    displayState.prevHourX, displayState.prevHourY,
    displayState.prevMinuteX, displayState.prevMinuteY,
    displayState.prevSecondX, displayState.prevSecondY,
    displayState.prevSecondTailX, displayState.prevSecondTailY
  );

  powerState.displayOn        = true;
  powerState.displayExpireMs  = millis() + power_manager::DISPLAY_ON_TIMEOUT_MS;
  displayState.lastTickMs     = millis();
  displayState.rtcBaseMs      = displayState.lastTickMs;
  powerState.pendingSleep     = false;
}
}  // namespace

void loop() {
  auto &display = display_manager::get();

  ble_time_sync::service();

  backlight::update();
  power_manager::serviceTiltIRQ();
  processSteps();
  battery_monitor::poll();
  handleDisplayTimeout();
  refreshDisplayIfNeeded(display);
  handlePendingSleep(display);
}
