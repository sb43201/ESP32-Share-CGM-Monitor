#pragma once
#include <Arduino.h>

#define USE_MOCK_DEXCOM_DATA 0
#define FORCE_TOUCH_CALIBRATION 0
#define FIRMWARE_VERSION "3.1.0"

constexpr uint32_t DEXCOM_POLL_INTERVAL_MS = 60000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint32_t DISPLAY_REFRESH_INTERVAL_MS = 1000;
constexpr uint32_t HEAP_LOG_INTERVAL_MS = 300000;
constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
constexpr uint8_t DEXCOM_READING_COUNT = 6;
constexpr uint16_t DEXCOM_READING_MINUTES = 1440;
constexpr size_t MAX_HISTORY_READINGS = 288;
constexpr int CRITICAL_LOW_REFERENCE = 65;
constexpr int LOW_REFERENCE = 70;
constexpr int HIGH_REFERENCE = 180;
constexpr int VERY_HIGH_REFERENCE = 250;
constexpr int TOUCH_MIN_X = 300;
constexpr int TOUCH_MAX_X = 3800;
constexpr int TOUCH_MIN_Y = 280;
constexpr int TOUCH_MAX_Y = 3850;
constexpr uint8_t SD_CS_PIN = 5;
constexpr uint8_t SD_SCK_PIN = 18;
constexpr uint8_t SD_MISO_PIN = 19;
constexpr uint8_t SD_MOSI_PIN = 23;
constexpr uint32_t SD_RETRY_INTERVAL_MS = 60000;
constexpr uint64_t SD_WARN_FREE_BYTES = 100ULL * 1024ULL * 1024ULL;
constexpr uint64_t SD_STOP_FREE_BYTES = 20ULL * 1024ULL * 1024ULL;

constexpr char TIMEZONE[] = "EST5EDT,M3.2.0,M11.1.0";
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";

enum class AppState {
  BOOT, WIFI_CONNECTING, TIME_SYNC, DEXCOM_AUTH, LOADING, NORMAL,
  STALE_DATA, NETWORK_ERROR, AUTH_ERROR, NO_DATA
};
