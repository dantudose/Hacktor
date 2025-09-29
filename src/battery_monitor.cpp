#include "battery_monitor.h"

#include <math.h>
#include <Arduino.h>

#include "app_state.h"
#include "fuel_gauge.h"

namespace battery_monitor {

void poll() {
  auto &state = app_state::get();
  unsigned long now = millis();
  if (now - state.lastBatteryPollMs < 60000UL) {
    return;
  }
  state.lastBatteryPollMs = now;

  float soc;
  if (fuel_gauge::readSOC(soc)) {
    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;
    state.batteryPercent = static_cast<uint8_t>(lroundf(soc));
  }
}

}  // namespace battery_monitor

