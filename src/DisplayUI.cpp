#include "DisplayUI.h"
#include "UIFonts.h"
#include "tangoFont.h"
#include <Preferences.h>
#include <time.h>

namespace {
constexpr int GRAPH_LEFT = 42, GRAPH_RIGHT = 470, GRAPH_TOP = 104, GRAPH_BOTTOM = 232;
constexpr int RANGE_TOP = 250, RANGE_BOTTOM = 290;
constexpr uint8_t SCREEN_ROTATION = 1;
}

void DisplayUI::selectFreeFont(const GFXfont *font) {
  if (tangoLoaded_) { tft_.unloadFont(); tangoLoaded_ = false; }
  tft_.setFreeFont(font);
}

void DisplayUI::selectTango() {
  if (tangoLoaded_) return;
  tft_.loadFont(tangoFont); tangoLoaded_ = true;
}

void DisplayUI::drawCenteredText(const String &text, const UiRect &area,
                                 const GFXfont *font, uint16_t color,
                                 uint16_t background) {
  selectFreeFont(font); tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(color, background);
  const int16_t measuredWidth = tft_.textWidth(text);
  const int16_t measuredHeight = tft_.fontHeight();
  (void)measuredWidth; (void)measuredHeight;
  tft_.drawString(text, area.centerX(), area.centerY());
}

void DisplayUI::drawCenteredTango(const String &text, const UiRect &area,
                                  uint16_t color, uint16_t background) {
  selectTango(); tft_.setTextDatum(MC_DATUM); tft_.setTextColor(color, background);
  const int16_t measuredWidth = tft_.textWidth(text);
  (void)measuredWidth;
  tft_.drawString(text, area.centerX(), area.centerY());
}

void DisplayUI::drawLeftTango(const String &text, int16_t x, int16_t centerY,
                              uint16_t color, uint16_t background) {
  selectTango(); tft_.setTextDatum(ML_DATUM); tft_.setTextColor(color, background);
  tft_.drawString(text, x, centerY);
}

const GFXfont *DisplayUI::fontThatFits(const String &text, const UiRect &area,
                                       const GFXfont *preferred,
                                       const GFXfont *fallback) {
  selectFreeFont(preferred);
  return tft_.textWidth(text) <= area.w - 4 ? preferred : fallback;
}

void DisplayUI::begin() {
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft_.init(); tft_.setRotation(SCREEN_ROTATION); tft_.setTextWrap(false); tft_.fillScreen(TFT_BLACK);
  selectTango();
  Serial.println("[DISPLAY] TFT initialized (ST7796, 480x320 landscape)");
  // Match the proven Switchbot-Blind-Desktop method: the XPT2046 uses the
  // exact HSPI instance owned by TFT_eSPI (pins 14/12/13).
  touch_.begin(tft_.getSPIinstance());
  touch_.setRotation(SCREEN_ROTATION);
  Preferences preferences; preferences.begin("dexcomui", false);
  touchCalibrated_ = preferences.getBool("touchCal", false) &&
      preferences.getUChar("touchRot", 255) == SCREEN_ROTATION;
  minX_ = preferences.getInt("touchMinX", TOUCH_MIN_X);
  maxX_ = preferences.getInt("touchMaxX", TOUCH_MAX_X);
  minY_ = preferences.getInt("touchMinY", TOUCH_MIN_Y);
  maxY_ = preferences.getInt("touchMaxY", TOUCH_MAX_Y);
  preferences.end();
  Serial.println("[TOUCH] XPT2046 initialized on TFT HSPI instance");
  if (!touchCalibrated_ || FORCE_TOUCH_CALIBRATION) calibrateTouch();
  else Serial.printf("[TOUCH] Loaded calibration x=%d..%d y=%d..%d\n", minX_, maxX_, minY_, maxY_);
  invalidate();
}

void DisplayUI::setBacklight(bool on) {
  digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
}

