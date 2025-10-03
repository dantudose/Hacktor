#include "display_manager.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "hardware_pins.h"

namespace display_manager {
namespace {

class ArduinoGraphicsAdapter final : public graphics::Graphics {
 public:
  explicit ArduinoGraphicsAdapter(Arduino_GFX &impl) : impl_(impl) {}

  void fillScreen(uint16_t color) override { impl_.fillScreen(color); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    impl_.fillRect(x, y, w, h, color);
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    impl_.drawRect(x, y, w, h, color);
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
    impl_.drawLine(x0, y0, x1, y1, color);
  }
  void fillTriangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    uint16_t color
  ) override {
    impl_.fillTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override {
    impl_.fillCircle(x0, y0, r, color);
  }

  uint8_t getRotation() const override { return impl_.getRotation(); }
  void setRotation(uint8_t rotation) override { impl_.setRotation(rotation); }

  void drawText(
    int16_t x, int16_t y,
    const char *text,
    uint16_t colorText, uint16_t colorBG,
    uint8_t textSize
  ) override {
    impl_.setTextWrap(false);
    impl_.setTextSize(textSize);
    impl_.setTextColor(colorText, colorBG);
    impl_.setCursor(x, y);
    impl_.print(text);
    impl_.setTextWrap(true);
  }

  int16_t width() const override { return impl_.width(); }
  int16_t height() const override { return impl_.height(); }

  void displayOn() override { impl_.displayOn(); }
  void displayOff() override { impl_.displayOff(); }

 private:
  Arduino_GFX &impl_;
};

Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;
ArduinoGraphicsAdapter *adapter = nullptr;

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
  if (!adapter && gfx) {
    adapter = new ArduinoGraphicsAdapter(*gfx);
  }
}

}  // namespace

void init() {
  ensureCreated();
}

graphics::Graphics &get() {
  ensureCreated();
  return *adapter;
}

void begin(uint32_t freq_hz) {
  ensureCreated();
  gfx->begin(freq_hz);
}

void reinitializeAfterWake() {
  begin();
}

}  // namespace display_manager

