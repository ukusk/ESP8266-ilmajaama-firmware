/*
 * ESP8266 WEATHER STATION - fully commented version
 * ==========================================================================
 * Hardware: HU-061 weather clock kit = ESP-01S (ESP8266) + 0.96" SSD1306 OLED + button KEY1.
 *
 * WHAT THE DEVICE DOES
 *   - Connects to your WiFi and fetches the weather of a chosen city from OpenWeatherMap.
 *   - Shows the weather + clock on the OLED and on a web page.
 *   - If there is no WiFi/config, it creates its own WiFi network "ESP_AP" with a setup page
 *     where you enter WiFi name, password, city and API key (no re-flashing needed).
 *   - Settings are stored in flash (LittleFS) and survive power loss.
 *
 * LIBRARIES (Arduino Library Manager):
 *   ArduinoJson (v6 or v7), NTPClient (Fabrice Weinberg),
 *   "ESP8266 and ESP32 OLED driver for SSD1306 displays" (ThingPulse v4.x)
 * ARDUINO IDE SETTINGS:
 *   Board: Generic ESP8266 Module, Flash Size: 1MB (FS:64KB OTA:~470KB), Upload Speed 115200
 *
 * REQUIREMENT -> CODE MAP
 *   F1 auto-connect to saved WiFi ........ connectSTA(), staWatchdogTick()
 *   F2 AP "ESP_AP" when no WiFi/config ... startAP()
 *   F3 web UI in AP mode ................. server + dns (captive portal), handleNotFound()
 *   F4 enter WiFi/city/key on the web .... handleConfig(), handleSave()
 *   F5 settings survive restart .......... saveConfig(), loadConfig()
 *   F6 show temp/humidity/pressure/wind .. fetchWeather(), oledTick(), handleRoot()
 *   F7 automatic weather refresh ......... weatherTick()
 *   F8 API errors never crash the device . error branches in fetchWeather()
 *   F9 NTP time sync ..................... ntpTick(), toLocal(), isEuDst()
 *   F10 time of last successful update ... lastOkEpoch
 *   F11 show WiFi + API status ........... oledTick(), handleRoot(), handleApiStatus()
 *   F12 recover after power cut .......... apRetryTick()
 */

// ---------------------------------------------------------------------------
// Libraries
// ---------------------------------------------------------------------------
#include <ESP8266WiFi.h>        // WiFi client (STA) and access point (AP)
#include <ESP8266WebServer.h>   // simple HTTP server on port 80
#include <ESP8266HTTPClient.h>  // HTTP client used to call OpenWeatherMap
#include <ESP8266mDNS.h>        // lets you open http://ilmajaam.local instead of an IP
#include <WiFiClientSecure.h>   // HTTPS (TLS) connection
#include <WiFiUdp.h>            // UDP socket, needed by NTP
#include <DNSServer.h>          // fake DNS server for the captive portal in AP mode
#include <LittleFS.h>           // file system in flash, stores config.json
#include <ArduinoJson.h>        // read/write JSON (config file, API reply, /api/status)
#include <NTPClient.h>          // gets the current time from an internet time server
#include <Wire.h>               // I2C bus (the OLED uses it)
#include <SSD1306Wire.h>        // OLED display driver

// ---------------------------------------------------------------------------
// Constants and settings (change them here, not deep in the code)
// ---------------------------------------------------------------------------
static const char*    AP_SSID        = "ESP_AP";        // name of the setup WiFi network
static const char*    AP_PASS        = "ilmajaam123";   // its password (min 8 chars; "" = open network)
static const char*    ADMIN_PASS     = "";              // if not empty, setup pages ask for it (user: admin)
static const char*    HOST_NAME      = "ilmajaam";      // http://ilmajaam.local
static const char*    CONFIG_FILE    = "/config.json";  // saved settings
static const char*    CONFIG_TMP     = "/config.tmp";   // temp file used while saving

static const uint32_t WIFI_TIMEOUT_MS   = 15000;   // how long to wait for WiFi at boot
static const uint32_t AP_RETRY_MS       = 120000;  // in AP mode, retry the home WiFi every 2 min
static const uint32_t WIFI_LOST_RECONN  = 30000;   // WiFi lost for 30 s -> try reconnecting
static const uint32_t WIFI_LOST_AP      = 180000;  // WiFi lost for 3 min -> start the AP
static const uint32_t API_RETRY_MS      = 60000;   // after an API error retry in 60 s
static const uint32_t API_TIMEOUT_MS    = 8000;    // give up waiting for the API after 8 s
static const uint32_t NTP_TRY_MS        = 15000;   // minimum gap between NTP requests
static const uint32_t RESET_HOLD_MS     = 5000;    // hold KEY1 this long to erase settings

// HU-061 board pins. The module's blue LED shares GPIO2 with OLED SCL, so no LED is used.
static const uint8_t  BUTTON_PIN        = 3;       // KEY1 is on RXD (GPIO3), pressed = GND
static const uint8_t  OLED_SDA          = 0;       // OLED data line  (IO0)
static const uint8_t  OLED_SCL          = 2;       // OLED clock line (IO2)
static const uint8_t  OLED_ADDR         = 0x3C;    // I2C address of the SSD1306
static const bool     OLED_FLIP         = false;   // default screen orientation (changeable on the settings page)

static const char*    NTP_SERVER     = "pool.ntp.org";  // time server

