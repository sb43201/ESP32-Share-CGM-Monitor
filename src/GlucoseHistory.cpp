#include "GlucoseHistory.h"
#include <math.h>

uint16_t GlucoseHistory::minutesFor(HistoryRange range) {
  return static_cast<uint16_t>(range) * 60;
}

size_t GlucoseHistory::expectedCount(HistoryRange range) {
  return min(MAX_HISTORY_READINGS, static_cast<size_t>(minutesFor(range) / 5));
}

bool GlucoseHistory::covers(HistoryRange range, uint64_t nowMs) const {
  if (!count_ || !nowMs) return false;
  const uint64_t desiredStart = nowMs - static_cast<uint64_t>(minutesFor(range)) * 60000ULL;
  // Allow one normal Dexcom interval of tolerance at the left edge.
  return points_[0].timestampMs <= desiredStart + 5ULL * 60000ULL;
}

bool GlucoseHistory::insertOrUpdate(const GlucoseReading &reading) {
  if (!reading.valid || !reading.timestampMs) return false;
  size_t position = 0;
  while (position < count_ && points_[position].timestampMs < reading.timestampMs) ++position;
  if (position < count_ && points_[position].timestampMs == reading.timestampMs) {
    const bool changed = points_[position].value != reading.value || !points_[position].valid;
    points_[position].value = reading.value;
    points_[position].valid = true;
    return changed;
  }
  if (count_ == MAX_HISTORY_READINGS) {
    if (position == 0) return false;
    memmove(points_, points_ + 1, (count_ - 1) * sizeof(HistoryPoint));
    --count_; --position;
  }
  if (position < count_) {
    memmove(points_ + position + 1, points_ + position,
            (count_ - position) * sizeof(HistoryPoint));
  }
  points_[position].value = static_cast<int16_t>(reading.value);
  points_[position].timestampMs = reading.timestampMs;
  points_[position].valid = true;
  ++count_;
  return true;
}

bool GlucoseHistory::merge(const GlucoseReading *readings, size_t count) {
  if (!readings || !count) return false;
  bool changed = false;
  for (size_t i = 0; i < count; ++i) changed = insertOrUpdate(readings[i]) || changed;
  if (changed) ++generation_;
  Serial.printf("[CGM] History count: %u (oldest-to-newest)\n", count_);
  return changed;
}

void GlucoseHistory::generateMock24Hours(uint64_t nowMs) {
  count_ = 0;
  for (size_t i = 0; i < MAX_HISTORY_READINGS; ++i) {
    if (i >= 105 && i <= 108) continue;  // Deliberate 25-minute gap.
    const float wave = 24.0f * sinf(i * 0.09f) + 11.0f * sinf(i * 0.027f);
    points_[count_].value = static_cast<int16_t>(125 + wave);
    points_[count_].timestampMs =
        nowMs - static_cast<uint64_t>(MAX_HISTORY_READINGS - 1 - i) * 300000ULL;
    points_[count_].valid = true;
    ++count_;
  }
  ++generation_;
  Serial.printf("[MOCK] Generated %u history points with a deliberate gap\n", count_);
}
