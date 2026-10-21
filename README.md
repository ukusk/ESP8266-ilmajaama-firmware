# ESP8266 ilmajaam – projektidokumentatsioon

2026-10-21

## Projekti eesmärk

Projekti eesmärk on ehitada ESP8266 mikrokontrolleril põhinev ilmajaam, mis toob OpenWeatherMap API kaudu valitud linna ilmaandmed ja mille seadistusi (WiFi, linn, API võti) saab muuta veebibrauseris ilma programmi uuesti üles laadimata.

Seade peab pärast voolukatkestust ise WiFi-ga ühenduma, salvestatud seadistused laadima ja ilmaandmete uuendamist jätkama.

## Seadme lühikirjeldus

Ilmajaam on USB-toitega ESP8266 arendusplaat, mis töötab ilma ekraani ja anduriteta: ilmaandmed tulevad internetist, kuvamine käib veebilehel.

- Seade ühendub salvestatud WiFi-võrku. Kui see ei õnnestu või seadistusi pole, avab ta oma võrgu `ESP_AP` ja veebilehe aadressil 192.168.4.1.
- Veebilehel sisestatakse WiFi nimi ja parool, linn ning OpenWeatherMapi API võti. Need salvestatakse LittleFS-i faili `/config.json`.
- Seade pärib ilmaandmed valitud intervalliga (vaikimisi 10 minutit) ja kuvab temperatuuri, õhuniiskuse, õhurõhu, tuule kiiruse ja ilmakirjelduse.
- Kellaaeg sünkroonitakse NTP-serveriga. Avalehel on näha viimase eduka uuenduse aeg, WiFi olek ja API päringute olek.
- Plaadi sinine LED näitab olekut ja FLASH-nupp (5 s hoidmine) kustutab seadistused.

## Komponentide loetelu

Seade vajab ühte arendusplaati ja toidet. Sensoreid ega ekraani pole, sest andmed tulevad internetist.

| Nr | Komponent | Kogus | Otstarve |
| --- | --- | --- | --- |
| 1 | NodeMCU v3 arendusplaat (ESP-12E, ESP8266, CP2102 või CH340 USB-kiip) | 1 | Kontroller, WiFi, veebiserver |
| 2 | Micro-USB andmekaabel | 1 | Programmeerimine ja toide |
| 3 | USB-toiteadapter 5 V, vähemalt 1 A | 1 | Püsitoide |
| 4 | Plaadile ehitatud sinine LED (GPIO2 / D4) | 1, plaadil | Olekunäit |
| 5 | Plaadile ehitatud FLASH-nupp (GPIO0 / D3) | 1, plaadil | Seadistuste kustutamine |
| 6 | 2,4 GHz WiFi-ruuter internetiühendusega | 1, väline | Side OpenWeatherMapi ja NTP-ga |
| 7 | Seadistamise seade (telefon või arvuti brauseriga) | 1, väline | Veebiliides |

Lisakomponente ei ole vaja. Soovi korral võib nupu paralleelselt plaadi omaga ühendada väline nupp D3 ja GND vahele.

## Kontroller

Kontrollerina töötab ESP8266 (NodeMCU v3 plaadil), mis ühendab protsessori, WiFi-raadio ja välkmälu ühes kiibis.

| Omadus | Väärtus |
| --- | --- |
| Protsessor | Tensilica L106, 32 bit, 80 MHz (kuni 160 MHz) |
| Mälu | umbes 80 KB RAM, 4 MB välkmälu (programm + LittleFS) |
| Side | WiFi 802.11 b/g/n, ainult 2,4 GHz |
| Loogikatase | 3,3 V (viigud ei talu 5 V) |
| Programmeerimine | Arduino C++, USB-UART (115200 baud) |
| Failisüsteem | LittleFS, Flash Size valik `4MB (FS:1MB OTA:~1019KB)` |

Kasutatud viigud:

| Plaadi viik | GPIO | Funktsioon |
| --- | --- | --- |
| D3 | GPIO0 | FLASH-nupp, sisend sisemise pull-up-ga |
| D4 | GPIO2 | Sisseehitatud sinine LED, väljund (süttib madala tasemega) |
| TX / RX | GPIO1 / GPIO3 | USB-UART, silumisväljund Serial Monitorisse |

## Sisendid

Seadmel on üks füüsiline sisend, kõik ülejäänud on andmeside kaudu.

| Sisend | Allikas | Mida seade sellega teeb |
| --- | --- | --- |
| FLASH-nupp (GPIO0) | Kasutaja | 5 s hoidmine kustutab seadistused ja taaskäivitab seadme |
| Veebivorm `/save` | Kasutaja brauserist | WiFi nimi ja parool, linn, API võti, intervall, ajavöönd |
| OpenWeatherMapi JSON-vastus | Internet (HTTPS) | Temperatuur, niiskus, rõhk, tuul, kirjeldus |
| NTP-vastus | Internet (UDP, `pool.ntp.org`) | Täpne kellaaeg |
| Fail `/config.json` | LittleFS | Salvestatud seadistused käivitusel |