// ---------------------------------------------------------------------------
// Global data
// ---------------------------------------------------------------------------

// Everything the user can configure (stored in /config.json)
struct Config {
  String   ssid;                                   // home WiFi name
  String   pass;                                   // home WiFi password
  String   city        = "Tallinn";                // city for the weather
  String   apiKey      = "f363bc65748e16da1f8af1f1714983a6";  // OpenWeatherMap key
  uint16_t intervalMin = 10;     // minutes between weather updates
  int8_t   tzHours     = 2;      // time zone, hours from UTC (Estonia = 2)
  bool     dstAuto     = true;   // add +1 h automatically during EU summer time
  bool     flip        = OLED_FLIP;  // rotate the OLED 180 degrees
};

// The latest weather received from the API
struct Weather {
  float  temp      = 0;          // temperature, deg C
  int    humidity  = 0;          // %
  int    pressure  = 0;          // hPa
  float  wind      = 0;          // m/s
  String desc;                   // description, e.g. "light rain"
  String place;                  // city name as returned by the API
  bool   valid     = false;      // true once at least one update succeeded
};

Config  cfg;                     // current settings
Weather weather;                 // current weather

ESP8266WebServer server(80);                                   // web server
DNSServer        dns;                                          // captive portal DNS
WiFiUDP          ntpUDP;                                       // UDP socket for NTP
NTPClient        timeClient(ntpUDP, NTP_SERVER, 0, 3600000);   // UTC time, re-sync every 1 h
SSD1306Wire      display(OLED_ADDR, OLED_SDA, OLED_SCL);       // the OLED

bool     apMode          = false;   // true while the setup access point is running
bool     apOk            = false;   // softAP() succeeded (shown on the OLED for diagnostics)
uint32_t lastStaTry      = 0;       // when we last tried the home WiFi while in AP mode
uint32_t wifiLostSince   = 0;       // when the WiFi dropped (0 = connection is fine)
uint32_t lastNtpTry      = 0;       // when we last asked the NTP server
uint32_t lastApiAttempt  = 0;       // when we last called the weather API
bool     apiAttempted    = false;   // has an API call been made at all yet
bool     lastApiOk       = false;   // did the last API call succeed
String   apiMessage      = "Pole veel päritud";  // human readable API status ("Not yet fetched")
int      apiHttpCode     = 0;       // last HTTP status code (200, 401, 404 ...)
uint32_t apiOkCount      = 0;       // number of successful API calls
uint32_t apiFailCount    = 0;       // number of failed API calls
time_t   lastOkEpoch     = 0;       // time of last success (UTC seconds), 0 = unknown  (F10)
uint32_t restartAt       = 0;       // if not 0: reboot when millis() reaches this value
uint32_t buttonDownSince = 0;       // when KEY1 was pressed down (0 = not pressed)

// ---------------------------------------------------------------------------
// CSS for the web pages, stored in flash (PROGMEM) so it does not use RAM
// ---------------------------------------------------------------------------
const char STYLE[] PROGMEM =
  "<style>"
  "body{font-family:system-ui,Arial,sans-serif;background:#eef2f6;margin:0;padding:12px;color:#1c2833}"
  ".w{max-width:520px;margin:auto}"
  ".c{background:#fff;border-radius:12px;padding:16px;margin-bottom:12px;box-shadow:0 1px 4px #0002}"
  "h1{font-size:1.4em;margin:4px 0 12px}h2{font-size:1.05em;margin:0 0 8px;color:#456}"
  ".t{font-size:2.6em;font-weight:600}"
  "table{width:100%;border-collapse:collapse}td{padding:4px 0;border-bottom:1px solid #eee}"
  "td:last-child{text-align:right;font-weight:500}"
  "label{display:block;margin-top:10px;font-size:.9em;color:#456}"
  "input[type=text],input[type=password],input[type=number]{width:100%;box-sizing:border-box;padding:9px;border:1px solid #bbb;border-radius:6px;font-size:1em}"
  "button,.b{display:inline-block;margin-top:14px;padding:10px 16px;border:0;border-radius:6px;background:#2874a6;color:#fff;font-size:1em;text-decoration:none;cursor:pointer}"
  ".r{background:#c0392b}.ok{color:#1e8449}.er{color:#c0392b}.s{font-size:.8em;color:#789}"
  "</style>";

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

// Replaces & < > " ' with HTML entities so user text (SSID, city, API message)
// cannot inject HTML/JavaScript into the page (XSS protection).
String htmlEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&':  out += F("&amp;");  break;
      case '<':  out += F("&lt;");   break;
      case '>':  out += F("&gt;");   break;
      case '"':  out += F("&quot;"); break;
      case '\'': out += F("&#39;");  break;
      default:   out += c;
    }
  }
  return out;
}

// URL-encodes text (spaces, umlauts, commas become %XX) so a city like "Tartu,EE"
// or "Pärnu" can be placed safely inside the API URL.
String urlEncode(const String &in) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;  // safe chars as-is
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }                      // everything else -> %XX
  }
  return out;
}

// --- Date math for summer time (does not depend on the timegm() library function) ---