void DisplayUI::showWelcome(const String &version) {
  setBacklight(true); tft_.fillScreen(TFT_BLACK);

  // Compact CGM mark: a cyan trace crossing a rounded monitor tile.
  const int16_t tileX = 190, tileY = 42, tileW = 100, tileH = 76;
  tft_.fillRoundRect(tileX, tileY, tileW, tileH, 14, 0x0841);
  tft_.drawRoundRect(tileX, tileY, tileW, tileH, 14, TFT_DARKCYAN);
  const int16_t traceX[] = {204, 219, 230, 241, 252, 265, 277};
  const int16_t traceY[] = {83, 83, 67, 96, 75, 75, 75};
  for (size_t i = 1; i < sizeof(traceX) / sizeof(traceX[0]); ++i)
    tft_.drawLine(traceX[i - 1], traceY[i - 1], traceX[i], traceY[i], TFT_CYAN);
  tft_.fillCircle(traceX[6], traceY[6], 4, TFT_GREEN);

  selectTango(); tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK); tft_.drawString("ESP32 CGM", 240, 145);
  tft_.setTextColor(TFT_CYAN, TFT_BLACK); tft_.drawString("SHARE DESKTOP MONITOR", 240, 181);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft_.drawString("Persistent glucose history", 240, 224);
  tft_.drawString("Firmware " + version, 240, 260);
  tft_.drawFastHLine(170, 292, 140, TFT_DARKGREY);
  tft_.setTextDatum(TL_DATUM); screenValid_ = false;
}

void DisplayUI::showSafetyWarning() {
  setBacklight(true); tft_.fillScreen(TFT_BLACK); selectTango();
  tft_.drawRoundRect(18, 20, 444, 280, 12, TFT_ORANGE);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft_.drawString("SAFETY NOTICE", 240, 48);
  tft_.drawFastHLine(120, 82, 240, TFT_DARKGREY);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString("SECONDARY DISPLAY ONLY", 240, 108);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft_.drawString("Do not use for treatment decisions", 240, 152);
  tft_.drawString("Confirm readings and alerts with the", 240, 188);
  tft_.drawString("official Dexcom app or receiver", 240, 216);
  tft_.setTextColor(TFT_RED, TFT_BLACK);
  tft_.drawString("NOT A REPLACEMENT FOR CGM ALARMS", 240, 260);
  tft_.setTextDatum(TL_DATUM); screenValid_ = false;
}

void DisplayUI::showWifiSetup(const String &ssid) {
  setBacklight(true); tft_.fillScreen(TFT_BLACK); selectTango();
  tft_.setTextDatum(TC_DATUM); tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.drawString("WI-FI SETUP", 240, 48);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK); tft_.drawString("Connect your phone to", 240, 104);
  tft_.setTextColor(TFT_YELLOW, TFT_BLACK); tft_.drawString(ssid, 240, 142);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK); tft_.drawString("Then open http://192.168.4.1", 240, 190);
  tft_.drawString("Portal closes after 3 minutes", 240, 232);
  tft_.setTextDatum(TL_DATUM); screenValid_ = false;
}

void DisplayUI::invalidate() {
  screenValid_ = false; cachedClock_ = ""; cachedTrend_ = ""; cachedFooter_ = "";
  cachedValue_ = -1; cachedAgeSeverity_ = 255; cachedHistoryGeneration_ = UINT32_MAX;
  cachedCriticalAlert_ = false; cachedCriticalFlashPhase_ = false;
}

