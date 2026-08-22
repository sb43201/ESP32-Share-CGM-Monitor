#pragma once
#include <Arduino.h>
#include "DeviceSettings.h"

class WifiProvisioner {
 public:
  explicit WifiProvisioner(DeviceSettings &settings) : settings_(settings) {}
  bool begin();
  String setupSsid() const;
 private:
  DeviceSettings &settings_;
};