// Converts a calendar date to "days since 1970-01-01".
static int64_t daysFromCivil(int y, int m, int d) {
  y -= (m <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int mp  = (m > 2) ? m - 3 : m + 9;
  int doy = (153 * mp + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + doe - 719468;
}

// Returns the UTC time (seconds) of 01:00 UTC on the last Sunday of the given month.
// EU summer time starts/ends at exactly that moment (March and October).
static int64_t lastSundayUtc(int year, int month) {
  int64_t last = daysFromCivil(year, month, 31);      // March and October both have 31 days
  int wd = (int)((last + 4) % 7);                     // weekday, 0 = Sunday (1970-01-01 was a Thursday)
  return (last - wd) * 86400LL + 3600;                // step back to Sunday, add 1 hour
}

// True if the given UTC time falls inside EU summer time.
bool isEuDst(time_t utc) {
  time_t t = utc;
  struct tm *tmv = gmtime(&t);
  int year = tmv->tm_year + 1900;
  int64_t u = (int64_t)utc;
  return u >= lastSundayUtc(year, 3) && u < lastSundayUtc(year, 10);
}

// UTC time -> local time (time zone offset + optional summer time).
time_t toLocal(time_t utc) {
  long off = (long)cfg.tzHours * 3600L;
  if (cfg.dstAuto && isEuDst(utc)) off += 3600;
  return utc + off;
}

// Formats a UTC timestamp as local "YYYY-MM-DD HH:MM:SS" (used on the web page).
String fmtDateTime(time_t utc) {
  if (utc == 0) return F("teadmata");   // "unknown"
  time_t l = toLocal(utc);
  struct tm *t = gmtime(&l);
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

// Formats a UTC timestamp as local "HH:MM" (used on the OLED clock).
String fmtTime(time_t utc) {
  if (utc == 0) return F("--:--");
  time_t l = toLocal(utc);
  struct tm *t = gmtime(&l);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
  return String(buf);
}

// Time since boot, formatted "Nd HH:MM:SS".
String fmtUptime() {
  uint32_t s = millis() / 1000;
  char buf[24];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
           (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24),
           (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  return String(buf);
}

// True when connected to the home WiFi as a client.
bool staConnected() { return WiFi.status() == WL_CONNECTED; }

// ---------------------------------------------------------------------------
// F5: load and save settings (LittleFS, /config.json)
// ---------------------------------------------------------------------------

// Reads /config.json into 'cfg'. Returns false if there is no file or it is broken.
bool loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println(F("[CFG] Seadistusfaili ei ole"));          // no config file
    return false;
  }
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    Serial.println(F("[CFG] Faili avamine ebaõnnestus"));      // could not open
    return false;
  }
#if ARDUINOJSON_VERSION_MAJOR >= 7        // ArduinoJson v7 and v6 use different document types
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.print(F("[CFG] JSON viga: "));                      // JSON error
    Serial.println(err.c_str());
    return false;
  }
  // "x | default" = use default if the key is missing
  cfg.ssid        = doc["ssid"]     | "";
  cfg.pass        = doc["pass"]     | "";
  cfg.city        = doc["city"]     | "Tallinn";
  cfg.apiKey      = doc["apikey"]   | "";
  cfg.intervalMin = doc["interval"] | 10;
  cfg.tzHours     = doc["tz"]       | 2;
  cfg.dstAuto     = doc["dst"]      | true;
  cfg.flip        = doc["flip"]     | OLED_FLIP;
  if (cfg.intervalMin < 2) cfg.intervalMin = 2;                // enforce minimum 2 minutes
  Serial.println(F("[CFG] Seadistus loetud"));
  return true;
}

// Saves 'cfg' to flash. Writes a temp file first and then renames it, so a power cut
// in the middle of writing can never destroy the existing good config file.
bool saveConfig() {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  doc["ssid"]     = cfg.ssid;
  doc["pass"]     = cfg.pass;
  doc["city"]     = cfg.city;
  doc["apikey"]   = cfg.apiKey;
  doc["interval"] = cfg.intervalMin;
  doc["tz"]       = cfg.tzHours;
  doc["dst"]      = cfg.dstAuto;
  doc["flip"]     = cfg.flip;

  File f = LittleFS.open(CONFIG_TMP, "w");                     // 1) write temp file
  if (!f) {
    Serial.println(F("[CFG] Ajutise faili loomine ebaõnnestus"));
    return false;
  }
  size_t n = serializeJson(doc, f);
  f.close();
  if (n == 0) {                                                // nothing written -> discard
    LittleFS.remove(CONFIG_TMP);
    return false;
  }
  LittleFS.remove(CONFIG_FILE);                                // 2) delete old file
  if (!LittleFS.rename(CONFIG_TMP, CONFIG_FILE)) {             // 3) temp -> real name
    Serial.println(F("[CFG] Ümbernimetamine ebaõnnestus"));
    return false;
  }
  Serial.println(F("[CFG] Seadistus salvestatud"));
  return true;
}

// ---------------------------------------------------------------------------
// F1, F2, F12: WiFi client (STA) and access point (AP)
// ---------------------------------------------------------------------------

