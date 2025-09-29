#include "time_keeper.h"

#include "app_state.h"
#include "steps.h"

#include <Arduino.h>
#include <string.h>
#include <stdio.h>

namespace {
int weekdayFromDate(int y, int m, int d) {
  if (m < 3) {
    m += 12;
    y--;
  }
  int K = y % 100;
  int J = y / 100;
  int h = (d + 13 * (m + 1) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
  return (h + 6) % 7;
}
}  // namespace

namespace time_keeper {

void initializeFromCompileTime() {
  auto &state = app_state::get();
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mStr[4];
  int day, year, hour, min, sec;
  sscanf(__DATE__, "%3s %d %d", mStr, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
  int month = static_cast<int>((strstr(months, mStr) - months) / 3) + 1;
  state.currentTime = { sec, min, hour, day, month - 1, year - 1900 };
  state.currentTime.tm_wday = weekdayFromDate(year, month, day);
}

void applyElapsedWalltime() {
  auto &state = app_state::get();
  unsigned long now = millis();
  unsigned long elapsed = now - state.rtcBaseMs;
  if (elapsed == 0) {
    return;
  }
  unsigned long secs = elapsed / 1000UL;
  if (secs == 0) {
    return;
  }
  state.rtcBaseMs += secs * 1000UL;

  for (unsigned long i = 0; i < secs; ++i) {
    if (++state.currentTime.tm_sec >= 60) {
      state.currentTime.tm_sec = 0;
      if (++state.currentTime.tm_min >= 60) {
        state.currentTime.tm_min = 0;
        if (++state.currentTime.tm_hour >= 24) {
          state.currentTime.tm_hour = 0;
          steps::resetDailyBaseline();
        }
      }
    }
  }
}

}  // namespace time_keeper
