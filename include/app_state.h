#pragma once

#include <stdint.h>
#include <time.h>

namespace app_state {

struct Runtime {
  tm currentTime{};
  int prevHourX = 0;
  int prevHourY = 0;
  int prevMinuteX = 0;
  int prevMinuteY = 0;
  int prevSecondX = 0;
  int prevSecondY = 0;
  int prevSecondTailX = 0;
  int prevSecondTailY = 0;
  unsigned long lastTickMs = 0;
  unsigned long rtcBaseMs = 0;
  uint8_t batteryPercent = 0;
  unsigned long lastBatteryPollMs = 0;
  bool displayOn = true;
  uint32_t displayExpireMs = 0;
  bool pendingSleep = false;
  bool pendingPanelOff = false;
  bool tiltIrqFlag = false;
};

Runtime &get();

}  // namespace app_state