// F1: connect to the saved WiFi, wait up to timeoutMs. Returns true on success.
bool connectSTA(uint32_t timeoutMs) {
  if (cfg.ssid.length() == 0) return false;                    // nothing configured
  Serial.printf("[WiFi] Ühendun: %s ", cfg.ssid.c_str());
  WiFi.mode(WIFI_STA);                                         // client mode only
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t start = millis();
  while (!staConnected() && millis() - start < timeoutMs) {
    delay(250);
    yield();                                                   // keep the ESP8266 watchdog happy
    Serial.print('.');
  }
  Serial.println();
  if (staConnected()) {
    Serial.print(F("[WiFi] Ühendatud, IP: "));
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println(F("[WiFi] Ühendamine ebaõnnestus"));
  return false;
}

// F2 + F3: start the "ESP_AP" setup network and the captive portal.
void startAP() {
  WiFi.mode(WIFI_AP_STA);   // AP + client at once, so it can still try the home WiFi in the background
  apOk = (strlen(AP_PASS) >= 8) ? WiFi.softAP(AP_SSID, AP_PASS) : WiFi.softAP(AP_SSID);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());   // captive portal: every domain name -> 192.168.4.1
  apMode = true;
  lastStaTry = millis();
  Serial.printf("[AP] %s, SSID=%s, IP=%s\n", apOk ? "Käivitatud" : "VIGA", AP_SSID,
                WiFi.softAPIP().toString().c_str());
}

// Stops the access point and goes back to client-only mode.
void stopAP() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apMode = false;
  Serial.println(F("[AP] Peatatud, töötan STA-režiimis"));
}

// Called each time the home WiFi connection is established.
void onWifiConnected() {
  wifiLostSince = 0;
  if (MDNS.begin(HOST_NAME)) MDNS.addService("http", "tcp", 80);   // announce ilmajaam.local
  timeClient.begin();                                              // start the NTP client
  lastNtpTry = 0;                                                  // allow an NTP request right away
}

// F12: while in AP mode, try the home WiFi again every 2 minutes
// (e.g. after a power cut the router may boot slower than the ESP).
void apRetryTick() {
  if (cfg.ssid.length() == 0) return;                          // no WiFi saved -> nothing to retry
  if (millis() - lastStaTry < AP_RETRY_MS) return;             // not time yet
  lastStaTry = millis();
  if (WiFi.softAPgetStationNum() > 0) return;                  // someone is on the setup page - don't disturb

  Serial.println(F("[WiFi] AP-režiimis: proovin põhivõrku"));
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t st = millis();
  while (!staConnected() && millis() - st < 12000) {           // wait up to 12 s
    delay(250);
    yield();
  }
  if (staConnected()) {                                        // success: leave AP mode
    stopAP();
    if (!staConnected()) connectSTA(10000);                    // mode change may drop the link; reconnect
    if (staConnected()) onWifiConnected();
  } else {
    WiFi.disconnect();                                         // failed: stop trying until next time
  }
}

// F1: while in normal mode, watch for the WiFi dropping and recover.
//   lost >  30 s -> force a reconnect
//   lost > 180 s -> start the AP so the device stays reachable
void staWatchdogTick() {
  if (staConnected()) {
    if (wifiLostSince != 0) {
      Serial.println(F("[WiFi] Ühendus taastus"));             // connection restored
      wifiLostSince = 0;
    }
    return;
  }
  if (wifiLostSince == 0) {                                    // first moment of the outage
    wifiLostSince = millis();
    if (wifiLostSince == 0) wifiLostSince = 1;                 // 0 means "ok", so never store 0
    Serial.println(F("[WiFi] Ühendus katkes"));
    return;
  }
  uint32_t lost = millis() - wifiLostSince;                    // how long it has been down
  static bool reconnTried = false;                             // reconnect only once per outage
  if (lost > WIFI_LOST_RECONN && !reconnTried) {
    reconnTried = true;
    WiFi.disconnect();
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  }
  if (staConnected()) reconnTried = false;
  if (lost > WIFI_LOST_AP) {
    reconnTried = false;
    startAP();   // device stays reachable; apRetryTick() keeps trying the home WiFi
  }
}

// ---------------------------------------------------------------------------
// F9: NTP (internet time)
// ---------------------------------------------------------------------------

// Asks the NTP server for the time, but not more often than every 15 s.
void ntpTick() {
  if (!staConnected()) return;                                 // needs internet
  if (lastNtpTry != 0 && millis() - lastNtpTry < NTP_TRY_MS) return;
  lastNtpTry = millis();
  timeClient.update();   // only sends a request when the 1 h interval is up or time is not yet set
}

// Current UTC time in seconds, or 0 if the time has not been synchronised yet.
time_t nowUtc() {
  return timeClient.isTimeSet() ? (time_t)timeClient.getEpochTime() : 0;
}

// ---------------------------------------------------------------------------
// F6, F7, F8, F10: OpenWeatherMap
// ---------------------------------------------------------------------------

