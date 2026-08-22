#pragma once
#include <Arduino.h>
#include "Config.h"
#include "DexcomClient.h"

enum class HistoryRange : uint8_t { H3 = 3, H6 = 6, H12 = 12, H24 = 24 };

struct HistoryPoint {
  int16_t value = 0;
  uint64_t timestampMs = 0;
  bool valid = false;
};

class GlucoseHistory {
 public:
  bool merge(const GlucoseReading *readings, size_t count);
  void generateMock24Hours(uint64_t nowMs);
  const HistoryPoint *points() const { return points_; }
  size_t count() const { return count_; }
  uint32_t generation() const { return generation_; }
  uint64_t oldestTimestampMs() const { return count_ ? points_[0].timestampMs : 0; }
  bool covers(HistoryRange range, uint64_t nowMs) const;
  static uint16_t minutesFor(HistoryRange range);
  static size_t expectedCount(HistoryRange range);

 private:
  HistoryPoint points_[MAX_HISTORY_READINGS]{};
  size_t count_ = 0;
  uint32_t generation_ = 0;
  bool insertOrUpdate(const GlucoseReading &reading);
};