TouchAction DisplayUI::updateTouch(UiScreen screen) {
  const bool down = touch_.touched();
  if (down && !touchWasDown_) {
    if (millis() - lastTouchMs_ < 180) return TouchAction::NONE;
    const TS_Point point = touch_.getPoint();
    lastTouchMs_ = millis(); touchWasDown_ = true;
    pressX_ = constrain(map(point.x, minX_, maxX_, 0, 479), 0, 479);
    pressY_ = constrain(map(point.y, minY_, maxY_, 0, 319), 0, 319);
    Serial.printf("[TOUCH] raw=%d,%d mapped=%u,%u\n", point.x, point.y, pressX_, pressY_);
    // Act on initial contact instead of making the user hold pressure until
    // release. The down latch prevents repeats while the finger remains down.
    if (screen == UiScreen::STATUS) {
      if (pressY_ >= 266) {
        if (pressX_ < 112) return TouchAction::SHOW_DASHBOARD;
        if (pressX_ < 336) return TouchAction::CALIBRATE_TOUCH;
        return TouchAction::TOGGLE_CLOCK_FORMAT;
      }
      return TouchAction::NONE;
    }
    // Make the small header label comfortable to hit with a fingertip. The
    // invisible target includes the adjacent CGM indicator and extra depth.
    if (pressY_ <= 80 && pressX_ >= 355) return TouchAction::SHOW_STATUS;
    // The visible tabs are 40 px tall; the touch target deliberately extends
    // into the surrounding whitespace for comfortable fingertip operation.
    if (pressY_ >= RANGE_TOP - 16 && pressY_ <= 319) {
      if (pressX_ < 120) return TouchAction::RANGE_3H;
      if (pressX_ < 240) return TouchAction::RANGE_6H;
      if (pressX_ < 360) return TouchAction::RANGE_12H;
      return TouchAction::RANGE_24H;
    }
    return TouchAction::NONE;
  }
  if (down) return TouchAction::NONE;
  if (!touchWasDown_) return TouchAction::NONE;
  touchWasDown_ = false;
  Serial.printf("[TOUCH] Release x=%u y=%u\n", pressX_, pressY_);
  return TouchAction::NONE;
}

void DisplayUI::calibrateTouch() {
  constexpr int16_t left = 28, right = 451, top = 54, bottom = 292;
  const int16_t targets[4][2] = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
  int rawX[4]{}, rawY[4]{};
  bool valid = false;
  while (!valid) {
    Serial.println("[TOUCH] Starting four-point calibration");
    for (uint8_t i = 0; i < 4; ++i) {
      tft_.fillScreen(TFT_BLACK); tft_.setTextColor(TFT_CYAN, TFT_BLACK);
      tft_.drawCentreString("TOUCH CALIBRATION", 240, 14, 4);
      tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft_.drawCentreString("Tap and release each target", 240, 142, 2);
      tft_.drawCentreString("Point " + String(i + 1) + " of 4", 240, 166, 2);
      tft_.drawCircle(targets[i][0], targets[i][1], 16, TFT_ORANGE);
      tft_.drawFastHLine(targets[i][0] - 22, targets[i][1], 45, TFT_ORANGE);
      tft_.drawFastVLine(targets[i][0], targets[i][1] - 22, 45, TFT_ORANGE);
      while (touch_.touched()) delay(10);
      while (!touch_.touched()) delay(10);
      const TS_Point point = touch_.getPoint();
      rawX[i] = point.x; rawY[i] = point.y;
      Serial.printf("[TOUCH] Calibration point %u raw=%d,%d\n", i + 1, point.x, point.y);
      while (touch_.touched()) delay(10);
      delay(80);
    }
    const int leftX = (rawX[0] + rawX[3]) / 2;
    const int rightX = (rawX[1] + rawX[2]) / 2;
    const int topY = (rawY[0] + rawY[1]) / 2;
    const int bottomY = (rawY[2] + rawY[3]) / 2;
    valid = abs(rightX - leftX) >= 300 && abs(bottomY - topY) >= 300;
    if (!valid) { Serial.println("[TOUCH] Calibration invalid; retrying"); continue; }
    const float xScale = (rightX - leftX) / static_cast<float>(right - left);
    const float yScale = (bottomY - topY) / static_cast<float>(bottom - top);
    minX_ = lroundf(leftX - xScale * left);
    maxX_ = lroundf(leftX + xScale * (479 - left));
    minY_ = lroundf(topY - yScale * top);
    maxY_ = lroundf(topY + yScale * (319 - top));
  }
  Preferences preferences; preferences.begin("dexcomui", false);
  preferences.putBool("touchCal", true); preferences.putUChar("touchRot", SCREEN_ROTATION);
  preferences.putInt("touchMinX", minX_); preferences.putInt("touchMaxX", maxX_);
  preferences.putInt("touchMinY", minY_); preferences.putInt("touchMaxY", maxY_);
  preferences.end(); touchCalibrated_ = true; lastTouchMs_ = millis();
  tft_.fillScreen(TFT_BLACK); tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.drawCentreString("CALIBRATION SAVED", 240, 145, 4); delay(700);
  Serial.printf("[TOUCH] Calibrated x=%d..%d y=%d..%d\n", minX_, maxX_, minY_, maxY_);
}

