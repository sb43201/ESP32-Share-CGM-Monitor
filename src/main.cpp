#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "Config.h"
#include "DexcomClient.h"
#include "DeviceSettings.h"
#include "DisplayUI.h"
#include "GlucoseHistory.h"
#include "SDLogger.h"
#include "TimeManager.h"
#include "WebDashboard.h"
#include "WifiProvisioner.h"

DisplayUI display;
DexcomClient dexcom;
TimeManager timeManager;
GlucoseHistory glucoseHistory;
SDLogger sdLogger;
DeviceSettings deviceSettings;
WifiProvisioner wifiProvisioner(deviceSettings);
WebDashboard webDashboard(glucoseHistory, sdLogger, deviceSettings);
GlucoseReading latestReading;
// Reused API scratch buffer. History itself uses compact HistoryPoint entries.
GlucoseReading fetchedReadings[MAX_HISTORY_READINGS];
size_t fetchedCount = 0;
AppState appState = AppState::BOOT;
UiScreen uiScreen = UiScreen::DASHBOARD;
HistoryRange selectedRange = HistoryRange::H3;
String stateDetail;
uint64_t lastSuccessfulRequestMs = 0;
uint32_t lastWifiAttempt = 0, lastPoll = 0, lastDisplay = 0, lastHeapLog = 0;
uint32_t lastMdnsAttempt = 0;
bool ntpStarted = false, ntpWasSynced = false;
bool webStarted = false;
bool mdnsStarted = false;
bool graphDirty = true, screenDirty = true;

enum class ScreenOverride : uint8_t { AUTO, FORCE_ON, FORCE_OFF };
ScreenOverride screenOverride = ScreenOverride::AUTO;

void loadHistoryRangePreference() {
  Preferences preferences;
  preferences.begin("dexcomui", false);
  const uint8_t savedHours = preferences.isKey("historyRange")
      ? preferences.getUChar("historyRange", 3) : 3;
  preferences.end();
  if (savedHours == 6) selectedRange = HistoryRange::H6;
  else if (savedHours == 12) selectedRange = HistoryRange::H12;
  else if (savedHours == 24) selectedRange = HistoryRange::H24;
  else selectedRange = HistoryRange::H3;
  Serial.printf("[UI] Restored history range: %uH\n",
                static_cast<unsigned>(selectedRange));
}

void saveHistoryRangePreference() {
  Preferences preferences;
  preferences.begin("dexcomui", false);
  preferences.putUChar("historyRange", static_cast<uint8_t>(selectedRange));
  preferences.end();
}

void startWifi() {
  Serial.println("[WIFI] Connecting using saved credentials..."); WiFi.mode(WIFI_STA); WiFi.reconnect();
  lastWifiAttempt = millis(); appState = AppState::WIFI_CONNECTING;
}

void startMdns() {
  if (WiFi.status() != WL_CONNECTED) return;
  lastMdnsAttempt = millis();
  if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
  const String &hostname = deviceSettings.data().hostname;
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Serial.printf("[MDNS] Dashboard available at http://%s.local\n", hostname.c_str());
  } else {
    Serial.println("[MDNS] Failed to start; will retry");
  }
}

void updateWifi() {
  static bool wasConnected = false;
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    Serial.println("[WIFI] Connected"); Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
    startMdns();
    if (!ntpStarted) { timeManager.begin(deviceSettings.data().timezone, deviceSettings.data().clock24Hour); ntpStarted = true; appState = AppState::TIME_SYNC; }
    lastPoll = 0; screenDirty = true;
    if (!webStarted) { webDashboard.begin(); webStarted = true; }
  } else if (!connected && wasConnected) {
    if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
    Serial.println("[WIFI] Disconnected; retaining glucose and history");
    appState = AppState::NETWORK_ERROR; stateDetail = "Wi-Fi disconnected"; screenDirty = true;
  }
  if (connected && !mdnsStarted && millis() - lastMdnsAttempt >= 5000)
    startMdns();
  if (!connected && millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL_MS) {
    Serial.println("[WIFI] Retrying connection"); WiFi.reconnect(); lastWifiAttempt = millis();
  }
  wasConnected = connected;
}

