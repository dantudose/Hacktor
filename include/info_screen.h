#pragma once

#include <Arduino_GFX_Library.h>
#include <time.h>

#include "system_stats.h"

namespace info_screen {

void draw(Arduino_GFX &display, const system_stats::Stats &stats, const tm &currentTime);

}  // namespace info_screen

