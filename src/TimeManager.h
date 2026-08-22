#pragma once
#include <Arduino.h>

class TimeManager {
 public:
  void begin(const String &timezone, bool clock24Hour);
  void setClock24Hour(bool enabled) { clock24Hour_ = enabled; }
  bool isSynchronized() const;
  uint64_t nowMs() const;
  String clockText() const;
  int64_t readingAgeMinutes(uint64_t timestampMs) const;
 private:
  bool clock24Hour_ = false;
};