#if USE_MOCK_DEXCOM_DATA
void initializeMock() {
  glucoseHistory.generateMock24Hours(timeManager.nowMs());
  const HistoryPoint &last = glucoseHistory.points()[glucoseHistory.count() - 1];
  latestReading.value = last.value; latestReading.timestampMs = last.timestampMs;
  latestReading.trend = "Flat"; latestReading.valid = true; appState = AppState::NORMAL;
  graphDirty = true;
}

void advanceMock() {
  static const char *trends[] = {"Flat", "FortyFiveUp", "SingleUp", "Flat",
      "FortyFiveDown", "SingleDown", "Flat", "DoubleUp"};
  static size_t step = 0;
  if (!glucoseHistory.count()) { initializeMock(); return; }
  GlucoseReading simulated;
  simulated.value = 120 + static_cast<int>(18.0f * sinf(step * 0.55f));
  simulated.trend = trends[step % (sizeof(trends) / sizeof(trends[0]))];
  simulated.timestampMs = timeManager.nowMs(); simulated.valid = true;
  glucoseHistory.merge(&simulated, 1); latestReading = simulated;
  ++step; graphDirty = true; appState = AppState::NORMAL;
}
#endif

DexcomResult fetchAndMerge(uint16_t minutes, size_t maxCount) {
  fetchedCount = 0;
  const DexcomResult result = dexcom.getRecentReadings(fetchedReadings, maxCount, fetchedCount, minutes);
  if (result == DexcomResult::OK && fetchedCount) {
    latestReading = fetchedReadings[0];
    if (glucoseHistory.merge(fetchedReadings, fetchedCount)) graphDirty = true;
    sdLogger.logReadings(fetchedReadings, fetchedCount);
    lastSuccessfulRequestMs = timeManager.nowMs(); appState = AppState::NORMAL; stateDetail = "";
  }
  return result;
}

void restoreHistoryFromSD() {
  if (!timeManager.isSynchronized() || !sdLogger.status().mounted) return;
  fetchedCount = sdLogger.restoreRecent(fetchedReadings, MAX_HISTORY_READINGS,
                                        timeManager.nowMs());
  if (fetchedCount) {
    glucoseHistory.merge(fetchedReadings, fetchedCount);
    latestReading = fetchedReadings[fetchedCount - 1];
    graphDirty = true;
  }
}

void handleDexcomFailure(DexcomResult result) {
  stateDetail = dexcom.lastError();
  if (result == DexcomResult::AUTH_FAILED) appState = AppState::AUTH_ERROR;
  else if (result == DexcomResult::EMPTY_DATA) appState = AppState::NO_DATA;
  else appState = AppState::NETWORK_ERROR;
  Serial.printf("[DEXCOM] Error: %s\n", stateDetail.c_str()); screenDirty = true;
}

void pollDexcom() {
  if (WiFi.status() != WL_CONNECTED || !timeManager.isSynchronized()) return;
  appState = dexcom.hasSession() ? AppState::LOADING : AppState::DEXCOM_AUTH;
#if USE_MOCK_DEXCOM_DATA
  advanceMock();
#else
  const bool startup = glucoseHistory.count() == 0;
  const uint16_t minutes = startup ? 180 : 30;
  const size_t count = startup ? 36 : DEXCOM_READING_COUNT;
  const DexcomResult result = fetchAndMerge(minutes, count);
  if (result != DexcomResult::OK) handleDexcomFailure(result);
#endif
  lastPoll = millis(); screenDirty = true;
}

void loadSelectedRangeIfNeeded() {
#if USE_MOCK_DEXCOM_DATA
  graphDirty = true;
#else
  if (glucoseHistory.covers(selectedRange, timeManager.nowMs()) || WiFi.status() != WL_CONNECTED) {
    graphDirty = true; return;
  }
  Serial.printf("[CGM] Loading %u-hour history on demand\n", static_cast<unsigned>(selectedRange));
  const DexcomResult result = fetchAndMerge(GlucoseHistory::minutesFor(selectedRange),
                                             GlucoseHistory::expectedCount(selectedRange));
  if (result != DexcomResult::OK) handleDexcomFailure(result);
#endif
  graphDirty = true;
}

