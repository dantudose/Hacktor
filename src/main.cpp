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

/* ---------------- Display ---------------- */
Arduino_DataBus *bus = new Arduino_ESP32SPI(pins::LCD_DC, pins::LCD_CS, pins::LCD_SCK, pins::LCD_MOSI, GFX_NOT_DEFINED, HSPI);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, 3, 1, true);

/* -------- Backlight PWM ramp (non-blocking) -------- */

/* ISRs */
void IRAM_ATTR imuInt1ISR() { steps::flagInterrupt(); }
void IRAM_ATTR imuInt2ISR() { power_manager::flagTiltInterrupt(); }

/* ---------------- Setup ---------------- */
void setup() {

  auto &state = app_state::get();

  setCpuFrequencyMhz(160);
  
  Serial.begin(115200);

  watchface::init();
  
  pinMode(pins::LCD_PWR, OUTPUT); digitalWrite(pins::LCD_PWR, HIGH);
  pinMode(pins::LCD_BL,  OUTPUT); digitalWrite(pins::LCD_BL,  HIGH);

  backlight::init(pins::LCD_BL);

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL);
  Wire.setClock(100000);
  delay(150);

  gfx->begin(80000000ul);
  gfx->fillScreen(watchface::COLOR_BG);
  time_keeper::initializeFromCompileTime();

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

  watchface::drawFullFaceAndHands(
    *gfx,
    state.currentTime,
    steps::today(),
    state.batteryPercent,
    state.prevHourX, state.prevHourY,
    state.prevMinuteX, state.prevMinuteY,
    state.prevSecondX, state.prevSecondY,
    state.prevSecondTailX, state.prevSecondTailY
  );

  state.lastTickMs     = millis();
  state.rtcBaseMs      = state.lastTickMs;
  state.lastBatteryPollMs = millis() - 60000; // force an immediate first poll
  state.displayOn     = true;
  state.displayExpireMs = millis() + power_manager::DISPLAY_ON_TIMEOUT_MS;
  Wire.setClock(400000);
}

