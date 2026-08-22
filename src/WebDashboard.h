#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "GlucoseHistory.h"
#include "SDLogger.h"
#include "DeviceSettings.h"

class WebDashboard {
 public:
  WebDashboard(GlucoseHistory &history, SDLogger &logger, DeviceSettings &settings)
      : history_(history), logger_(logger), settings_(settings) {}
  void begin();
  void handle();
  void setNow(uint64_t nowMs) { nowMs_ = nowMs; }
  bool consumeTouchCalibrationRequest();
  bool consumeWifiResetRequest();
  bool consumeDeviceSettingsChanged();
 private:
  WebServer server_{80};
  GlucoseHistory &history_;
  SDLogger &logger_;
  DeviceSettings &settings_;
  uint64_t nowMs_ = 0;
  bool longHistory_ = true;
  uint16_t retentionDays_ = 0;
  bool touchCalibrationRequested_ = false;
  bool wifiResetRequested_ = false;
  bool deviceSettingsChanged_ = false;
  void root();
  void status();
  void history();
  void exportCsv();
  void settings();
  void deviceSettings();
  static bool validDate(const String &date);
  static String datePath(const String &date);
  static String localDate(uint64_t timestampMs);
  void beginChunked(const char *type, const String &filename = "");
  void chunk(const String &text);
};
