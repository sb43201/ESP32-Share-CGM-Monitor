#include "SDLogger.h"
#include "Config.h"
#include <time.h>

bool SDLogger::begin() {
  Serial.println("[SD] Initializing...");
  spi_.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  return mount();
}

bool SDLogger::mount() {
  lastMountAttemptMs_ = millis();
  pinMode(SD_CS_PIN, OUTPUT); digitalWrite(SD_CS_PIN, HIGH);
  if (!SD.begin(SD_CS_PIN, spi_, 10000000)) {
    markUnavailable("Mount failed or no card");
    Serial.println("[SD] No card"); return false;
  }
  const uint8_t type = SD.cardType();
  if (type == CARD_NONE) {
    SD.end(); markUnavailable("No card"); Serial.println("[SD] No card"); return false;
  }
  status_.mounted = true; status_.lastError = ""; status_.cardType = cardTypeName(type);
  status_.capacityBytes = SD.cardSize(); updateSpace();
  Serial.println("[SD] Card detected");
  Serial.printf("[SD] Type: %s\n", status_.cardType.c_str());
  Serial.printf("[SD] Size: %llu MB\n", status_.capacityBytes / 1048576ULL);
  Serial.printf("[SD] Free: %llu MB\n", status_.freeBytes / 1048576ULL);
  return true;
}

bool SDLogger::update() {
  if (status_.mounted) return true;
  if (millis() - lastMountAttemptMs_ < SD_RETRY_INTERVAL_MS) return false;
  Serial.println("[SD] Retrying mount"); return mount();
}

void SDLogger::markUnavailable(const String &error) {
  SD.end();
  status_.mounted = false; status_.lastError = error; status_.currentFile = "";
  status_.loggingStopped = false; status_.lowSpace = false;
}

void SDLogger::updateSpace() {
  if (!status_.mounted) return;
  const uint64_t total = SD.totalBytes(); const uint64_t used = SD.usedBytes();
  if (total) status_.capacityBytes = total;
  status_.freeBytes = total > used ? total - used : 0;
  status_.lowSpace = status_.freeBytes < SD_WARN_FREE_BYTES;
  status_.loggingStopped = status_.freeBytes < SD_STOP_FREE_BYTES;
  if (status_.loggingStopped) status_.lastError = "SD logging stopped: card full";
  else if (status_.lowSpace) status_.lastError = "SD card low space";
}

String SDLogger::cardTypeName(uint8_t type) {
  if (type == CARD_MMC) return "MMC"; if (type == CARD_SD) return "SDSC";
  if (type == CARD_SDHC) return "SDHC"; return "UNKNOWN";
}

String SDLogger::pathForTimestamp(uint64_t timestampMs) const {
  time_t seconds = timestampMs / 1000ULL; struct tm local{}; localtime_r(&seconds, &local);
  char path[48]; strftime(path, sizeof(path), "/dexcom/%Y/%m/%Y-%m-%d.csv", &local);
  return path;
}

String SDLogger::isoTimestamp(uint64_t timestampMs) {
  time_t seconds = timestampMs / 1000ULL; struct tm local{}; localtime_r(&seconds, &local);
  char base[32], zone[8]; strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &local);
  strftime(zone, sizeof(zone), "%z", &local);
  String offset(zone);
  if (offset.length() == 5) offset = offset.substring(0, 3) + ":" + offset.substring(3);
  return String(base) + offset;
}

String SDLogger::trendText(const String &trend) {
  if (trend == "DoubleUp") return "RISING_FAST"; if (trend == "SingleUp") return "RISING";
  if (trend == "FortyFiveUp") return "RISING_SLOWLY"; if (trend == "Flat") return "STEADY";
  if (trend == "FortyFiveDown") return "FALLING_SLOWLY"; if (trend == "SingleDown") return "FALLING";
  if (trend == "DoubleDown") return "FALLING_FAST"; if (trend == "RateOutOfRange") return "TREND_UNAVAILABLE";
  return "NO_TREND";
}

bool SDLogger::ensureDirectories(const String &path) {
  int slash = 1;
  while ((slash = path.indexOf('/', slash)) >= 0) {
    const String directory = path.substring(0, slash);
    if (directory.length() && !SD.exists(directory) && !SD.mkdir(directory)) return false;
    ++slash;
  }
  return true;
}