// Calls the API once and stores the result in 'weather'. Every failure path sets a readable
// message in apiMessage and returns - the device never crashes because of the API (F8).
void fetchWeather() {
  apiAttempted   = true;
  lastApiAttempt = millis();
  lastApiOk      = false;                                      // assume failure until proven otherwise

  if (cfg.apiKey.length() == 0 || cfg.city.length() == 0) {    // settings incomplete
    apiMessage = F("API võti või linn puudub");
    apiHttpCode = 0;
    return;
  }
  if (ESP.getFreeHeap() < 14000) {                             // TLS needs ~14 KB RAM, avoid crashing
    apiMessage = F("Liiga vähe vaba mälu");
    apiFailCount++;
    return;
  }

  // Build the request URL (city and key are URL-encoded)
  String url = F("https://api.openweathermap.org/data/2.5/weather?q=");
  url += urlEncode(cfg.city);
  url += F("&appid=");
  url += urlEncode(cfg.apiKey);
  url += F("&units=metric&lang=et");                           // metric units (deg C, m/s)

  // setInsecure(): the server certificate is NOT verified (ESP8266 has little memory).
  // The traffic is encrypted, but the server's identity is not checked.
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);                            // small TLS buffers to save RAM

  HTTPClient http;
  http.setTimeout(API_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    apiMessage = F("HTTP-ühenduse loomine ebaõnnestus");
    apiFailCount++;
    return;
  }

  int code = http.GET();                                       // send the request
  apiHttpCode = code;

  if (code == HTTP_CODE_OK) {                                  // 200: parse the weather JSON
    String body = http.getString();
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(2048);
#endif
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      apiMessage = String(F("JSON viga: ")) + err.c_str();
      apiFailCount++;
    } else if (!doc["main"]["temp"].is<float>()) {             // reply without weather data
      apiMessage = F("Vastus ei sisalda ilmaandmeid");
      apiFailCount++;
    } else {                                                   // success: copy values out
      weather.temp     = doc["main"]["temp"].as<float>();
      weather.humidity = doc["main"]["humidity"] | 0;
      weather.pressure = doc["main"]["pressure"] | 0;
      weather.wind     = doc["wind"]["speed"]    | 0.0f;
      weather.desc     = String((const char *)(doc["weather"][0]["description"] | ""));
      weather.place    = String((const char *)(doc["name"] | ""));
      weather.valid    = true;
      lastApiOk        = true;
      apiOkCount++;
      lastOkEpoch      = nowUtc();                             // F10: remember the update time
      apiMessage       = F("OK");
      Serial.printf("[API] OK: %.1f C, %d%%, %d hPa, %.1f m/s, %s\n",
                    weather.temp, weather.humidity, weather.pressure, weather.wind,
                    weather.desc.c_str());
    }
  } else if (code > 0) {                                       // the server answered with an error code
    // OpenWeatherMap puts the reason in JSON: {"cod":401,"message":"..."}
    String body = http.getString();
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(512);
#endif
    String msg;
    if (!deserializeJson(doc, body)) msg = String((const char *)(doc["message"] | ""));
    if (code == 401)      apiMessage = F("Vigane API võti (401)");           // bad/inactive key
    else if (code == 404) apiMessage = F("Linna ei leitud (404)");           // city not found
    else if (code == 429) apiMessage = F("Päringute limiit ületatud (429)"); // rate limit
    else                  apiMessage = String(F("HTTP ")) + code + (msg.length() ? " - " + msg : "");
    apiFailCount++;
  } else {                                                     // code <= 0: no connection / timeout
    apiMessage = String(F("Ühenduse viga: ")) + HTTPClient::errorToString(code);
    apiFailCount++;
  }
  http.end();                                                  // free the connection
  if (!lastApiOk) Serial.println("[API] Viga: " + apiMessage);
}

// F7: decides when to call the API.
//   after success: wait the configured interval (2-1440 min)
//   after failure: retry in 60 s
void weatherTick() {
  if (!staConnected()) return;
  uint32_t interval = lastApiOk ? (uint32_t)cfg.intervalMin * 60000UL : API_RETRY_MS;
  if (!apiAttempted || millis() - lastApiAttempt >= interval) fetchWeather();
}

// ---------------------------------------------------------------------------
// F3, F4, F11: web server
// ---------------------------------------------------------------------------

// If ADMIN_PASS is set, asks for HTTP basic login (user "admin"). Returns true if allowed.
bool authOk() {
  if (strlen(ADMIN_PASS) == 0) return true;                    // no password configured
  if (server.authenticate("admin", ADMIN_PASS)) return true;
  server.requestAuthentication();                              // pops up the browser login dialog
  return false;
}

// Starts an HTML response (head + style). Sent in chunks so the whole page is not held in RAM.
// refreshSec > 0 adds an auto-refresh of the page.
void sendHead(const char *title, int refreshSec = 0) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);             // chunked transfer
  server.send(200, "text/html; charset=utf-8", "");
  String h = F("<!DOCTYPE html><html lang='et'><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  if (refreshSec > 0) { h += F("<meta http-equiv='refresh' content='"); h += refreshSec; h += F("'>"); }
  h += F("<title>"); h += title; h += F("</title>");
  server.sendContent(h);
  server.sendContent_P(STYLE);                                 // CSS straight from flash
  server.sendContent(F("</head><body><div class='w'>"));
}

// Ends the HTML response.
void sendFoot() {
  server.sendContent(F("</div></body></html>"));
  server.sendContent("");                                      // empty chunk = end of response
}

// One table row "<tr><td>key</td><td>value</td></tr>".
String row(const char *k, const String &v) {
  return String(F("<tr><td>")) + k + F("</td><td>") + v + F("</td></tr>");
}