void handleTouch() {
  const TouchAction action = display.updateTouch(uiScreen);
  if (action == TouchAction::NONE) return;
  if (action == TouchAction::SCREEN_OFF) {
    screenOverride = ScreenOverride::FORCE_OFF;
    display.setBacklight(false);
    Serial.println("[DISPLAY] Manual screen-off override");
    return;
  } else if (action == TouchAction::WAKE_SCREEN) {
    screenOverride = ScreenOverride::FORCE_ON;
    display.setBacklight(true);
    Serial.println("[DISPLAY] Manual wake override");
    return;
  } else if (action == TouchAction::SHOW_STATUS) uiScreen = UiScreen::STATUS;
  else if (action == TouchAction::SHOW_DASHBOARD) uiScreen = UiScreen::DASHBOARD;
  else if (action == TouchAction::CALIBRATE_TOUCH) {
    display.setBacklight(true);
    display.calibrateTouch();
    Serial.println("[TOUCH] Calibration requested from system page");
  } else if (action == TouchAction::TOGGLE_CLOCK_FORMAT) {
    auto &settings = deviceSettings.data();
    settings.clock24Hour = !settings.clock24Hour;
    deviceSettings.save();
    timeManager.setClock24Hour(settings.clock24Hour);
    Serial.printf("[TIME] Clock format changed to %s from system page\n",
                  settings.clock24Hour ? "24-hour" : "12-hour");
  } else {
    const HistoryRange previousRange = selectedRange;
    if (action == TouchAction::RANGE_3H) selectedRange = HistoryRange::H3;
    else if (action == TouchAction::RANGE_6H) selectedRange = HistoryRange::H6;
    else if (action == TouchAction::RANGE_12H) selectedRange = HistoryRange::H12;
    else if (action == TouchAction::RANGE_24H) selectedRange = HistoryRange::H24;
    if (selectedRange != previousRange) saveHistoryRangePreference();
    Serial.printf("[TOUCH] History range: %uH\n", static_cast<unsigned>(selectedRange));
    loadSelectedRangeIfNeeded();
  }
  display.invalidate(); screenDirty = true;
}

StatusData makeStatusData(int64_t age) {
  StatusData status;
  status.wifiConnected = WiFi.status() == WL_CONNECTED; status.rssi = status.wifiConnected ? WiFi.RSSI() : 0;
  status.ip = status.wifiConnected ? WiFi.localIP().toString() : "--";
  status.dexcomConnected = appState == AppState::NORMAL || appState == AppState::STALE_DATA;
  status.sessionActive = dexcom.hasSession(); status.ntpSynchronized = timeManager.isSynchronized();
  status.clock24Hour = deviceSettings.data().clock24Hour;
  status.hostname = deviceSettings.data().hostname;
  status.lastSuccessMs = lastSuccessfulRequestMs; status.latestReadingMs = latestReading.timestampMs;
  status.readingAgeMinutes = age; status.historyCount = glucoseHistory.count();
  status.oldestReadingMs = glucoseHistory.oldestTimestampMs(); status.uptimeSeconds = millis() / 1000;
  status.freeHeap = ESP.getFreeHeap(); status.minHeap = ESP.getMinFreeHeap(); status.lastError = stateDetail;
  const SDLoggerStatus &sd = sdLogger.status();
  status.sdMounted = sd.mounted; status.sdLowSpace = sd.lowSpace || sd.loggingStopped;
  status.sdType = sd.cardType; status.sdFreeBytes = sd.freeBytes;
  status.sdCurrentFile = sd.currentFile; status.sdLastWriteMs = sd.lastWriteTimestampMs;
  return status;
}

void setup() {
  Serial.begin(115200); delay(100); Serial.printf("\n[BOOT] ESP32 Share CGM Monitor v%s\n", FIRMWARE_VERSION);
  loadHistoryRangePreference();
  display.begin();
  display.showWelcome(FIRMWARE_VERSION); delay(2000);
  display.showSafetyWarning(); delay(2000);
  sdLogger.begin(); deviceSettings.load();
  Serial.printf("[WIFI] Initial setup: join %s and open http://192.168.4.1\n", wifiProvisioner.setupSsid().c_str());
  display.showWifiSetup(wifiProvisioner.setupSsid());
  wifiProvisioner.begin(); lastWifiAttempt = millis();
  dexcom.setCredentials(deviceSettings.data().dexcomLogin, deviceSettings.data().dexcomPassword);
  display.invalidate(); screenDirty = true;
}