## Väljundid

| Väljund | Sihtkoht | Sisu |
| --- | --- | --- |
| Avaleht `/` | Brauser | Ilmaandmed, WiFi ja API olek, kellaaeg, viimase uuenduse aeg |
| Seadistusleht `/config` | Brauser | Seadistuste vaatamine ja muutmine |
| JSON `/api/status` | Brauser või koduautomaatika | Kõik andmed ja olekud masinloetavalt |
| Sinine LED (GPIO2) | Kasutaja | Pidevalt põleb: WiFi ja API korras. Aeglane vilkumine (1 s): ühendus olemas, ilmaandmed puuduvad või viga. Kiire vilkumine (0,2 s): AP-režiim |
| Serial Monitor | Arvuti, 115200 baud | Ühenduse, seadistuse ja API päringute logi |
| Fail `/config.json` | LittleFS | Salvestatud seadistused |
| WiFi-pääsupunkt `ESP_AP` | Telefon või arvuti | Esmane seadistamine, kui põhivõrku pole |

## Muu: toide, side, andmete hoidmine ja tarkvara

**Toide.** Plaat saab 5 V USB-ühenduse kaudu, plaadi regulaator teeb sellest 3,3 V. Voolutarve on WiFi saatmise ajal ligikaudu 70–200 mA, nii et 1 A adapter on piisav (ligikaudsed väärtused).

**Side.**

- WiFi 2,4 GHz kliendina (STA) põhivõrku, AP-režiimis pääsupunktina `ESP_AP` (WPA2, vaikeparool `ilmajaam123`, muudetav konstandiga `AP_PASS`).
- HTTPS päring `api.openweathermap.org` (port 443). Serveri sertifikaati ei kontrollita (`setInsecure()`), sest ESP8266 mälu on piiratud.
- NTP (UDP 123) aadressile `pool.ntp.org`, ajavöönd ja EL-i suveaeg arvestatakse programmis.
- mDNS nimi `ilmajaam.local` ja captive portal AP-režiimis.

**Andmete hoidmine.** Seadistused on failis `/config.json` (JSON, LittleFS). Salvestamine käib ajutise faili kaudu, et voolukatkestus ei rikuks vana faili. Veebiliides ei näita salvestatud parooli ega API võtit.

**Tarkvara.**

| Teek | Otstarve |
| --- | --- |
| ESP8266WiFi, ESP8266WebServer, ESP8266HTTPClient, ESP8266mDNS, DNSServer, LittleFS | ESP8266 core 3.x kaasas |
| ArduinoJson (v6 või v7) | JSON-i lugemine ja kirjutamine |
| NTPClient (Fabrice Weinberg) | Kellaaja sünkroonimine |

Arendusvahend on Arduino IDE, plaat `NodeMCU 1.0 (ESP-12E Module)`.

**OpenWeatherMapi võti.** Tasuta võtme saab openweathermap.org kontolt. Päringute limiit on tasuta kontol umbes 60 minutis ja 1000 päevas, programmi minimaalne intervall 2 minutit jääb sellest kaugele alla.

## Nõuded loodavale seadmele

**Funktsionaalsed nõuded**

| Nr | Nõue | Kuidas täidetud |
| --- | --- | --- |
| F1 | Seade ühendub automaatselt salvestatud WiFi-võrku | `connectSTA()` käivitusel, `staWatchdogTick()` katkestuse korral |
| F2 | Ühenduse ebaõnnestumisel või seadistuse puudumisel käivitub AP `ESP_AP` | `startAP()` |
| F3 | AP-režiimis pääseb seadme veebiliidesesse | Veebiserver ja captive portal aadressil 192.168.4.1 |
| F4 | WiFi nime ja parooli, linna ning API võtme sisestamine veebis | Leht `/config`, töötleja `handleSave()` |
| F5 | Seadistused salvestatakse LittleFS-i ja säilivad taaskäivitusel | `saveConfig()`, `loadConfig()` |
| F6 | Kuvatakse temperatuur, niiskus, rõhk, tuule kiirus ja kirjeldus | `fetchWeather()`, `handleRoot()` |
| F7 | Ilmaandmeid uuendatakse automaatselt kindla intervalliga | `weatherTick()`, intervall seadistatav (2–1440 min) |
| F8 | API vigade korral seade ei jookse kokku | Kõik veatee harud püütakse kinni, uus katse 60 s pärast |
| F9 | Kellaaeg sünkroonitakse NTP-ga | `ntpTick()` |
| F10 | Kuvatakse viimase eduka uuenduse aeg | `lastOkEpoch`, avaleht |
| F11 | Kuvatakse WiFi ja API päringute olek | Avaleht ja `/api/status` |
| F12 | Pärast voolukatkestust taastub töö ilma kasutaja sekkumiseta | AP-režiimis `apRetryTick()` proovib põhivõrku iga 2 min tagant |

