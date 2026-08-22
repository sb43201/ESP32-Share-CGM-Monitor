#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "DexcomClient.h"
#include "GlucoseHistory.h"

struct SDLoggerStatus {
  bool mounted = false;
  bool lowSpace = false;
  bool loggingStopped = false;
  String cardType;
  uint64_t capacityBytes = 0;
  uint64_t freeBytes = 0;
  String currentFile;
  uint64_t lastWriteTimestampMs = 0;
  uint32_t loggedThisBoot = 0;
  String lastError;
};

class SDLogger {
 public:
  bool begin();
  bool update();
  bool logReadings(const GlucoseReading *readings, size_t count);
  bool backfill(const HistoryPoint *points, size_t count);
  size_t restoreRecent(GlucoseReading *readings, size_t capacity, uint64_t nowMs);
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }
  const SDLoggerStatus &status() const { return status_; }

 private:
  SPIClass spi_{VSPI};
  SDLoggerStatus status_;
  uint32_t lastMountAttemptMs_ = 0;
  uint64_t lastWrittenTimestampMs_ = 0;
  bool enabled_ = true;

  bool mount();
  void markUnavailable(const String &error);
  bool appendReading(const GlucoseReading &reading);
  bool ensureDirectories(const String &path);
  String pathForTimestamp(uint64_t timestampMs) const;
  static String isoTimestamp(uint64_t timestampMs);
  static String trendText(const String &trend);
  static String cardTypeName(uint8_t type);
  void updateSpace();
  size_t restoreFile(const String &path, GlucoseReading *readings,
                     size_t capacity, size_t count);
};