// Home page "/": weather + status (F6, F10, F11). Refreshes itself every 30 s.
void handleRoot() {
  if (apMode && !staConnected()) {                             // first-time setup: go straight to the form
    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "");
    return;
  }
  sendHead("Ilmajaam", 30);
  String s;
  s.reserve(1500);
  s += F("<h1>&#127782; Ilmajaam</h1><div class='c'><h2>");
  s += htmlEscape(weather.valid && weather.place.length() ? weather.place : cfg.city);
  s += F("</h2>");
  if (weather.valid) {                                         // weather card
    s += F("<div class='t'>"); s += String(weather.temp, 1); s += F(" &deg;C</div><div>");
    s += htmlEscape(weather.desc);
    s += F("</div><table>");
    s += row("Õhuniiskus", String(weather.humidity) + " %");
    s += row("Õhurõhk",    String(weather.pressure) + " hPa");
    s += row("Tuule kiirus", String(weather.wind, 1) + " m/s");
    s += row("Viimati uuendatud", fmtDateTime(lastOkEpoch));   // F10
    s += F("</table>");
  } else {
    s += F("<p>Ilmaandmeid pole veel saadud.</p>");            // no data yet
  }
  s += F("</div><div class='c'><h2>Olek</h2><table>");         // status card
  s += row("WiFi", staConnected()
      ? String("<span class='ok'>ühendatud</span> (") + htmlEscape(WiFi.SSID()) + ", " + WiFi.RSSI() + " dBm)"
      : String("<span class='er'>ühendamata</span>"));
  s += row("IP", staConnected() ? WiFi.localIP().toString() : String("-"));
  if (apMode) s += row("AP-režiim", String("aktiivne (") + AP_SSID + ")");
  s += row("API olek", lastApiOk ? String("<span class='ok'>OK</span>")
                                 : String("<span class='er'>") + htmlEscape(apiMessage) + "</span>");
  s += row("API päringud", String(apiOkCount) + " õnnestus / " + apiFailCount + " ebaõnnestus");
  s += row("Kellaaeg", fmtDateTime(nowUtc()));
  s += row("Töötanud", fmtUptime());
  s += row("Vaba mälu", String(ESP.getFreeHeap()) + " B");
  s += F("</table></div><a class='b' href='/config'>Seadistused</a>");
  s += F("<p class='s'>Leht uueneb automaatselt iga 30 s.</p>");
  server.sendContent(s);
  sendFoot();
}

// Settings form "/config" (F4).
void handleConfig() {
  if (!authOk()) return;
  sendHead("Seadistused");
  String s;
  s.reserve(2500);
  s += F("<h1>&#9881; Seadistused</h1><form class='c' method='POST' action='/save'>");

  // SSID field with a drop-down list of the WiFi networks found nearby (scan takes ~2 s)
  s += F("<label>WiFi võrgu nimi (SSID)</label><input type='text' name='ssid' list='nets' maxlength='32' value='");
  s += htmlEscape(cfg.ssid);
  s += F("'><datalist id='nets'>");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 15; i++) {
    s += F("<option value='"); s += htmlEscape(WiFi.SSID(i)); s += F("'>");
  }
  WiFi.scanDelete();                                           // free the scan results
  s += F("</datalist>");

  // The saved password and API key are never shown; an empty field means "keep current"
  s += F("<label>WiFi parool</label><input type='password' name='pass' maxlength='63' placeholder='");
  s += cfg.pass.length() ? F("(jäta tühjaks, et säilitada praegune)") : F("(avatud võrgu korral tühi)");
  s += F("'>");

  s += F("<label>Linn (nt Tallinn või Tartu,EE, vaata API docsi)</label><input type='text' name='city' maxlength='60' value='");
  s += htmlEscape(cfg.city);
  s += F("'>");

  s += F("<label>OpenWeatherMap API võti</label><input type='password' name='apikey' maxlength='64' placeholder='");
  s += cfg.apiKey.length() ? F("(jäta tühjaks, et säilitada praegune)") : F("32-märgiline võti");
  s += F("'>");

  s += F("<label>Uuendusintervall (minutit, 2-1440)</label><input type='number' name='interval' min='2' max='1440' value='");
  s += cfg.intervalMin;
  s += F("'>");

  s += F("<label>Ajavöönd (tunde UTC-st, Eesti = 2)</label><input type='number' name='tz' min='-12' max='14' value='");
  s += cfg.tzHours;
  s += F("'>");

  s += F("<label><input type='checkbox' name='dst' value='1'");
  if (cfg.dstAuto) s += F(" checked");
  s += F("> Arvesta automaatselt suveaega (EL)</label>");

  s += F("<label><input type='checkbox' name='flip' value='1'");
  if (cfg.flip) s += F(" checked");
  s += F("> Pööra ekraan 180° (kui tekst on tagurpidi)</label>");

  s += F("<button type='submit'>Salvesta ja taaskäivita</button></form>");
  // Second form: factory reset button with a confirmation popup
  s += F("<form class='c' method='POST' action='/reset' onsubmit=\"return confirm('Kustutada kõik seadistused?')\">"
         "<h2>Tehaseseaded</h2><button class='r' type='submit'>Kustuta seadistused</button></form>"
         "<a class='b' href='/'>&larr; Tagasi</a>");
  server.sendContent(s);
  sendFoot();
}