const char *DisplayUI::trendDescription(const String &t) {
  if (t == "DoubleUp") return "RISING FAST"; if (t == "SingleUp") return "RISING";
  if (t == "FortyFiveUp") return "RISING SLOWLY"; if (t == "Flat") return "STEADY";
  if (t == "FortyFiveDown") return "FALLING SLOWLY"; if (t == "SingleDown") return "FALLING";
  if (t == "DoubleDown") return "FALLING FAST"; if (t == "RateOutOfRange") return "TREND UNAVAILABLE";
  return "NO TREND";
}

void DisplayUI::drawTrendArrow(int x, int y, int size, const String &trend, uint16_t color) {
  if (trend == "NotComputable" || trend == "RateOutOfRange" || trend == "None") {
    const UiRect symbol{static_cast<int16_t>(x - 24), static_cast<int16_t>(y - 24), 48, 48};
    drawCenteredText(trend == "None" ? "-" : "?", symbol,
                     UIFonts::Trend, color); return;
  }
  float ux = 0.0f, uy = 0.0f;
  if (trend == "Flat") ux = 1.0f;
  else if (trend == "SingleUp" || trend == "DoubleUp") uy = -1.0f;
  else if (trend == "SingleDown" || trend == "DoubleDown") uy = 1.0f;
  else if (trend == "FortyFiveUp") { ux = 0.7071f; uy = -0.7071f; }
  else { ux = 0.7071f; uy = 0.7071f; }

  // Build each symbol from a perpendicular, constant-width shaft and a
  // compact arrowhead. This keeps horizontal and diagonal trends as crisp as
  // vertical ones and gives double arrows balanced spacing.
  const float px = -uy, py = ux;
  const bool doubled = trend == "DoubleUp" || trend == "DoubleDown";
  const int arrowLength = doubled ? size - 4 : size;
  const int headLength = doubled ? 10 : 13;
  const int headHalfWidth = doubled ? 6 : 8;
  const int shaftHalfWidth = 2;
  auto pointX = [](float value) { return static_cast<int16_t>(lroundf(value)); };
  auto one = [&](float lateralOffset) {
    const float cx = x + px * lateralOffset, cy = y + py * lateralOffset;
    const float tailX = cx - ux * arrowLength / 2.0f;
    const float tailY = cy - uy * arrowLength / 2.0f;
    const float tipX = cx + ux * arrowLength / 2.0f;
    const float tipY = cy + uy * arrowLength / 2.0f;
    const float baseX = tipX - ux * headLength;
    const float baseY = tipY - uy * headLength;
    const int16_t ax = pointX(tailX + px * shaftHalfWidth);
    const int16_t ay = pointX(tailY + py * shaftHalfWidth);
    const int16_t bx = pointX(baseX + px * shaftHalfWidth);
    const int16_t by = pointX(baseY + py * shaftHalfWidth);
    const int16_t cx2 = pointX(baseX - px * shaftHalfWidth);
    const int16_t cy2 = pointX(baseY - py * shaftHalfWidth);
    const int16_t dx = pointX(tailX - px * shaftHalfWidth);
    const int16_t dy = pointX(tailY - py * shaftHalfWidth);
    tft_.fillTriangle(ax, ay, bx, by, cx2, cy2, color);
    tft_.fillTriangle(ax, ay, cx2, cy2, dx, dy, color);
    tft_.fillCircle(pointX(tailX), pointX(tailY), shaftHalfWidth, color);
    tft_.fillTriangle(pointX(tipX), pointX(tipY),
        pointX(baseX + px * headHalfWidth), pointX(baseY + py * headHalfWidth),
        pointX(baseX - px * headHalfWidth), pointX(baseY - py * headHalfWidth), color);
  };
  if (doubled) { one(-7.0f); one(7.0f); } else one(0.0f);
}