/* ---------------- Loop ---------------- */
void loop() {
  auto &state = app_state::get();
  // Always keep the BL ramp running
  backlight::update();

  // While awake, a tilt extends screen-on timeout
  power_manager::serviceTiltIRQ();

  // Steps keep updating whether screen is on or off (IMU runs)
  steps::serviceInterrupt();
  steps::pollWatchdog(millis());

  // Battery % (polled at 60s cadence regardless of screen state)
  battery_monitor::poll();

  // If timeout hits, begin fade-out (non-blocking) and plan to sleep later
  if (!state.pendingSleep && millis() > state.displayExpireMs) {
    imu::setAccelODR(0x20);     // 52 Hz while off
    power_manager::panelSleep(*gfx, true);         // begin fade-out; panelOff happens after fade
    state.pendingSleep = true;     // we'll sleep once fade completes and panel is off
  }

  if (state.displayOn) {
    unsigned long now = millis();
    unsigned long elapsed_s = (now > state.lastTickMs) ? ((now - state.lastTickMs) / 1000UL) : 0UL;
    if (elapsed_s > 0) {
      time_keeper::applyElapsedWalltime();   // advance currentTime by real elapsed seconds

      // Compute new endpoints
      int nhx, nhy, nmx, nmy, nsx, nsy, ntx, nty;
      watchface::calcHourEnd(state.currentTime, nhx, nhy);
      watchface::calcMinuteEnd(state.currentTime, nmx, nmy);
      watchface::calcSecondEnds(state.currentTime, nsx, nsy, ntx, nty);

      bool needHour   = (nhx != state.prevHourX) || (nhy != state.prevHourY);
      bool needMinute = (nmx != state.prevMinuteX) || (nmy != state.prevMinuteY);

      // Erase previous second hand only
      gfx->drawLine(watchface::CENTER_X, watchface::CENTER_Y, state.prevSecondX, state.prevSecondY, watchface::COLOR_BG);
      gfx->drawLine(watchface::CENTER_X, watchface::CENTER_Y, state.prevSecondTailX, state.prevSecondTailY, watchface::COLOR_BG);
      gfx->fillCircle(watchface::CENTER_X, watchface::CENTER_Y, 6, watchface::COLOR_BG);

      // Update overlays that might clear pixels (tight boxes; no overlap)
      watchface::drawDateRotatedCWRightOfCenter(*gfx, state.currentTime);
      watchface::drawStepsBelowCenter(*gfx, steps::today());
      watchface::drawBatteryRotatedCWLeftOfCenter(*gfx, state.batteryPercent);

      // Redraw ticks (thin and fast; placed after boxes to avoid erase)
      watchface::drawTicks(*gfx);

      // Hour/minute: only change when endpoint moves; otherwise repaint
      if (needHour) {
        watchface::drawThick3Line(*gfx, watchface::CENTER_X, watchface::CENTER_Y, state.prevHourX, state.prevHourY, watchface::COLOR_BG);
        watchface::drawThick3Line(*gfx, watchface::CENTER_X, watchface::CENTER_Y, nhx, nhy, watchface::COLOR_HOUR_HAND);
        state.prevHourX = nhx; state.prevHourY = nhy;
      } else {
        watchface::drawThick3Line(*gfx, watchface::CENTER_X, watchface::CENTER_Y, state.prevHourX, state.prevHourY, watchface::COLOR_HOUR_HAND);
      }

      if (needMinute) {
        watchface::drawThick3Line(*gfx, watchface::CENTER_X, watchface::CENTER_Y, state.prevMinuteX, state.prevMinuteY, watchface::COLOR_BG);
        watchface::drawThick3Line(*gfx, watchface::CENTER_X, watchface::CENTER_Y, nmx, nmy, watchface::COLOR_MIN_HAND);
        state.prevMinuteX = nmx; state.prevMinuteY = nmy;
      } else {
        watchface::drawThick3Line(*gfx, watchface::CENTER_X, watchface::CENTER_Y, state.prevMinuteX, state.prevMinuteY, watchface::COLOR_MIN_HAND);
      }

      // Draw new second hand
      gfx->drawLine(watchface::CENTER_X, watchface::CENTER_Y, nsx, nsy, watchface::COLOR_SEC_HAND);
      gfx->drawLine(watchface::CENTER_X, watchface::CENTER_Y, ntx, nty, watchface::COLOR_SEC_HAND);
      gfx->fillCircle(watchface::CENTER_X, watchface::CENTER_Y, 6, watchface::COLOR_FACE);
      gfx->fillCircle(watchface::CENTER_X, watchface::CENTER_Y, 3, watchface::COLOR_SEC_HAND);
      state.prevSecondX = nsx; state.prevSecondY = nsy;
      state.prevSecondTailX = ntx; state.prevSecondTailY = nty;

      state.lastTickMs += elapsed_s * 1000UL;
    }
  }

  // If we owe a panel-off, do it after fade reaches 0
  if (state.pendingPanelOff && backlight::isIdle()) {
    gfx->displayOff();
    state.pendingPanelOff = false;
  }

  // If fade-out finished and panel is off, actually enter light sleep now
  if (state.pendingSleep && !state.pendingPanelOff && backlight::isIdle()) {
    state.displayOn = false;

    // Sleep until wrist tilt
    power_manager::sleepUntilTilt(*gfx);            // blocks; wakes on INT2

    // Wake path: panel on + fade-in (non-blocking), restore ODR
    power_manager::panelSleep(*gfx, false);           // displayOn + fade to 255
    imu::setAccelODR(0x40);        // 104 Hz while on

    time_keeper::applyElapsedWalltime();      // catch up time
    watchface::drawFullFaceAndHands(
      *gfx,
      state.currentTime,
      steps::today(),
      state.batteryPercent,
      state.prevHourX, state.prevHourY,
      state.prevMinuteX, state.prevMinuteY,
      state.prevSecondX, state.prevSecondY,
      state.prevSecondTailX, state.prevSecondTailY
    );

    state.displayOn        = true;
    state.displayExpireMs = millis() + power_manager::DISPLAY_ON_TIMEOUT_MS;
    state.lastTickMs        = millis();
    state.rtcBaseMs         = state.lastTickMs;
    state.pendingSleep     = false;
  }
}

/* ---------------- Battery helpers ---------------- */