**Mittefunktsionaalsed nõuded**

- Turvalisus: parool ja API võti on veebiliideses peidetud, AP-võrk on WPA2-parooliga, valikuline `ADMIN_PASS` kaitseb seadistuslehti.
- Töökindlus: seadistusfail kirjutatakse ajutise faili kaudu, enne API päringut kontrollitakse vaba mälu (vähemalt 14 KB).
- Kasutusmugavus: SSID valitakse leitud võrkude loendist, seade on leitav nimega `ilmajaam.local`, LED näitab olekut ja FLASH-nupp kustutab seadistused.
- Hooldatavus: kood on kommenteeritud, seadistused ja ajad on konstantidena faili alguses.
- Piirang: HTTPS-i serveri sertifikaati ei kontrollita.

## Ühendusskeem

Seade ei vaja välisi juhtmeid: LED ja nupp on NodeMCU plaadil, plaat saab toite USB-st ja kogu side käib WiFi kaudu.

```text
                    ┌──────────── NodeMCU v3 plaat ────────────┐
                    │                                          │
 ┌──────────────┐   │  ┌────────────────────────────────────┐  │
 │  USB-toide   │   │  │ Micro-USB pesa                     │  │
 │ 5 V, ≥ 1 A   ├──►│  │ 5 V toide ja Serial-andmed         │  │
 └──────────────┘   │  └─────────────────┬──────────────────┘  │
   (Micro-USB)      │                    │ 5 V                 │
                    │  ┌─────────────────▼──────────────────┐  │      ┌──────────────┐
                    │  │ 3,3 V regulaator ja USB-UART       │  │      │   Internet   │
                    │  │ (CP2102 või CH340)                 │  │      │ ilma-API, NTP│
                    │  └─────────────────┬──────────────────┘  │      └──────▲───────┘
                    │                    │ 3,3 V               │             │ HTTPS, NTP
                    │  ┌─────────────────▼──────────────────┐  │      ┌──────▼───────┐
                    │  │ ESP8266 (ESP-12E)                  │◄ ┼ ─ ─ ►│  WiFi-ruuter │
                    │  │ protsessor, WiFi, välkmälu         │  │ WiFi │   2,4 GHz    │
                    │  └───────▲─────────────────┬──────────┘  │      └──────▲───────┘
                    │          │                 │             │             ┊ WiFi
                    │  ┌───────┴──────┐  ┌───────▼──────┐      │      ┌──────▼───────┐
                    │  │ FLASH-nupp   │  │ Sinine LED   │      │◄ ─ ─ ┤   Brauser    │
                    │  │ D3 / GPIO0   │  │ D4 / GPIO2   │      │ ESP_AP│ telefon/arvuti│
                    │  └──────────────┘  └──────────────┘      │      └──────────────┘
                    └──────────────────────────────────────────┘
```

Pidev joon tähendab kaablit, kriipsjoon (`◄ ─ ►`, `┊`) WiFi-ühendust. Plaat ühendub pidevalt ruuteri kaudu internetti ja brauserisse. AP-režiimis (`ESP_AP`) ühendub brauser otse plaadiga, ilma ruuterita.

## Juhtkood

Programm on ühes failis `weather_station.ino` (Arduino IDE-s peab kaust kandma sama nime). Vajalikud teegid on kirjeldatud lõigus „Muu“.

