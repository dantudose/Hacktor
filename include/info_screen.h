#pragma once

#include <time.h>

#include "graphics.h"
#include "system_stats.h"

namespace info_screen {

void draw(graphics::Graphics &display, const system_stats::Stats &stats, const tm &currentTime);

}  // namespace info_screen

