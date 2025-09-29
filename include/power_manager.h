#pragma once

#include <Arduino_GFX_Library.h>

#include "app_state.h"

namespace power_manager {

inline constexpr uint32_t DISPLAY_ON_TIMEOUT_MS = 8000;  // time the screen stays on after last activity

void panelSleep(Arduino_GFX &display, bool on);
void sleepUntilTilt(Arduino_GFX &display);
void serviceTiltIRQ();
inline void flagTiltInterrupt() {
  app_state::get().tiltIrqFlag = true;
}

}  // namespace power_manager
