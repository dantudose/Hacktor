#pragma once

#include <cstdint>

namespace graphics {

class Graphics {
 public:
  virtual ~Graphics() = default;

  virtual void fillScreen(uint16_t color) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
  virtual void fillTriangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    uint16_t color
  ) = 0;
  virtual void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;

  virtual uint8_t getRotation() const = 0;
  virtual void setRotation(uint8_t rotation) = 0;

  virtual void setCursor(int16_t x, int16_t y) = 0;
  virtual void setTextColor(uint16_t foreground, uint16_t background) = 0;
  virtual void setTextSize(uint8_t size) = 0;
  virtual void setTextWrap(bool wrap) = 0;
  virtual void print(const char *text) = 0;

  virtual void getTextBounds(
    const char *text,
    int16_t x, int16_t y,
    int16_t *x1, int16_t *y1,
    uint16_t *w, uint16_t *h
  ) = 0;

  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;

  virtual void displayOn() = 0;
  virtual void displayOff() = 0;
};

}  // namespace graphics