void DisplayUI::drawConnectionIndicator(int x, int y, const char *label, bool connected, bool stale) {
  const uint16_t color = connected ? (stale ? TFT_ORANGE : TFT_GREEN) : TFT_RED;
  tft_.fillCircle(x, y, 5, color);
  drawLeftTango(label, x + 9, y, TFT_WHITE);
}

void DisplayUI::drawDashboardFrame(HistoryRange range) {
  tft_.fillScreen(TFT_BLACK); tft_.drawFastHLine(0, 38, 480, TFT_DARKGREY);
  drawCenteredTango("STATUS", UILayout::StatusButton, TFT_LIGHTGREY);
  tft_.drawRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT - GRAPH_LEFT, GRAPH_BOTTOM - GRAPH_TOP, TFT_DARKGREY);
  drawRangeButtons(range);
}

void DisplayUI::drawRangeButtons(HistoryRange selected) {
  const HistoryRange ranges[] = {HistoryRange::H3, HistoryRange::H6, HistoryRange::H12, HistoryRange::H24};
  const char *labels[] = {"3H", "6H", "12H", "24H"};
  constexpr uint16_t inactiveFill = 0x18C5;  // consistent deep blue-gray
  constexpr uint16_t activeFill = 0x020C;    // darker teal selection
  for (int i = 0; i < 4; ++i) {
    const int x = i * 120; const bool active = ranges[i] == selected;
    const uint16_t fill = active ? activeFill : inactiveFill;
    tft_.fillRoundRect(x + 6, RANGE_TOP, 108, RANGE_BOTTOM - RANGE_TOP, 6, fill);
    if (active) {
      // A restrained outline and marker remain visible without washing out.
      tft_.drawRoundRect(x + 6, RANGE_TOP, 108, RANGE_BOTTOM - RANGE_TOP, 6, TFT_CYAN);
      tft_.fillRoundRect(x + 20, RANGE_BOTTOM - 7, 80, 3, 1, TFT_YELLOW);
    } else {
      tft_.drawRoundRect(x + 6, RANGE_TOP, 108, RANGE_BOTTOM - RANGE_TOP, 6, TFT_DARKGREY);
    }
    const UiRect button{static_cast<int16_t>(x + 6), RANGE_TOP, 108, RANGE_BOTTOM - RANGE_TOP};
    drawCenteredTango(labels[i], button, active ? TFT_WHITE : TFT_LIGHTGREY, fill);
  }
}