// Receives the form (POST /save): validates, saves, reboots (F4, F5).
void handleSave() {
  if (!authOk()) return;
  Config n = cfg;                                              // work on a copy until all is valid

  String ssid = server.arg("ssid"); ssid.trim();
  String pass = server.arg("pass");
  String city = server.arg("city"); city.trim();
  String key  = server.arg("apikey"); key.trim();

  // Reject empty / too long values (trust boundary: anything can be POSTed here)
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63 ||
      city.length() == 0 || city.length() > 60) {
    server.send(400, "text/plain; charset=utf-8", "Vigased andmed: kontrolli SSID-d ja linna.");
    return;
  }
  if (ssid != cfg.ssid) n.pass = pass;       // new network -> use the password from the form (may be empty)
  else if (pass.length() > 0) n.pass = pass; // same network -> empty field keeps the old password
  n.ssid = ssid;
  n.city = city;
  if (key.length() > 0) n.apiKey = key;      // empty key field keeps the old key

  long iv = server.arg("interval").toInt();
  long tz = server.arg("tz").toInt();
  n.intervalMin = (uint16_t)constrain(iv, 2L, 1440L);          // clamp to the allowed range
  n.tzHours     = (int8_t)constrain(tz, -12L, 14L);
  n.flip        = server.hasArg("flip");
  n.dstAuto     = server.hasArg("dst");                      // an unticked checkbox is not sent at all

  cfg = n;                                                     // everything valid -> accept
  bool ok = saveConfig();

  sendHead(ok ? "Salvestatud" : "Viga");
  if (ok) {
    server.sendContent(F("<div class='c'><h1>&#10004; Salvestatud</h1>"
                         "<p>Seade taaskäivitub ja ühendub võrku. Kui ühendus õnnestub, "
                         "leiad lehe aadressilt <b>http://ilmajaam.local</b> "
                         "või IP-aadressilt, mis kuvatakse seadme ekraanil.</p></div>"));
    restartAt = millis() + 1500;                               // reboot in 1.5 s, after the page is delivered
  } else {
    server.sendContent(F("<div class='c'><h1 class='er'>Salvestamine ebaõnnestus</h1>"
                         "<p>LittleFS-i kirjutamine ei õnnestunud. Kontrolli, et Flash Size valikus "
                         "on failisüsteem (nt 1MB FS:64KB).</p></div><a class='b' href='/config'>Tagasi</a>"));
  }
  sendFoot();
}

// Factory reset from the web (POST /reset): deletes the config and reboots into AP mode.
void handleReset() {
  if (!authOk()) return;
  LittleFS.remove(CONFIG_FILE);
  sendHead("Lähtestatud");
  server.sendContent(F("<div class='c'><h1>Seadistused kustutatud</h1>"
                       "<p>Seade taaskäivitub ja avab võrgu <b>ESP_AP</b>.</p></div>"));
  sendFoot();
  restartAt = millis() + 1500;
}

// /api/status: all data as JSON (F11) - handy for home automation.
void handleApiStatus() {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  doc["wifi_connected"] = staConnected();
  doc["ap_mode"]        = apMode;
  doc["rssi"]           = staConnected() ? WiFi.RSSI() : 0;    // WiFi signal strength, dBm
  doc["api_ok"]         = lastApiOk;
  doc["api_message"]    = apiMessage;
  doc["api_http_code"]  = apiHttpCode;
  doc["api_ok_count"]   = apiOkCount;
  doc["api_fail_count"] = apiFailCount;
  doc["last_update"]    = fmtDateTime(lastOkEpoch);
  doc["temp"]           = weather.temp;
  doc["humidity"]       = weather.humidity;
  doc["pressure"]       = weather.pressure;
  doc["wind"]           = weather.wind;
  doc["description"]    = weather.desc;
  doc["city"]           = weather.place;
  doc["valid"]          = weather.valid;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json; charset=utf-8", out);
}

// Any unknown URL. In AP mode this makes the captive portal work: phones probe random
// addresses to detect a login page, and we redirect all of them to the setup page.
void handleNotFound() {
  if (apMode) {
    server.sendHeader("Location", "http://192.168.4.1/config");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain; charset=utf-8", "Lehte ei leitud");   // "page not found"
  }
}

// ---------------------------------------------------------------------------
// Button KEY1 (GPIO3): hold 5 s = erase settings and reboot
// ---------------------------------------------------------------------------
void buttonTick() {
  if (digitalRead(BUTTON_PIN) == LOW) {                        // LOW = pressed (button connects to GND)
    if (buttonDownSince == 0) buttonDownSince = millis();      // remember when the press started
    else if (millis() - buttonDownSince > RESET_HOLD_MS) {     // held long enough
      Serial.println(F("[BTN] Seadistused kustutatud"));
      LittleFS.remove(CONFIG_FILE);
      delay(200);
      ESP.restart();                                           // comes back up in AP mode
    }
  } else {
    buttonDownSince = 0;                                       // released early -> cancel
  }
}

