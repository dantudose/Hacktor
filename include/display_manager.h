#pragma once

#include <Arduino_GFX_Library.h>

namespace display_manager {

void init();
Arduino_GFX &get();
void begin(uint32_t freq_hz = 80000000ul);
void reinitializeAfterWake();

}  // namespace display_manager