void DisplayUI::drawGraph(const GlucoseHistory &history, HistoryRange range, uint64_t nowMs) {
  tft_.fillRect(0, GRAPH_TOP - 1, 480, GRAPH_BOTTOM - GRAPH_TOP + 18, TFT_BLACK);
  tft_.drawRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT - GRAPH_LEFT, GRAPH_BOTTOM - GRAPH_TOP, TFT_DARKGREY);
  constexpr int graphMin = 40, graphMax = 300;
  auto yFor = [&](int value) { return GRAPH_BOTTOM - constrain(value, graphMin, graphMax) *
      (GRAPH_BOTTOM - GRAPH_TOP) / (graphMax - graphMin) + graphMin * (GRAPH_BOTTOM - GRAPH_TOP) / (graphMax - graphMin); };
  for (int reference : {LOW_REFERENCE, HIGH_REFERENCE, VERY_HIGH_REFERENCE}) {
    const int y = yFor(reference);
    const uint16_t color = reference == LOW_REFERENCE ? TFT_RED
        : (reference == HIGH_REFERENCE ? TFT_ORANGE : TFT_MAGENTA);
    for (int x = GRAPH_LEFT; x < GRAPH_RIGHT; x += 8) tft_.drawFastHLine(x, y, 4, color);
    // Keep reference values subordinate to the current glucose reading.
    selectFreeFont(nullptr); tft_.setTextFont(2); tft_.setTextDatum(MR_DATUM);
    tft_.setTextColor(color, TFT_BLACK); tft_.drawString(String(reference), GRAPH_LEFT - 4, y);
  }
  const uint64_t windowMs = static_cast<uint64_t>(GlucoseHistory::minutesFor(range)) * 60000ULL;
  bool havePrevious = false; int px = 0, py = 0; uint64_t pts = 0;
  for (size_t i = 0; i < history.count(); ++i) {
    const HistoryPoint &point = history.points()[i];
    if (!point.valid || point.timestampMs > nowMs || nowMs - point.timestampMs > windowMs) continue;
    const int x = GRAPH_RIGHT - static_cast<int>((nowMs - point.timestampMs) * (GRAPH_RIGHT - GRAPH_LEFT) / windowMs);
    const int y = yFor(point.value);
    if (havePrevious && point.timestampMs - pts <= 12ULL * 60000ULL) tft_.drawLine(px, py, x, y, TFT_CYAN);
    if (range == HistoryRange::H3 || range == HistoryRange::H6) tft_.fillCircle(x, y, 2, TFT_CYAN);
    px = x; py = y; pts = point.timestampMs; havePrevious = true;
  }
  selectTango(); tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const int hours = static_cast<int>(range); const int divisions = 3;
  for (int i = 0; i <= divisions; ++i) {
    const int x = GRAPH_LEFT + i * (GRAPH_RIGHT - GRAPH_LEFT) / divisions;
    String label = i == divisions ? "Now" : "-" + String(hours - i * hours / divisions) + "h";
    if (i == 0) tft_.setTextDatum(TL_DATUM);
    else if (i == divisions) tft_.setTextDatum(TR_DATUM);
    else tft_.setTextDatum(TC_DATUM);
    tft_.drawString(label, i == divisions ? GRAPH_RIGHT - 2 : x, GRAPH_BOTTOM + 2);
  }
}