// ---------------------------------------------------------------------------
// OLED (128x64), redrawn once per second
//   AP mode        -> setup instructions (network name, password, address)
//   otherwise      -> 10 s weather view, then 5 s status view, repeating (15 s cycle)
// ---------------------------------------------------------------------------
void oledTick() {
  static uint32_t lastDraw = 0;
  if (lastDraw != 0 && millis() - lastDraw < 1000) return;     // draw max once per second
  lastDraw = millis();

  display.clear();                                             // wipe the frame buffer
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  if (apMode && !staConnected()) {
    // --- Setup view: tells the user how to reach the setup page ---
    display.drawString(0, 0,  F("Seadistamine"));
    display.drawString(0, 16, String(F("WiFi: ")) + AP_SSID);
    display.drawString(0, 28, String(F("Parool: ")) + AP_PASS);
    display.drawString(0, 40, String(F("http://")) + WiFi.softAPIP().toString());
    // Diagnostics line: is the AP up, which channel, how many phones are connected
    display.drawString(0, 52, String(apOk ? F("AP OK") : F("AP VIGA")) + F(", kanal ") + WiFi.channel() +
                              F(", kliente ") + WiFi.softAPgetStationNum());
    Serial.printf("[AP] %s, SSID=%s, kanal %d, kliente %d, vaba mälu %u\n", apOk ? "OK" : "VIGA",
                  WiFi.softAPSSID().c_str(), WiFi.channel(), WiFi.softAPgetStationNum(), ESP.getFreeHeap());
  } else if ((millis() / 5000) % 3 == 2) {
    // --- Status view (5 s of every 15 s): WiFi, IP, API state (F11) ---
    display.drawString(0, 0,  F("Olek"));
    display.drawString(0, 16, staConnected() ? String(F("WiFi: ")) + WiFi.SSID() : String(F("WiFi: ühendamata")));
    display.drawString(0, 28, String(F("IP: ")) + (staConnected() ? WiFi.localIP().toString() : String("-")));
    display.drawString(0, 40, String(F("API: ")) + apiMessage);
  } else {
    // --- Weather view (10 s of every 15 s): city, clock, temperature, description, details (F6) ---
    display.drawString(0, 0, weather.valid && weather.place.length() ? weather.place : cfg.city);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 0, fmtTime(nowUtc()));             // clock, top right
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if (weather.valid) {
      display.setFont(ArialMT_Plain_24);                       // big temperature
      display.drawString(0, 12, String(weather.temp, 1) + " °C");
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 38, weather.desc);
      display.drawString(0, 52, String(weather.humidity) + "%  " + weather.pressure + " hPa  " +
                                String(weather.wind, 1) + " m/s");
    } else {
      display.drawString(0, 24, F("Ilmaandmeid pole veel"));   // "No weather data yet"
    }
  }
  display.display();                                           // push the frame buffer to the screen
}

// ---------------------------------------------------------------------------
// setup(): runs once at power-on
// ---------------------------------------------------------------------------
void setup() {
  // Serial is TX only: GPIO3 (the RX pin) is used by the button KEY1
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  delay(200);
  Serial.println();
  Serial.println(F("=== ESP8266 ilmajaam ==="));

  pinMode(BUTTON_PIN, INPUT_PULLUP);                           // internal pull-up, button pulls to GND

  display.init();                                              // start the OLED
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, F("Ilmajaam käivitub..."));         // boot splash
  display.display();

  WiFi.persistent(false);          // don't write WiFi credentials to flash on every connect (saves wear)
  WiFi.setAutoReconnect(true);     // the WiFi stack reconnects by itself after a drop
  WiFi.hostname(HOST_NAME);        // name the device shows in the router

  // Mount the file system; on the very first boot it is empty, so format it
  if (!LittleFS.begin()) {
    Serial.println(F("[FS] Monteerimine ebaõnnestus, vormindan..."));
    LittleFS.format();
    if (!LittleFS.begin()) Serial.println(F("[FS] VIGA: kontrolli Flash Size seadet (vaja FS)"));
  }

  bool haveCfg = loadConfig();                                 // F5: restore saved settings
  if (cfg.flip) display.flipScreenVertically();                // user-selected orientation

  // F1/F2: saved WiFi -> connect; otherwise (or on failure) start the setup AP
  if (haveCfg && cfg.ssid.length() > 0 && connectSTA(WIFI_TIMEOUT_MS)) {
    onWifiConnected();
  } else {
    startAP();
  }

  // Web server routes
  server.on("/",           HTTP_GET,  handleRoot);             // weather + status page
  server.on("/config",     HTTP_GET,  handleConfig);           // settings form
  server.on("/save",       HTTP_POST, handleSave);             // form submit
  server.on("/reset",      HTTP_POST, handleReset);            // factory reset
  server.on("/api/status", HTTP_GET,  handleApiStatus);        // JSON status
  server.onNotFound(handleNotFound);                           // captive portal redirect / 404
  server.begin();
  Serial.println(F("[HTTP] Veebiserver käivitatud"));
}

// ---------------------------------------------------------------------------
// loop(): runs forever. Everything is non-blocking "tick" functions that each
// check "is it my time yet?", so no single task can hang the device.
// ---------------------------------------------------------------------------
void loop() {
  server.handleClient();     // answer web requests
  MDNS.update();             // keep ilmajaam.local alive
  buttonTick();              // watch KEY1
  oledTick();                // refresh the screen

  // A scheduled reboot (after saving or resetting settings)
  if (restartAt != 0 && (int32_t)(millis() - restartAt) >= 0) {
    Serial.println(F("Taaskäivitus..."));
    delay(100);
    ESP.restart();
  }

  if (apMode) {
    dns.processNextRequest();   // captive portal DNS replies
    apRetryTick();              // F12: try to get back onto the home WiFi
  } else {
    staWatchdogTick();          // F1: watch the WiFi link
  }

  ntpTick();                    // F9: keep the clock in sync
  weatherTick();                // F7: fetch weather when due (all errors are caught - F8)
  yield();                      // let the ESP8266 background tasks run
}
