#include "TimeManager.h"
#include <time.h>
#include "Config.h"

void TimeManager::begin(const String &timezone, bool clock24Hour) {
  clock24Hour_ = clock24Hour;
  configTzTime(timezone.c_str(), NTP_SERVER_1, NTP_SERVER_2);
  Serial.println("[NTP] Synchronization requested");
}

bool TimeManager::isSynchronized() const { return time(nullptr) > 1700000000; }
uint64_t TimeManager::nowMs() const { return static_cast<uint64_t>(time(nullptr)) * 1000ULL; }

String TimeManager::clockText() const {
  if (!isSynchronized()) return "--:--";
  struct tm local{};
  if (!getLocalTime(&local, 20)) return "--:--";
  char buffer[16];
  strftime(buffer, sizeof(buffer), clock24Hour_ ? "%H:%M" : "%I:%M %p", &local);
  return !clock24Hour_ && buffer[0] == '0' ? String(buffer + 1) : String(buffer);
}

int64_t TimeManager::readingAgeMinutes(uint64_t timestampMs) const {
  if (!isSynchronized() || timestampMs == 0) return -1;
  const int64_t delta = static_cast<int64_t>(nowMs()) - static_cast<int64_t>(timestampMs);
  return delta <= 0 ? 0 : delta / 60000;
}
