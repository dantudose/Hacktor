#include "display_manager.h"

#include <Arduino.h>

#include "hardware_pins.h"

namespace display_manager {
namespace {
Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

void ensureCreated() {
  if (!bus) {
    bus = new Arduino_ESP32SPI(
      pins::LCD_DC,
      pins::LCD_CS,
      pins::LCD_SCK,
      pins::LCD_MOSI,
      GFX_NOT_DEFINED,
      HSPI
    );
  }
  if (!gfx) {
    gfx = new Arduino_GC9A01(bus, 3, 1, true);
  }
}
}  // namespace

void init() {
  ensureCreated();
}

Arduino_GFX &get() {
  ensureCreated();
  return *gfx;
}

void begin(uint32_t freq_hz) {
  ensureCreated();
  gfx->begin(freq_hz);
}

void reinitializeAfterWake() {
  begin();
}

}  // namespace display_manager

