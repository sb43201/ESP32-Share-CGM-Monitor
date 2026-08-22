#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct GlucoseReading {
  int value = 0;
  String trend;
  uint64_t timestampMs = 0;
  bool valid = false;
};

enum class DexcomResult {
  OK, AUTH_FAILED, SESSION_INVALID, NETWORK_ERROR, SERVER_ERROR,
  EMPTY_DATA, MALFORMED_DATA
};

class DexcomClient {
 public:
  bool authenticate();
  DexcomResult getLatestReading(GlucoseReading &reading);
  DexcomResult getRecentReadings(GlucoseReading *readings, size_t capacity,
                                 size_t &readingCount, uint16_t minutes);
  void invalidateSession();
  const String &lastError() const { return lastError_; }
  bool hasSession() const { return sessionId_.length() > 0; }
  void setCredentials(const String &login, const String &password);
  bool hasCredentials() const { return login_.length() && password_.length(); }

 private:
  static constexpr const char *BASE_URL =
      "https://share2.dexcom.com/ShareWebServices/Services/";
  static constexpr const char *APPLICATION_ID =
      "d89443d2-327c-4a6f-89e5-496bbb0317db";

  String accountId_;
  String sessionId_;
  String lastError_;
  String login_;
  String password_;

  DexcomResult postJson(const char *endpoint, const String &payload,
                        String &response, int &httpStatus,
                        bool responseMayContainToken);
  bool establishSession();
  DexcomResult requestReadings(GlucoseReading *readings, size_t capacity,
                               size_t &readingCount, uint16_t minutes);
  DexcomResult ensureSessionAndRequest(GlucoseReading *readings, size_t capacity,
                                      size_t &readingCount, uint16_t minutes);
  static DexcomResult parseReading(JsonObjectConst object, GlucoseReading &reading,
                                   String &errorText);
  DexcomResult classifyError(int httpStatus, const String &body);
  static bool extractTimestampMs(const String &source, uint64_t &timestampMs);
  static bool isKnownTrend(const String &trend);
  static String shortId(const String &id);
};