```cpp
/*
 * ESP8266 WiFi ilmajaam (OpenWeatherMap + LittleFS + veebipõhine seadistus)
 * ==========================================================================
 *
 * Vajalikud teegid (Arduino Library Manager):
 *   - ArduinoJson   (Benoit Blanchon, v6.x või v7.x - kood töötab mõlemaga)
 *   - NTPClient     (Fabrice Weinberg, v3.2.x)
 * Sisseehitatud (ESP8266 core 3.x): ESP8266WiFi, ESP8266WebServer, ESP8266HTTPClient,
 *   ESP8266mDNS, DNSServer, LittleFS, WiFiUdp
 *
 * Arduino IDE seaded:
 *   Board:       "NodeMCU 1.0 (ESP-12E Module)" või "Generic ESP8266 Module"
 *   Flash Size:  "4MB (FS:1MB OTA:~1019KB)"  <- failisüsteem PEAB olema valitud!
 *   Board package: "esp8266 by ESP8266 Community" 3.0.0 või uuem
 *   Serial Monitor: 115200 baud
 *
 * Töö loogika:
 *   1. Käivitusel loetakse /config.json LittleFS-ist.
 *   2. Kui WiFi seadistus on olemas -> ühendutakse (15 s). Ebaõnnestumisel käivitub AP "ESP_AP".
 *   3. AP-režiimis proovib seade ise iga 2 min tagant põhivõrku tagasi ühenduda
 *      (nt pärast voolukatkestust, kui ruuter käivitub aeglasemalt).
 *   4. Veebiliides: http://<seadme-IP>/  või  http://ilmajaam.local/
 *      AP-režiimis: ühendu WiFi-võrku "ESP_AP" ja ava http://192.168.4.1/
 *   5. FLASH-nupu (GPIO0) hoidmine 5 s töötamise ajal kustutab seadistused.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

// ---------------------------------------------------------------------------
// Konstandid ja seaded
// ---------------------------------------------------------------------------
static const char*    AP_SSID        = "ESP_AP";        // AP-režiimi võrgu nimi
static const char*    AP_PASS        = "ilmajaam123";   // min 8 märki; "" = avatud võrk
static const char*    ADMIN_PASS     = "";              // != "" -> seadistuslehed nõuavad parooli (kasutaja: admin)
static const char*    HOST_NAME      = "ilmajaam";      // http://ilmajaam.local
static const char*    CONFIG_FILE    = "/config.json";
static const char*    CONFIG_TMP     = "/config.tmp";

static const uint32_t WIFI_TIMEOUT_MS   = 15000;   // esmane ühendumine
static const uint32_t AP_RETRY_MS       = 120000;  // AP-režiimis STA uuesti proovimine
static const uint32_t WIFI_LOST_RECONN  = 30000;   // mitu ms katkestust enne uuesti ühendamist
static const uint32_t WIFI_LOST_AP      = 180000;  // mitu ms katkestust enne AP käivitamist
static const uint32_t API_RETRY_MS      = 60000;   // vea korral proovi uuesti
static const uint32_t API_TIMEOUT_MS    = 8000;
static const uint32_t NTP_TRY_MS        = 15000;   // NTP päringute minimaalne vahe
static const uint32_t RESET_HOLD_MS     = 5000;    // FLASH-nupu hoidmine

static const uint8_t  LED_PIN           = 2;       // sisseehitatud sinine LED (GPIO2 / D4), süttib LOW tasemega
static const uint8_t  BUTTON_PIN        = 0;       // FLASH-nupp (GPIO0 / D3)

static const char*    NTP_SERVER     = "pool.ntp.org";

// ---------------------------------------------------------------------------
// Globaalsed objektid ja muutujad
// ---------------------------------------------------------------------------
struct Config {
  String   ssid;
  String   pass;
  String   city        = "Tallinn";
  String   apiKey;
  uint16_t intervalMin = 10;     // ilmauuenduse intervall minutites
  int8_t   tzHours     = 2;      // ajavöönd UTC suhtes (Eesti = 2)
  bool     dstAuto     = true;   // EL-i suveaja automaatne arvestus
};

struct Weather {
  float  temp      = 0;
  int    humidity  = 0;
  int    pressure  = 0;
  float  wind      = 0;
  String desc;
  String place;
  bool   valid     = false;
};

Config  cfg;
Weather weather;

ESP8266WebServer server(80);
DNSServer        dns;
WiFiUDP          ntpUDP;
NTPClient        timeClient(ntpUDP, NTP_SERVER, 0, 3600000);  // UTC, uuendus 1 h

bool     apMode          = false;
uint32_t lastStaTry      = 0;
uint32_t wifiLostSince   = 0;     // 0 = ühendus korras
uint32_t lastNtpTry      = 0;
uint32_t lastApiAttempt  = 0;
bool     apiAttempted    = false;
bool     lastApiOk       = false;
String   apiMessage      = "Pole veel päritud";
int      apiHttpCode     = 0;
uint32_t apiOkCount      = 0;
uint32_t apiFailCount    = 0;
time_t   lastOkEpoch     = 0;     // viimane edukas uuendus (UTC epoch), 0 = teadmata
uint32_t restartAt       = 0;     // != 0 -> taaskäivita selle aja saabudes
uint32_t buttonDownSince = 0;

// ---------------------------------------------------------------------------
// HTML-i stiil (hoitakse flash-mälus)
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
// Abifunktsioonid
// ---------------------------------------------------------------------------

// HTML-i erimärkide kaitse (XSS-i vältimiseks väärtuste kuvamisel)
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

// URL-kodeering (linnanimi võib sisaldada täpitähti ja tühikuid)
String urlEncode(const String &in) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

// --- Kuupäevaarvutus (suveaja jaoks, ei sõltu newlib-i timegm-ist) ---
static int64_t daysFromCivil(int y, int m, int d) {
  y -= (m <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int mp  = (m > 2) ? m - 3 : m + 9;
  int doy = (153 * mp + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + doe - 719468;
}

// Kuu viimane pühapäev kell 01:00 UTC (EL-i suveaja piir) -> UTC epoch
static int64_t lastSundayUtc(int year, int month) {
  int64_t last = daysFromCivil(year, month, 31);      // märts ja oktoober on 31-päevased
  int wd = (int)((last + 4) % 7);                     // 0 = pühapäev (1970-01-01 oli neljapäev)
  return (last - wd) * 86400LL + 3600;
}

bool isEuDst(time_t utc) {
  time_t t = utc;
  struct tm *tmv = gmtime(&t);
  int year = tmv->tm_year + 1900;
  int64_t u = (int64_t)utc;
  return u >= lastSundayUtc(year, 3) && u < lastSundayUtc(year, 10);
}

// UTC epoch -> kohalik epoch (ajavöönd + valikuline suveaeg)
time_t toLocal(time_t utc) {
  long off = (long)cfg.tzHours * 3600L;
  if (cfg.dstAuto && isEuDst(utc)) off += 3600;
  return utc + off;
}

String fmtDateTime(time_t utc) {
  if (utc == 0) return F("teadmata");
  time_t l = toLocal(utc);
  struct tm *t = gmtime(&l);
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

String fmtUptime() {
  uint32_t s = millis() / 1000;
  char buf[24];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
           (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24),
           (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  return String(buf);
}

bool staConnected() { return WiFi.status() == WL_CONNECTED; }

// ---------------------------------------------------------------------------
// Seadistuse lugemine ja salvestamine (LittleFS)
// ---------------------------------------------------------------------------
bool loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println(F("[CFG] Seadistusfaili ei ole"));
    return false;
  }
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    Serial.println(F("[CFG] Faili avamine ebaõnnestus"));
    return false;
  }
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.print(F("[CFG] JSON viga: "));
    Serial.println(err.c_str());
    return false;
  }
  cfg.ssid        = doc["ssid"]     | "";
  cfg.pass        = doc["pass"]     | "";
  cfg.city        = doc["city"]     | "Tallinn";
  cfg.apiKey      = doc["apikey"]   | "";
  cfg.intervalMin = doc["interval"] | 10;
  cfg.tzHours     = doc["tz"]       | 2;
  cfg.dstAuto     = doc["dst"]      | true;
  if (cfg.intervalMin < 2) cfg.intervalMin = 2;
  Serial.println(F("[CFG] Seadistus loetud"));
  return true;
}

// Kirjutame esmalt ajutisse faili ja nimetame ümber -> pooleli jäänud kirjutus ei riku vana faili
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

  File f = LittleFS.open(CONFIG_TMP, "w");
  if (!f) {
    Serial.println(F("[CFG] Ajutise faili loomine ebaõnnestus"));
    return false;
  }
  size_t n = serializeJson(doc, f);
  f.close();
  if (n == 0) {
    LittleFS.remove(CONFIG_TMP);
    return false;
  }
  LittleFS.remove(CONFIG_FILE);
  if (!LittleFS.rename(CONFIG_TMP, CONFIG_FILE)) {
    Serial.println(F("[CFG] Ümbernimetamine ebaõnnestus"));
    return false;
  }
  Serial.println(F("[CFG] Seadistus salvestatud"));
  return true;
}

// ---------------------------------------------------------------------------
// WiFi: STA ja AP
// ---------------------------------------------------------------------------
bool connectSTA(uint32_t timeoutMs) {
  if (cfg.ssid.length() == 0) return false;
  Serial.printf("[WiFi] Ühendun: %s ", cfg.ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t start = millis();
  while (!staConnected() && millis() - start < timeoutMs) {
    delay(250);
    yield();
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

void startAP() {
  WiFi.mode(WIFI_AP_STA);   // AP + STA, et saaks taustal põhivõrku uuesti proovida
  bool ok = (strlen(AP_PASS) >= 8) ? WiFi.softAP(AP_SSID, AP_PASS) : WiFi.softAP(AP_SSID);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());   // captive portal: kõik nimed -> 192.168.4.1
  apMode = true;
  lastStaTry = millis();
  Serial.printf("[AP] %s, SSID=%s, IP=%s\n", ok ? "Käivitatud" : "VIGA", AP_SSID,
                WiFi.softAPIP().toString().c_str());
}

void stopAP() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apMode = false;
  Serial.println(F("[AP] Peatatud, töötan STA-režiimis"));
}

void onWifiConnected() {
  wifiLostSince = 0;
  if (MDNS.begin(HOST_NAME)) MDNS.addService("http", "tcp", 80);
  timeClient.begin();
  lastNtpTry = 0;
}

// AP-režiimis: proovi aeg-ajalt salvestatud võrku tagasi ühenduda
void apRetryTick() {
  if (cfg.ssid.length() == 0) return;
  if (millis() - lastStaTry < AP_RETRY_MS) return;
  lastStaTry = millis();
  if (WiFi.softAPgetStationNum() > 0) return;   // kasutaja seadistab parajasti - ära sega

  Serial.println(F("[WiFi] AP-režiimis: proovin põhivõrku"));
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t st = millis();
  while (!staConnected() && millis() - st < 12000) {
    delay(250);
    yield();
  }
  if (staConnected()) {
    stopAP();
    if (!staConnected()) connectSTA(10000);
    if (staConnected()) onWifiConnected();
  } else {
    WiFi.disconnect();
  }
}

// STA-režiimis: jälgi ühenduse katkemist
void staWatchdogTick() {
  if (staConnected()) {
    if (wifiLostSince != 0) {
      Serial.println(F("[WiFi] Ühendus taastus"));
      wifiLostSince = 0;
    }
    return;
  }
  if (wifiLostSince == 0) {
    wifiLostSince = millis();
    if (wifiLostSince == 0) wifiLostSince = 1;
    Serial.println(F("[WiFi] Ühendus katkes"));
    return;
  }
  uint32_t lost = millis() - wifiLostSince;
  static bool reconnTried = false;
  if (lost > WIFI_LOST_RECONN && !reconnTried) {
    reconnTried = true;
    WiFi.disconnect();
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  }
  if (staConnected()) reconnTried = false;
  if (lost > WIFI_LOST_AP) {
    reconnTried = false;
    startAP();   // seade jääb ligipääsetavaks; apRetryTick jätkab proovimist
  }
}

// ---------------------------------------------------------------------------
// NTP
// ---------------------------------------------------------------------------
void ntpTick() {
  if (!staConnected()) return;
  if (lastNtpTry != 0 && millis() - lastNtpTry < NTP_TRY_MS) return;
  lastNtpTry = millis();
  timeClient.update();   // teeb päringu ainult siis, kui intervall on täis või aeg pole seatud
}

time_t nowUtc() {
  return timeClient.isTimeSet() ? (time_t)timeClient.getEpochTime() : 0;
}

// ---------------------------------------------------------------------------
// OpenWeatherMap
// ---------------------------------------------------------------------------
void fetchWeather() {
  apiAttempted   = true;
  lastApiAttempt = millis();
  lastApiOk      = false;

  if (cfg.apiKey.length() == 0 || cfg.city.length() == 0) {
    apiMessage = F("API võti või linn puudub");
    apiHttpCode = 0;
    return;
  }
  if (ESP.getFreeHeap() < 14000) {
    apiMessage = F("Liiga vähe vaba mälu");
    apiFailCount++;
    return;
  }

  String url = F("https://api.openweathermap.org/data/2.5/weather?q=");
  url += urlEncode(cfg.city);
  url += F("&appid=");
  url += urlEncode(cfg.apiKey);
  url += F("&units=metric&lang=et");

  // setInsecure(): sertifikaati ei kontrollita (ESP8266 mälu on piiratud).
  // API võti liigub krüpteeritult, aga serveri ehtsust ei kontrollita.
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);

  HTTPClient http;
  http.setTimeout(API_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    apiMessage = F("HTTP-ühenduse loomine ebaõnnestus");
    apiFailCount++;
    return;
  }

  int code = http.GET();
  apiHttpCode = code;

  if (code == HTTP_CODE_OK) {
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
    } else if (!doc["main"]["temp"].is<float>()) {
      apiMessage = F("Vastus ei sisalda ilmaandmeid");
      apiFailCount++;
    } else {
      weather.temp     = doc["main"]["temp"].as<float>();
      weather.humidity = doc["main"]["humidity"] | 0;
      weather.pressure = doc["main"]["pressure"] | 0;
      weather.wind     = doc["wind"]["speed"]    | 0.0f;
      weather.desc     = String((const char *)(doc["weather"][0]["description"] | ""));
      weather.place    = String((const char *)(doc["name"] | ""));
      weather.valid    = true;
      lastApiOk        = true;
      apiOkCount++;
      lastOkEpoch      = nowUtc();
      apiMessage       = F("OK");
      Serial.printf("[API] OK: %.1f C, %d%%, %d hPa, %.1f m/s, %s\n",
                    weather.temp, weather.humidity, weather.pressure, weather.wind,
                    weather.desc.c_str());
    }
  } else if (code > 0) {
    // OWM annab vea korral JSON-i: {"cod":401,"message":"..."}
    String body = http.getString();
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(512);
#endif
    String msg;
    if (!deserializeJson(doc, body)) msg = String((const char *)(doc["message"] | ""));
    if (code == 401)      apiMessage = F("Vigane API võti (401)");
    else if (code == 404) apiMessage = F("Linna ei leitud (404)");
    else if (code == 429) apiMessage = F("Päringute limiit ületatud (429)");
    else                  apiMessage = String(F("HTTP ")) + code + (msg.length() ? " - " + msg : "");
    apiFailCount++;
  } else {
    apiMessage = String(F("Ühenduse viga: ")) + HTTPClient::errorToString(code);
    apiFailCount++;
  }
  http.end();
  if (!lastApiOk) Serial.println("[API] Viga: " + apiMessage);
}

void weatherTick() {
  if (!staConnected()) return;
  uint32_t interval = lastApiOk ? (uint32_t)cfg.intervalMin * 60000UL : API_RETRY_MS;
  if (!apiAttempted || millis() - lastApiAttempt >= interval) fetchWeather();
}

// ---------------------------------------------------------------------------
// Veebiserver
// ---------------------------------------------------------------------------
bool authOk() {
  if (strlen(ADMIN_PASS) == 0) return true;
  if (server.authenticate("admin", ADMIN_PASS)) return true;
  server.requestAuthentication();
  return false;
}

void sendHead(const char *title, int refreshSec = 0) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  String h = F("<!DOCTYPE html><html lang='et'><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  if (refreshSec > 0) { h += F("<meta http-equiv='refresh' content='"); h += refreshSec; h += F("'>"); }
  h += F("<title>"); h += title; h += F("</title>");
  server.sendContent(h);
  server.sendContent_P(STYLE);
  server.sendContent(F("</head><body><div class='w'>"));
}

void sendFoot() {
  server.sendContent(F("</div></body></html>"));
  server.sendContent("");   // lõpetab chunked-vastuse
}

String row(const char *k, const String &v) {
  return String(F("<tr><td>")) + k + F("</td><td>") + v + F("</td></tr>");
}

// Avaleht: ilm + olek
void handleRoot() {
  if (apMode && !staConnected()) {   // esmakordsel seadistamisel otse seadistuslehele
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
  if (weather.valid) {
    s += F("<div class='t'>"); s += String(weather.temp, 1); s += F(" &deg;C</div><div>");
    s += htmlEscape(weather.desc);
    s += F("</div><table>");
    s += row("Õhuniiskus", String(weather.humidity) + " %");
    s += row("Õhurõhk",    String(weather.pressure) + " hPa");
    s += row("Tuule kiirus", String(weather.wind, 1) + " m/s");
    s += row("Viimati uuendatud", fmtDateTime(lastOkEpoch));
    s += F("</table>");
  } else {
    s += F("<p>Ilmaandmeid pole veel saadud.</p>");
  }
  s += F("</div><div class='c'><h2>Olek</h2><table>");
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

// Seadistuste vorm
void handleConfig() {
  if (!authOk()) return;
  sendHead("Seadistused");
  String s;
  s.reserve(2500);
  s += F("<h1>&#9881; Seadistused</h1><form class='c' method='POST' action='/save'>");

  // Ümbritsevad WiFi-võrgud (sünkroonne skaneerimine ~2 s)
  s += F("<label>WiFi võrgu nimi (SSID)</label><input type='text' name='ssid' list='nets' maxlength='32' value='");
  s += htmlEscape(cfg.ssid);
  s += F("'><datalist id='nets'>");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 15; i++) {
    s += F("<option value='"); s += htmlEscape(WiFi.SSID(i)); s += F("'>");
  }
  WiFi.scanDelete();
  s += F("</datalist>");

  s += F("<label>WiFi parool</label><input type='password' name='pass' maxlength='63' placeholder='");
  s += cfg.pass.length() ? F("(jäta tühjaks, et säilitada praegune)") : F("(avatud võrgu korral tühi)");
  s += F("'>");

  s += F("<label>Linn (nt Tallinn või Tartu,EE)</label><input type='text' name='city' maxlength='60' value='");
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

  s += F("<button type='submit'>Salvesta ja taaskäivita</button></form>");
  s += F("<form class='c' method='POST' action='/reset' onsubmit=\"return confirm('Kustutada kõik seadistused?')\">"
         "<h2>Tehaseseaded</h2><button class='r' type='submit'>Kustuta seadistused</button></form>"
         "<a class='b' href='/'>&larr; Tagasi</a>");
  server.sendContent(s);
  sendFoot();
}

// Seadistuste salvestamine
void handleSave() {
  if (!authOk()) return;
  Config n = cfg;   // töötame koopiaga, kuni kõik on valideeritud

  String ssid = server.arg("ssid"); ssid.trim();
  String pass = server.arg("pass");
  String city = server.arg("city"); city.trim();
  String key  = server.arg("apikey"); key.trim();

  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63 ||
      city.length() == 0 || city.length() > 60) {
    server.send(400, "text/plain; charset=utf-8", "Vigased andmed: kontrolli SSID-d ja linna.");
    return;
  }
  if (ssid != cfg.ssid) n.pass = pass;       // uus võrk -> parool tuleb vormilt (võib olla tühi)
  else if (pass.length() > 0) n.pass = pass; // sama võrk -> tühi väli säilitab vana parooli
  n.ssid = ssid;
  n.city = city;
  if (key.length() > 0) n.apiKey = key;

  long iv = server.arg("interval").toInt();
  long tz = server.arg("tz").toInt();
  n.intervalMin = (uint16_t)constrain(iv, 2L, 1440L);
  n.tzHours     = (int8_t)constrain(tz, -12L, 14L);
  n.dstAuto     = server.hasArg("dst");

  cfg = n;
  bool ok = saveConfig();

  sendHead(ok ? "Salvestatud" : "Viga");
  if (ok) {
    server.sendContent(F("<div class='c'><h1>&#10004; Salvestatud</h1>"
                         "<p>Seade taaskäivitub ja ühendub võrku. Kui ühendus õnnestub, "
                         "leiad lehe aadressilt <b>http://ilmajaam.local</b> "
                         "või ruuteri poolt antud IP-aadressilt (vaata Serial Monitori).</p></div>"));
    restartAt = millis() + 1500;
  } else {
    server.sendContent(F("<div class='c'><h1 class='er'>Salvestamine ebaõnnestus</h1>"
                         "<p>LittleFS-i kirjutamine ei õnnestunud. Kontrolli, et Flash Size valikus "
                         "on failisüsteem (nt 4MB FS:1MB).</p></div><a class='b' href='/config'>Tagasi</a>"));
  }
  sendFoot();
}

void handleReset() {
  if (!authOk()) return;
  LittleFS.remove(CONFIG_FILE);
  sendHead("Lähtestatud");
  server.sendContent(F("<div class='c'><h1>Seadistused kustutatud</h1>"
                       "<p>Seade taaskäivitub ja avab võrgu <b>ESP_AP</b>.</p></div>"));
  sendFoot();
  restartAt = millis() + 1500;
}

// JSON-olek (nt koduautomaatika jaoks)
void handleApiStatus() {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(1024);
#endif
  doc["wifi_connected"] = staConnected();
  doc["ap_mode"]        = apMode;
  doc["rssi"]           = staConnected() ? WiFi.RSSI() : 0;
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

void handleNotFound() {
  if (apMode) {   // captive portal: suuna kõik päringud seadistuslehele
    server.sendHeader("Location", "http://192.168.4.1/config");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain; charset=utf-8", "Lehte ei leitud");
  }
}

// ---------------------------------------------------------------------------
// FLASH-nupp (GPIO0): 5 s hoidmine kustutab seadistused
// ---------------------------------------------------------------------------
void buttonTick() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (buttonDownSince == 0) buttonDownSince = millis();
    else if (millis() - buttonDownSince > RESET_HOLD_MS) {
      Serial.println(F("[BTN] Seadistused kustutatud"));
      LittleFS.remove(CONFIG_FILE);
      delay(200);
      ESP.restart();
    }
  } else {
    buttonDownSince = 0;
  }
}

// ---------------------------------------------------------------------------
// Olekunäit (sisseehitatud LED)
//   pidevalt põleb  - WiFi ühendatud ja viimane API päring õnnestus
//   aeglane vilkumine (1 s) - WiFi olemas, aga ilmaandmed puuduvad või viga
//   kiire vilkumine (0,2 s) - AP-režiim (seadistamine)
// ---------------------------------------------------------------------------
void ledTick() {
  bool on;
  if (apMode)                          on = (millis() / 200) % 2 == 0;
  else if (staConnected() && lastApiOk) on = true;
  else                                  on = (millis() / 1000) % 2 == 0;
  digitalWrite(LED_PIN, on ? LOW : HIGH);   // LED on aktiivselt madal
}

// ---------------------------------------------------------------------------
// setup() ja loop()
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== ESP8266 ilmajaam ==="));

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);     // LED välja
  WiFi.persistent(false);          // ära kulutada flash-mälu WiFi seadistuse kirjutamisega
  WiFi.setAutoReconnect(true);
  WiFi.hostname(HOST_NAME);

  // LittleFS: kui esmakordsel käivitusel pole failisüsteemi, vormindatakse see
  if (!LittleFS.begin()) {
    Serial.println(F("[FS] Monteerimine ebaõnnestus, vormindan..."));
    LittleFS.format();
    if (!LittleFS.begin()) Serial.println(F("[FS] VIGA: kontrolli Flash Size seadet (vaja FS)"));
  }

  bool haveCfg = loadConfig();

  if (haveCfg && cfg.ssid.length() > 0 && connectSTA(WIFI_TIMEOUT_MS)) {
    onWifiConnected();
  } else {
    startAP();
  }

  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/config",     HTTP_GET,  handleConfig);
  server.on("/save",       HTTP_POST, handleSave);
  server.on("/reset",      HTTP_POST, handleReset);
  server.on("/api/status", HTTP_GET,  handleApiStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("[HTTP] Veebiserver käivitatud"));
}

void loop() {
  server.handleClient();
  MDNS.update();
  buttonTick();
  ledTick();

  if (restartAt != 0 && (int32_t)(millis() - restartAt) >= 0) {
    Serial.println(F("Taaskäivitus..."));
    delay(100);
    ESP.restart();
  }

  if (apMode) {
    dns.processNextRequest();
    apRetryTick();
  } else {
    staWatchdogTick();
  }

  ntpTick();
  weatherTick();   // kõik vead püütakse kinni - seade ei jookse kokku
  yield();
}
```
