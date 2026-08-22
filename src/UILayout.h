#pragma once
#include <Arduino.h>

struct UiRect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  constexpr int16_t centerX() const { return x + w / 2; }
  constexpr int16_t centerY() const { return y + h / 2; }
};

namespace UILayout {
constexpr UiRect TopBar{0, 0, 480, 38};
constexpr UiRect Date{8, 0, 105, 38};
constexpr UiRect Clock{112, 0, 153, 38};
constexpr UiRect Wifi{270, 0, 70, 38};
constexpr UiRect Cgm{342, 0, 65, 38};
constexpr UiRect StatusButton{407, 0, 73, 38};
constexpr UiRect Glucose{5, 40, 250, 63};
constexpr UiRect GlucoseValue{18, 40, 165, 63};
constexpr UiRect Unit{157, 47, 80, 54};
constexpr UiRect Trend{255, 40, 225, 63};
constexpr UiRect TrendText{326, 40, 150, 63};
constexpr UiRect Graph{0, 103, 480, 145};
constexpr UiRect RangeButtons{0, 250, 480, 40};
constexpr UiRect Footer{8, 292, 464, 28};
}  // namespace UILayout
