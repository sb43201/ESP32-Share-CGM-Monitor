#pragma once
#include <Arduino.h>

struct DeviceSettingsData {
  String hostname = "esp32-cgm-monitor";
  String timezone = "EST5EDT,M3.2.0,M11.1.0";
  bool clock24Hour = false;
  bool screenScheduleEnabled = false;
  uint16_t screenOffMinute = 0;
  uint16_t screenOnMinute = 7 * 60;
  String dexcomLogin;
  String dexcomPassword;
};

class DeviceSettings {
 public:
  void load();
  void save() const;
  DeviceSettingsData &data() { return data_; }
  const DeviceSettingsData &data() const { return data_; }
  static bool parseClockMinutes(const String &value, uint16_t &result);
  static String formatClockMinutes(uint16_t minutes);
  static String cleanHostname(String value);
  bool hasDexcomCredentials() const { return data_.dexcomLogin.length() && data_.dexcomPassword.length(); }
 private:
  DeviceSettingsData data_;
};