void DisplayUI::renderDashboard(AppState state, const GlucoseReading &reading, int64_t age,
    const String &clock, bool wifi, HistoryRange range, const GlucoseHistory &history,
    uint64_t nowMs, bool forceGraph) {
  const bool force = !screenValid_ || renderedScreen_ != UiScreen::DASHBOARD;
  if (force) { drawDashboardFrame(range); renderedScreen_ = UiScreen::DASHBOARD; screenValid_ = true; }
  String compactDate = "--- --- --";
  if (nowMs > 1700000000000ULL) {
    time_t seconds = nowMs / 1000ULL; struct tm local{}; localtime_r(&seconds, &local);
    char compactDateText[16];
    strftime(compactDateText, sizeof(compactDateText), "%a %b %e", &local);
    compactDate = compactDateText; compactDate.toUpperCase();
  }
  const String dateKey = compactDate;
  if (force || cachedClock_ != clock || cachedDate_ != dateKey || cachedWifi_ != wifi) {
    tft_.fillRect(0, 0, 407, 37, TFT_BLACK);
    drawLeftTango(compactDate, UILayout::Date.x, UILayout::Date.centerY(), TFT_LIGHTGREY);
    selectFreeFont(UIFonts::Trend); tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString(clock, UILayout::Clock.x, UILayout::Clock.centerY());
    drawConnectionIndicator(UILayout::Wifi.x + 6, UILayout::Wifi.centerY(), "WiFi", wifi);
    const bool cgmOnline = state == AppState::NORMAL || state == AppState::STALE_DATA;
    drawConnectionIndicator(UILayout::Cgm.x + 6, UILayout::Cgm.centerY(), "CGM", cgmOnline, state == AppState::STALE_DATA);
    cachedClock_ = clock; cachedDate_ = dateKey; cachedWifi_ = wifi;
  }
  const uint8_t ageSeverity = age > 15 ? 2 : (age >= 8 ? 1 : 0);
  const bool criticalAlert = reading.valid &&
      (reading.value < CRITICAL_LOW_REFERENCE || reading.value > VERY_HIGH_REFERENCE);
  const bool criticalFlashPhase = criticalAlert && ((millis() / 600U) % 2U == 0U);
  if (force || cachedValue_ != reading.value || cachedAgeSeverity_ != ageSeverity ||
      cachedCriticalAlert_ != criticalAlert ||
      (criticalAlert && cachedCriticalFlashPhase_ != criticalFlashPhase)) {
    const uint16_t valueBackground = criticalFlashPhase ? TFT_RED : TFT_BLACK;
    tft_.fillRect(UILayout::Glucose.x, UILayout::Glucose.y,
                  UILayout::Glucose.w, UILayout::Glucose.h, valueBackground);
    uint16_t valueColor = TFT_LIGHTGREY;
    if (reading.valid) {
      if (criticalFlashPhase) valueColor = TFT_BLACK;
      else if (age > 15 || reading.value < LOW_REFERENCE) valueColor = TFT_RED;
      else if (reading.value > HIGH_REFERENCE) valueColor = TFT_YELLOW;
      else valueColor = TFT_GREEN;
    }
    drawCenteredText(reading.valid ? String(reading.value) : "---",
                     UILayout::GlucoseValue, UIFonts::Glucose,
                     valueColor, valueBackground);
    drawCenteredText("mg/dL", UILayout::Unit, UIFonts::Unit,
                     criticalFlashPhase ? TFT_BLACK : TFT_LIGHTGREY,
                     valueBackground);
    cachedValue_ = reading.value; cachedAgeSeverity_ = ageSeverity;
    cachedCriticalAlert_ = criticalAlert;
    cachedCriticalFlashPhase_ = criticalFlashPhase;
  }
  if (force || cachedTrend_ != reading.trend) {
    tft_.fillRect(UILayout::Trend.x, UILayout::Trend.y,
                  UILayout::Trend.w, UILayout::Trend.h, TFT_BLACK);
    drawTrendArrow(290, UILayout::Trend.centerY(), 44, reading.trend, TFT_CYAN);
    const String description = trendDescription(reading.trend);
    const GFXfont *trendFont = fontThatFits(description, UILayout::TrendText,
                                            UIFonts::Trend, UIFonts::Button);
    drawCenteredText(description, UILayout::TrendText, trendFont, TFT_CYAN);
    cachedTrend_ = reading.trend;
  }
  if (force || forceGraph || cachedRange_ != range || cachedHistoryGeneration_ != history.generation()) {
    drawGraph(history, range, nowMs); drawRangeButtons(range);
    cachedRange_ = range; cachedHistoryGeneration_ = history.generation();
  }
  String footer;
  if (!reading.valid) footer = "NO GLUCOSE DATA";
  else if (age > 15) footer = "NO RECENT DATA  |  Last reading " + String(age) + " min ago";
  else if (state == AppState::NETWORK_ERROR) footer = "DEXCOM OFFLINE  |  Retrying...  |  " + String(age) + " min old";
  else if (age >= 8) footer = "STALE  |  " + String(age) + " min old";
  else footer = age == 0 ? "Updated just now" : "Updated " + String(age) + " min ago";
  if (force || footer != cachedFooter_) {
    tft_.fillRect(UILayout::Footer.x, UILayout::Footer.y,
                  UILayout::Footer.w, UILayout::Footer.h, TFT_BLACK);
    drawLeftTango(footer, UILayout::Footer.x, UILayout::Footer.centerY(),
        (age >= 8 || state == AppState::NETWORK_ERROR) ? TFT_ORANGE : TFT_GREEN);
    cachedFooter_ = footer;
  }
}

String DisplayUI::formatTimestamp(uint64_t timestampMs, bool includeDate) {
  if (!timestampMs) return "Never"; time_t seconds = timestampMs / 1000ULL; struct tm local{};
  localtime_r(&seconds, &local); char buffer[32];
  strftime(buffer, sizeof(buffer), includeDate ? "%b %d %I:%M %p" : "%I:%M:%S %p", &local);
  return buffer[0] == '0' ? String(buffer + 1) : String(buffer);
}

