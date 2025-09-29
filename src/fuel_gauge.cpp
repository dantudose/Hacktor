#include "fuel_gauge.h"

#include <Wire.h>

namespace fuel_gauge {

bool readSOC(float &pct) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(REG_SOC);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(static_cast<int>(ADDRESS), 2);
  if (Wire.available() < 2) {
    return false;
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  pct = static_cast<float>(msb) + (static_cast<float>(lsb) / 256.0f);
  return true;
}

}  // namespace fuel_gauge

