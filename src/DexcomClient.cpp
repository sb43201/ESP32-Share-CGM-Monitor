#include "DexcomClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "Config.h"

namespace {
constexpr char AUTH_ENDPOINT[] = "General/AuthenticatePublisherAccount";
constexpr char LOGIN_ENDPOINT[] = "General/LoginPublisherAccountById";
constexpr char READ_ENDPOINT[] = "Publisher/ReadPublisherLatestGlucoseValues";
}

String DexcomClient::shortId(const String &id) {
  if (id.length() < 10) return "(invalid)";
  return id.substring(0, 4) + "..." + id.substring(id.length() - 4);
}

DexcomResult DexcomClient::postJson(const char *endpoint, const String &payload,
                                    String &response, int &httpStatus,
                                    bool responseMayContainToken) {
  if (WiFi.status() != WL_CONNECTED) return DexcomResult::NETWORK_ERROR;
  WiFiClientSecure client;
  // Proof-of-concept only. Replace with client.setCACert(...) for production.
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  const String url = String(BASE_URL) + endpoint;
  String safeEndpoint(endpoint);
  const int queryStart = safeEndpoint.indexOf('?');
  if (queryStart >= 0) safeEndpoint.remove(queryStart);
  Serial.printf("[HTTP] POST %s\n", safeEndpoint.c_str());
  if (!http.begin(client, url)) {
    lastError_ = "Unable to initialize HTTPS";
    return DexcomResult::NETWORK_ERROR;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept-Encoding", "application/json");
  httpStatus = http.POST(payload);
  response = http.getString();
  http.end();
  Serial.printf("[HTTP] Status: %d\n", httpStatus);
  Serial.printf("[HTTP] Response length: %u\n", response.length());
  if (httpStatus != 200 && !responseMayContainToken && response.length() < 512) {
    Serial.printf("[HTTP] Error body: %s\n", response.c_str());
  }
  if (httpStatus == 200) return DexcomResult::OK;
  if (httpStatus <= 0) {
    lastError_ = "DNS, TLS, connection, or timeout failure (HTTP " +
                 String(httpStatus) + ")";
    return DexcomResult::NETWORK_ERROR;
  }
  return classifyError(httpStatus, response);
}

DexcomResult DexcomClient::classifyError(int status, const String &body) {
  JsonDocument doc;
  const auto error = deserializeJson(doc, body);
  const String code = error ? "" : String(doc["Code"] | "");
  const String message = error ? "" : String(doc["Message"] | "");
  if (error) {
    Serial.printf("[HTTP] Error JSON parsing: %s\n", error.c_str());
  } else {
    // Dexcom error codes/messages are safe to log; request payloads, credentials,
    // account IDs, and session IDs are deliberately never printed.
    Serial.printf("[DEXCOM] Error code: %s\n", code.length() ? code.c_str() : "(missing)");
    if (message.length()) {
      String safeMessage = message.substring(0, 240);
      if(login_.length()) safeMessage.replace(login_, "[redacted-login]");
      if(password_.length()) safeMessage.replace(password_, "[redacted-password]");
      Serial.printf("[DEXCOM] Error message: %s\n", safeMessage.c_str());
    }
  }
  if (code == "SessionIdNotFound" || code == "SessionNotValid" || status == 401) {
    lastError_ = "Dexcom session invalid";
    return DexcomResult::SESSION_INVALID;
  }
  if (code == "AccountPasswordInvalid" || code == "SSO_AuthenticateMaxAttemptsExceeded" ||
      (code == "SSO_InternalError" && message.indexOf("Cannot Authenticate") >= 0)) {
    lastError_ = "Dexcom publisher authentication failed";
    return DexcomResult::AUTH_FAILED;
  }
  lastError_ = "Dexcom server error (HTTP " + String(status) + ", code " + code + ")";
  return DexcomResult::SERVER_ERROR;
}

bool DexcomClient::authenticate() {
  invalidateSession();
  lastError_ = "";
  if (!hasCredentials()) { lastError_ = "Dexcom credentials are not configured"; return false; }
  Serial.println("[DEXCOM] Authenticating publisher account");
  JsonDocument authDoc;
  authDoc["accountName"] = login_;  // Pass through unchanged: username/email/+phone.
  authDoc["password"] = password_;
  authDoc["applicationId"] = APPLICATION_ID;
  String payload;
  serializeJson(authDoc, payload);
  String response;
  int status = 0;
  DexcomResult result = postJson(AUTH_ENDPOINT, payload, response, status, true);
  payload = "";
  authDoc.clear();
  if (result != DexcomResult::OK) return false;
  DeserializationError jsonError = deserializeJson(authDoc, response);
  Serial.printf("[HTTP] JSON parsing: %s\n", jsonError ? jsonError.c_str() : "OK");
  if (jsonError || !authDoc.is<String>()) {
    lastError_ = "Malformed account authentication response";
    return false;
  }
  accountId_ = authDoc.as<String>();
  if (accountId_.length() < 30 || accountId_.startsWith("00000000")) {
    lastError_ = "Dexcom returned an invalid account ID";
    return false;
  }
  Serial.println("[DEXCOM] Publisher authentication successful");
  return establishSession();
}

bool DexcomClient::establishSession() {
  Serial.println("[DEXCOM] Establishing session");
  JsonDocument doc;
  doc["accountId"] = accountId_;
  doc["password"] = password_;
  doc["applicationId"] = APPLICATION_ID;
  String payload;
  serializeJson(doc, payload);
  String response;
  int status = 0;
  DexcomResult result = postJson(LOGIN_ENDPOINT, payload, response, status, true);
  payload = "";
  doc.clear();
  if (result != DexcomResult::OK) return false;
  DeserializationError jsonError = deserializeJson(doc, response);
  Serial.printf("[HTTP] JSON parsing: %s\n", jsonError ? jsonError.c_str() : "OK");
  if (jsonError || !doc.is<String>()) {
    lastError_ = "Malformed session response";
    return false;
  }
  sessionId_ = doc.as<String>();
  if (sessionId_.length() < 30 || sessionId_.startsWith("00000000")) {
    invalidateSession();
    lastError_ = "Dexcom returned an invalid session ID";
    return false;
  }
  Serial.printf("[DEXCOM] Session established: %s\n", shortId(sessionId_).c_str());
  return true;
}

void DexcomClient::invalidateSession() { sessionId_ = ""; }
void DexcomClient::setCredentials(const String &login, const String &password) {
  if(login_==login&&password_==password)return; login_=login; password_=password;
  accountId_=""; invalidateSession(); lastError_="";
}

DexcomResult DexcomClient::getLatestReading(GlucoseReading &reading) {
  size_t count = 0;
  return ensureSessionAndRequest(&reading, 1, count, DEXCOM_READING_MINUTES);
}

DexcomResult DexcomClient::getRecentReadings(GlucoseReading *readings,
                                             size_t capacity,
                                             size_t &readingCount,
                                             uint16_t minutes) {
  return ensureSessionAndRequest(readings, capacity, readingCount, minutes);
}

DexcomResult DexcomClient::ensureSessionAndRequest(GlucoseReading *readings,
                                                   size_t capacity,
                                                   size_t &readingCount,
                                                   uint16_t minutes) {
  readingCount = 0;
  if(!hasCredentials()){lastError_="Dexcom credentials are not configured";return DexcomResult::AUTH_FAILED;}
  if (!hasSession() && !authenticate()) {
    return lastError_.indexOf("authentication") >= 0 ? DexcomResult::AUTH_FAILED
                                                     : DexcomResult::SERVER_ERROR;
  }
  DexcomResult result = requestReadings(readings, capacity, readingCount, minutes);
  if (result == DexcomResult::SESSION_INVALID) {
    Serial.println("[DEXCOM] Session invalid");
    Serial.println("[DEXCOM] Reauthenticating");
    invalidateSession();
    if (!authenticate()) return DexcomResult::AUTH_FAILED;
    result = requestReadings(readings, capacity, readingCount, minutes);  // One retry.
  }
  return result;
}

DexcomResult DexcomClient::requestReadings(GlucoseReading *readings,
                                           size_t capacity,
                                           size_t &readingCount,
                                           uint16_t minutes) {
  readingCount = 0;
  if (readings == nullptr || capacity == 0) return DexcomResult::MALFORMED_DATA;
  Serial.println("[DEXCOM] Requesting latest glucose");
  String endpoint = String(READ_ENDPOINT) + "?sessionId=" + sessionId_ +
                    "&minutes=" + minutes +
                    "&maxCount=" + capacity;
  String response;
  int status = 0;
  const DexcomResult httpResult = postJson(endpoint.c_str(), "{}", response, status, true);
  if (httpResult != DexcomResult::OK) return httpResult;
  JsonDocument doc;
  const DeserializationError jsonError = deserializeJson(doc, response);
  Serial.printf("[HTTP] JSON parsing: %s\n", jsonError ? jsonError.c_str() : "OK");
  if (jsonError || !doc.is<JsonArray>()) {
    lastError_ = "Malformed glucose JSON";
    return DexcomResult::MALFORMED_DATA;
  }
  JsonArray array = doc.as<JsonArray>();
  Serial.printf("[DEXCOM] Received %u readings\n", array.size());
  if (array.isNull() || array.size() == 0) {
    lastError_ = "No glucose readings returned; verify Share and a follower are configured";
    return DexcomResult::EMPTY_DATA;
  }
  const size_t toCopy = min(capacity, array.size());
  for (size_t i = 0; i < toCopy; ++i) {
    readings[i] = GlucoseReading{};
    const DexcomResult parsed = parseReading(array[i].as<JsonObjectConst>(), readings[i], lastError_);
    if (parsed != DexcomResult::OK) return parsed;
    ++readingCount;
  }
  Serial.printf("[CGM] Value: %d mg/dL\n", readings[0].value);
  Serial.printf("[CGM] Trend: %s\n", readings[0].trend.c_str());
  Serial.printf("[CGM] Timestamp: %llu\n", readings[0].timestampMs);
  return DexcomResult::OK;
}

DexcomResult DexcomClient::parseReading(JsonObjectConst newest,
                                        GlucoseReading &reading,
                                        String &errorText) {
  if (!newest["Value"].is<int>()) {
    errorText = "Glucose response is missing Value";
    return DexcomResult::MALFORMED_DATA;
  }
  reading.value = newest["Value"].as<int>();
  reading.trend = String(newest["Trend"] | "None");
  const String wt = String(newest["WT"] | "");
  const String st = String(newest["ST"] | "");
  const String dt = String(newest["DT"] | "");
  if (!isKnownTrend(reading.trend)) reading.trend = "None";
  const String timestampSource = wt.length() ? wt : (st.length() ? st : dt);
  if (!extractTimestampMs(timestampSource, reading.timestampMs)) {
    errorText = "Malformed Dexcom timestamp";
    return DexcomResult::MALFORMED_DATA;
  }
  reading.valid = reading.value > 0;
  if (!reading.valid) {
    errorText = "Invalid glucose value";
    return DexcomResult::MALFORMED_DATA;
  }
  return DexcomResult::OK;
}

bool DexcomClient::extractTimestampMs(const String &source, uint64_t &timestampMs) {
  int start = source.indexOf("Date(");
  if (start < 0) return false;
  start += 5;
  while (start < static_cast<int>(source.length()) && !isDigit(source[start])) ++start;
  int end = start;
  while (end < static_cast<int>(source.length()) && isDigit(source[end])) ++end;
  if (end - start < 10) return false;
  timestampMs = strtoull(source.substring(start, end).c_str(), nullptr, 10);
  return timestampMs > 0;
}

bool DexcomClient::isKnownTrend(const String &trend) {
  static const char *const trends[] = {"DoubleUp", "SingleUp", "FortyFiveUp", "Flat",
      "FortyFiveDown", "SingleDown", "DoubleDown", "NotComputable",
      "RateOutOfRange", "None"};
  for (const char *known : trends) if (trend == known) return true;
  return false;
}