bool SDLogger::appendReading(const GlucoseReading &reading) {
  if (!enabled_ || !status_.mounted || status_.loggingStopped || !reading.valid || !reading.timestampMs) return false;
  if (reading.timestampMs <= lastWrittenTimestampMs_) return true;
  updateSpace(); if (status_.loggingStopped) return false;
  const String path = pathForTimestamp(reading.timestampMs);
  if (!ensureDirectories(path)) { markUnavailable("Directory creation failed"); return false; }
  const bool newFile = !SD.exists(path);
  File file = SD.open(path, FILE_APPEND);
  if (!file) { markUnavailable("File open failed"); Serial.println("[SD] ERROR opening file"); return false; }
  if (newFile) file.println("timestamp_iso,epoch_ms,glucose_mgdl,trend,trend_text");
  const String line = isoTimestamp(reading.timestampMs) + "," + String(reading.timestampMs) + "," +
      String(reading.value) + "," + reading.trend + "," + trendText(reading.trend);
  const size_t written = file.println(line); file.flush(); file.close();
  if (!written) { markUnavailable("Write failed or card removed"); Serial.println("[SD] Logging paused"); return false; }
  lastWrittenTimestampMs_ = reading.timestampMs; status_.lastWriteTimestampMs = reading.timestampMs;
  status_.currentFile = path; ++status_.loggedThisBoot;
  Serial.printf("[SD] Appended reading: %d @ %s\n", reading.value, isoTimestamp(reading.timestampMs).c_str());
  return true;
}

bool SDLogger::logReadings(const GlucoseReading *readings, size_t count) {
  if (!status_.mounted || !readings) return false;
  bool ok = true;
  for (size_t i = count; i > 0; --i) ok = appendReading(readings[i - 1]) && ok;
  return ok;
}

bool SDLogger::backfill(const HistoryPoint *points, size_t count) {
  if (!status_.mounted || !points) return false;
  size_t pending = 0;
  for (size_t i = 0; i < count; ++i) if (points[i].timestampMs > lastWrittenTimestampMs_) ++pending;
  if (pending) Serial.printf("[SD] Backfilling %u readings\n", pending);
  bool ok = true;
  for (size_t i = 0; i < count; ++i) {
    if (points[i].timestampMs <= lastWrittenTimestampMs_) continue;
    GlucoseReading reading; reading.value = points[i].value; reading.timestampMs = points[i].timestampMs;
    reading.trend = "None"; reading.valid = points[i].valid;
    ok = appendReading(reading) && ok;
  }
  return ok;
}

size_t SDLogger::restoreFile(const String &path, GlucoseReading *readings,
                             size_t capacity, size_t count) {
  // Missing daily files are normal on a new card or a day without readings.
  // Check first so ESP32 VFS does not emit a misleading open() error.
  if (!SD.exists(path)) return count;
  File file = SD.open(path, FILE_READ); if (!file) return count;
  while (file.available()) {
    String line = file.readStringUntil('\n'); line.trim();
    if (!line.length() || line.startsWith("timestamp_iso")) continue;
    const int p1 = line.indexOf(','), p2 = line.indexOf(',', p1 + 1);
    const int p3 = line.indexOf(',', p2 + 1), p4 = line.indexOf(',', p3 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) continue;
    GlucoseReading reading;
    reading.timestampMs = strtoull(line.substring(p1 + 1, p2).c_str(), nullptr, 10);
    reading.value = line.substring(p2 + 1, p3).toInt();
    reading.trend = line.substring(p3 + 1, p4); reading.valid = reading.timestampMs && reading.value > 0;
    if (!reading.valid) continue;
    if (reading.timestampMs > lastWrittenTimestampMs_) lastWrittenTimestampMs_ = reading.timestampMs;
    if (count == capacity) {
      for (size_t i = 1; i < count; ++i) readings[i - 1] = readings[i];
      --count;
    }
    readings[count++] = reading;
  }
  file.close(); return count;
}

size_t SDLogger::restoreRecent(GlucoseReading *readings, size_t capacity, uint64_t nowMs) {
  if (!status_.mounted || !readings || !capacity || !nowMs) return 0;
  size_t count = 0;
  for (int day = 1; day >= 0; --day) {
    const uint64_t timestamp = nowMs - static_cast<uint64_t>(day) * 86400000ULL;
    count = restoreFile(pathForTimestamp(timestamp), readings, capacity, count);
  }
  Serial.printf("[SD] Restored %u recent readings\n", count);
  status_.loggedThisBoot = count;
  return count;
}