void updateScreenSchedule() {
  const auto &s = deviceSettings.data(); bool off = false;
  if (s.screenScheduleEnabled && timeManager.isSynchronized() && s.screenOffMinute != s.screenOnMinute) {
    time_t stamp=time(nullptr); struct tm local{}; localtime_r(&stamp,&local);
    const uint16_t minute=local.tm_hour*60+local.tm_min;
    off=s.screenOffMinute<s.screenOnMinute ? minute>=s.screenOffMinute&&minute<s.screenOnMinute
                                          : minute>=s.screenOffMinute||minute<s.screenOnMinute;
  }
  if (screenOverride == ScreenOverride::FORCE_ON) off = false;
  else if (screenOverride == ScreenOverride::FORCE_OFF) off = true;
  display.setBacklight(!off);
}

void loop() {
  updateWifi(); handleTouch();
  webDashboard.setNow(timeManager.nowMs()); if (webStarted) webDashboard.handle();
  if (webDashboard.consumeTouchCalibrationRequest()) { display.setBacklight(true); display.calibrateTouch(); display.invalidate(); screenDirty=true; }
  if (webDashboard.consumeDeviceSettingsChanged()) {
    dexcom.setCredentials(deviceSettings.data().dexcomLogin, deviceSettings.data().dexcomPassword);
    lastPoll=0;
    timeManager.begin(deviceSettings.data().timezone, deviceSettings.data().clock24Hour);
    startMdns();
    ntpStarted=true; ntpWasSynced=false; display.invalidate(); screenDirty=true;
  }
  if (webDashboard.consumeWifiResetRequest()) {
    Serial.println("[WIFI] Erasing saved network and restarting provisioning"); delay(250); WiFi.disconnect(true, true); delay(250); ESP.restart();
  }
  updateScreenSchedule();
  if (timeManager.isSynchronized() && !ntpWasSynced) {
    ntpWasSynced = true; Serial.println("[NTP] Time synchronized"); appState = AppState::LOADING; screenDirty = true;
    restoreHistoryFromSD();
  }
  const bool wasSdMounted = sdLogger.status().mounted;
  const bool isSdMounted = sdLogger.update();
  if (!wasSdMounted && isSdMounted && timeManager.isSynchronized()) {
    restoreHistoryFromSD();
    sdLogger.backfill(glucoseHistory.points(), glucoseHistory.count()); screenDirty = true;
  }
  if (WiFi.status() == WL_CONNECTED && timeManager.isSynchronized() &&
      (lastPoll == 0 || millis() - lastPoll >= DEXCOM_POLL_INTERVAL_MS)) pollDexcom();

  const int64_t age = timeManager.readingAgeMinutes(latestReading.timestampMs);
  if (latestReading.valid && appState != AppState::NETWORK_ERROR && appState != AppState::AUTH_ERROR) {
    if (age > 15) appState = AppState::NO_DATA; else if (age >= 8) appState = AppState::STALE_DATA;
    else appState = AppState::NORMAL;
  }
  if (screenDirty || millis() - lastDisplay >= DISPLAY_REFRESH_INTERVAL_MS) {
    if (uiScreen == UiScreen::DASHBOARD) {
      display.renderDashboard(appState, latestReading, age, timeManager.clockText(),
          WiFi.status() == WL_CONNECTED, selectedRange, glucoseHistory,
          timeManager.nowMs(), graphDirty);
      graphDirty = false;
    } else display.renderStatus(makeStatusData(age), timeManager.clockText(), screenDirty);
    lastDisplay = millis(); screenDirty = false;
  }
  if (millis() - lastHeapLog >= HEAP_LOG_INTERVAL_MS) {
    Serial.printf("[SYS] Free heap: %u, min: %u, largest: %u, uptime: %us\n",
        ESP.getFreeHeap(), ESP.getMinFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), millis() / 1000);
    Serial.printf("[CGM] History count: %u / %u\n", glucoseHistory.count(), MAX_HISTORY_READINGS);
    lastHeapLog = millis();
  }
  delay(5);
}
