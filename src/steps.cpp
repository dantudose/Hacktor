#include "steps.h"

#include <Arduino.h>

#include "imu.h"

namespace steps {
namespace {
volatile bool irqFlag = false;
uint32_t hwTotal = 0;
uint16_t hwPrev16 = 0;
uint32_t currentBaseline = 0;
uint32_t stepsToday = 0;
unsigned long lastPollMs = 0;

void updateSteps(uint16_t s16) {
  if (s16 < hwPrev16) {
    hwTotal += 0x10000 - hwPrev16 + s16;
  } else {
    hwTotal += s16 - hwPrev16;
  }
  hwPrev16 = s16;
  stepsToday = (hwTotal >= currentBaseline) ? hwTotal - currentBaseline : 0;
}
}  // namespace

void init(uint16_t initialHardwareCount) {
  hwPrev16 = hwTotal = initialHardwareCount;
  currentBaseline = hwTotal;
  stepsToday = 0;
  lastPollMs = millis();
  irqFlag = false;
}

void resetDailyBaseline() {
  currentBaseline = hwTotal;
  stepsToday = 0;
}

void flagInterrupt() {
  irqFlag = true;
}

void serviceInterrupt() {
  if (!irqFlag) {
    return;
  }
  irqFlag = false;
  (void)imu::read8(imu::REG_FUNC_SRC);
  uint16_t s16;
  if (imu::read16(imu::REG_STEP_COUNTER_L, s16)) {
    updateSteps(s16);
  }
}

void pollWatchdog(unsigned long now) {
  if (now - lastPollMs < 1000UL) {
    return;
  }
  lastPollMs = now;
  uint16_t s16;
  if (imu::read16(imu::REG_STEP_COUNTER_L, s16)) {
    updateSteps(s16);
  }
}

uint32_t hardwareTotal() {
  return hwTotal;
}

uint32_t today() {
  return stepsToday;
}

uint32_t baseline() {
  return currentBaseline;
}

}  // namespace steps