void DisplayUI::renderStatus(const StatusData &s, const String &clock, bool force) {
  if (!screenValid_ || renderedScreen_ != UiScreen::STATUS) force = true;
  if (!force) return;
  tft_.fillScreen(TFT_BLACK); renderedScreen_ = UiScreen::STATUS; screenValid_ = true; cachedClock_ = clock;
  drawCenteredTango("SYSTEM STATUS", UiRect{0, 0, 480, 38}, TFT_CYAN);
  tft_.drawFastHLine(0, 39, 480, TFT_DARKGREY);

  drawLeftTango("WI-FI", 14, 58, TFT_WHITE);
  drawLeftTango(s.wifiConnected ? "CONNECTED" : "OFFLINE", 14, 81,
                s.wifiConnected ? TFT_GREEN : TFT_RED);
  drawLeftTango("RSSI  " + String(s.rssi) + " dBm", 14, 104, TFT_LIGHTGREY);
  drawLeftTango("IP  " + s.ip, 14, 127, TFT_LIGHTGREY);

  drawLeftTango("DEXCOM", 174, 58, TFT_WHITE);
  drawLeftTango(s.dexcomConnected ? "CONNECTED" : "OFFLINE / ERROR", 174, 81,
                s.dexcomConnected ? TFT_GREEN : TFT_RED);
  drawLeftTango(String("SESSION  ") + (s.sessionActive ? "ACTIVE" : "NONE"), 174, 104, TFT_LIGHTGREY);
  String localHostname = s.hostname;
  if (localHostname.length() > 22) localHostname = localHostname.substring(0, 19) + "...";
  drawLeftTango("http://" + localHostname + ".local", 174, 127, TFT_CYAN);

  drawLeftTango("SYSTEM", 340, 58, TFT_WHITE);
  drawLeftTango("V" FIRMWARE_VERSION, 340, 81, TFT_LIGHTGREY);
  drawLeftTango(String("NTP  ") + (s.ntpSynchronized ? "SYNCED" : "WAITING"), 340, 104, TFT_LIGHTGREY);

  tft_.drawFastHLine(14, 147, 452, TFT_DARKGREY);
  drawLeftTango("HISTORY", 14, 166, TFT_WHITE);
  drawLeftTango(String(s.historyCount) + " readings  |  Oldest " +
      formatTimestamp(s.oldestReadingMs, true) + "  |  Age " +
      String(s.readingAgeMinutes) + " min", 14, 191, TFT_LIGHTGREY);
  drawLeftTango("Last request  " + formatTimestamp(s.lastSuccessMs), 14, 214, TFT_LIGHTGREY);
  drawLeftTango("Heap  " + String(s.freeHeap / 1024) + " KB  |  Min " +
      String(s.minHeap / 1024) + " KB  |  Up " +
      String(s.uptimeSeconds / 3600) + "h", 14, 237, TFT_LIGHTGREY);
  const String sdLine = s.sdMounted
      ? "SD  " + s.sdType + "  |  Free " + String(s.sdFreeBytes / 1048576ULL) + " MB"
      : "SD  NOT AVAILABLE - RAM LOGGING ONLY";
  drawLeftTango(sdLine, 14, 260,
                !s.sdMounted || s.sdLowSpace ? TFT_ORANGE : TFT_GREEN);

  constexpr UiRect backButton{10, 275, 96, 38};
  constexpr UiRect calibrationButton{116, 275, 210, 38};
  constexpr UiRect clockButton{336, 275, 134, 38};
  tft_.fillRoundRect(backButton.x, backButton.y, backButton.w, backButton.h, 6, TFT_DARKCYAN);
  drawCenteredTango("BACK", backButton, TFT_WHITE, TFT_DARKCYAN);
  tft_.fillRoundRect(calibrationButton.x, calibrationButton.y,
                     calibrationButton.w, calibrationButton.h, 6, 0x18C5);
  drawCenteredTango("CALIBRATE TOUCH", calibrationButton, TFT_WHITE, 0x18C5);
  tft_.fillRoundRect(clockButton.x, clockButton.y, clockButton.w, clockButton.h, 6, 0x18C5);
  drawCenteredTango(s.clock24Hour ? "24H CLOCK" : "12H CLOCK",
                    clockButton, TFT_WHITE, 0x18C5);
}
