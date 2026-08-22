#pragma once
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"
#include "DexcomClient.h"
#include "GlucoseHistory.h"
#include "UILayout.h"

enum class UiScreen : uint8_t { DASHBOARD, STATUS };
enum class TouchAction : uint8_t {
  NONE, SHOW_STATUS, SHOW_DASHBOARD, RANGE_3H, RANGE_6H, RANGE_12H,
  RANGE_24H, CALIBRATE_TOUCH, TOGGLE_CLOCK_FORMAT, SCREEN_OFF, WAKE_SCREEN
};

struct StatusData {
  bool wifiConnected = false;
  int rssi = 0;
  String ip;
  bool dexcomConnected = false;
  bool sessionActive = false;
  bool ntpSynchronized = false;
  bool clock24Hour = false;
  String hostname;
  uint64_t lastSuccessMs = 0;
  uint64_t latestReadingMs = 0;
  int64_t readingAgeMinutes = -1;
  size_t historyCount = 0;
  uint64_t oldestReadingMs = 0;
  uint32_t uptimeSeconds = 0;
  uint32_t freeHeap = 0;
  uint32_t minHeap = 0;
  String lastError;
  bool sdMounted = false;
  bool sdLowSpace = false;
  String sdType;
  uint64_t sdFreeBytes = 0;
  String sdCurrentFile;
  uint64_t sdLastWriteMs = 0;
};

class DisplayUI {
 public:
  void begin();
  TouchAction updateTouch(UiScreen screen);
  void invalidate();
  void calibrateTouch();
  void setBacklight(bool on);
  bool backlightOn() const { return backlightOn_; }
  void showWifiSetup(const String &ssid);
  void showWelcome(const String &version);
  void showSafetyWarning();
  void renderDashboard(AppState state, const GlucoseReading &reading,
      int64_t ageMinutes, const String &clock, bool wifiConnected,
      HistoryRange range, const GlucoseHistory &history, uint64_t nowMs,
      bool forceGraph = false);
  void renderStatus(const StatusData &status, const String &clock, bool force = false);

 private:
  TFT_eSPI tft_;
  XPT2046_Touchscreen touch_{33, 36};
  bool touchWasDown_ = false;
  uint16_t pressX_ = 0, pressY_ = 0;
  uint32_t lastTouchMs_ = 0;
  uint32_t lastCenterTapMs_ = 0;
  int minX_ = TOUCH_MIN_X, maxX_ = TOUCH_MAX_X;
  int minY_ = TOUCH_MIN_Y, maxY_ = TOUCH_MAX_Y;
  bool touchCalibrated_ = false;
  bool backlightOn_ = true;
  bool tangoLoaded_ = false;
  bool screenValid_ = false;
  UiScreen renderedScreen_ = UiScreen::DASHBOARD;
  String cachedClock_, cachedDate_, cachedTrend_, cachedFooter_;
  int cachedValue_ = -1;
  uint8_t cachedAgeSeverity_ = 255;
  bool cachedCriticalAlert_ = false;
  bool cachedCriticalFlashPhase_ = false;
  bool cachedWifi_ = false;
  HistoryRange cachedRange_ = HistoryRange::H3;
  uint32_t cachedHistoryGeneration_ = UINT32_MAX;

  void drawDashboardFrame(HistoryRange range);
  void drawGraph(const GlucoseHistory &history, HistoryRange range, uint64_t nowMs);
  void drawRangeButtons(HistoryRange selected);
  void drawTrendArrow(int x, int y, int size, const String &trend, uint16_t color);
  void drawConnectionIndicator(int x, int y, const char *label, bool connected, bool stale = false);
  void drawCenteredText(const String &text, const UiRect &area,
                        const GFXfont *font, uint16_t color,
                        uint16_t background = TFT_BLACK);
  void drawCenteredTango(const String &text, const UiRect &area,
                         uint16_t color, uint16_t background = TFT_BLACK);
  void drawLeftTango(const String &text, int16_t x, int16_t centerY,
                     uint16_t color, uint16_t background = TFT_BLACK);
  void selectFreeFont(const GFXfont *font);
  void selectTango();
  const GFXfont *fontThatFits(const String &text, const UiRect &area,
                              const GFXfont *preferred,
                              const GFXfont *fallback);
  static const char *trendDescription(const String &trend);
  static String formatTimestamp(uint64_t timestampMs, bool includeDate = false);
};
