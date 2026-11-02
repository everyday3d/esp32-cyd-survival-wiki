#include "gps.h"
#include "config.h"

// ---- a position sent by the phone's browser (web.cpp, /api/pos). Used when no GPS module has a fix. ----
#define PHONE_FRESH_MS 45000
static struct { double lat, lon; float acc; uint32_t ms; } phoneFix = {};
static portMUX_TYPE phoneMux = portMUX_INITIALIZER_UNLOCKED;

void gpsSetPhone(double lat, double lon, float accM) {
  portENTER_CRITICAL(&phoneMux);
  phoneFix.lat = lat; phoneFix.lon = lon; phoneFix.acc = accM;
  phoneFix.ms = millis() | 1;                        // never 0, 0 means "never"
  portEXIT_CRITICAL(&phoneMux);
}

static bool phoneFresh(GpsFix *out) {
  portENTER_CRITICAL(&phoneMux);
  double lat = phoneFix.lat, lon = phoneFix.lon;
  uint32_t ms = phoneFix.ms;
  portEXIT_CRITICAL(&phoneMux);
  if (!ms || millis() - ms > PHONE_FRESH_MS) return false;
  if (out) { out->valid = true; out->lat = lat; out->lon = lon; out->phone = true; out->fixMs = ms; }
  return true;
}

// ---- GPS time (UTC from the NMEA RMC sentence) ----
static uint32_t timeEpoch = 0, timeSyncMs = 0;      // epoch seconds at the last stamp, and millis() then
static bool timeSeen = false;

// days since 1970-01-01 for a civil date (Howard Hinnant's algorithm)
static int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

bool gpsUtcNow(uint32_t *epochS) {
  if (!timeSeen) return false;
  if (epochS) *epochS = timeEpoch + (millis() - timeSyncMs) / 1000;
  return true;
}
uint32_t gpsTimeSyncAgeS() { return timeSeen ? (millis() - timeSyncMs) / 1000 : 0xFFFFFFFF; }
#if PIN_GPS_RX >= 0
static GpsFix fix = {};
static char line[100];
static uint8_t len = 0;

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; }

// Splits an NMEA body in place at commas. Returns the number of fields.
static int splitFields(char *s, char **f, int max) {
  int n = 0; f[n++] = s;
  for (; *s && n < max; s++) if (*s == ',') { *s = 0; f[n++] = s + 1; }
  return n;
}

// ddmm.mmmm + hemisphere -> signed degrees
static double toDeg(const char *v, const char *hemi) {
  if (!*v) return 0;
  double raw = atof(v); int deg = (int)(raw / 100);
  double d = deg + (raw - deg * 100) / 60.0;
  return (*hemi == 'S' || *hemi == 'W') ? -d : d;
}

static void handle(char *s) {                       // s = text between '$' and '*'
  char *f[20]; int n = splitFields(s, f, 20);
  const char *t = f[0];
  if (strlen(t) < 5) return;
  t += 2;                                          // skip talker (GP, GN, GL...)
  if (!strncmp(t, "GGA", 3) && n >= 10) {
    int q = atoi(f[6]);
    fix.sats = atoi(f[7]);
    if (q > 0 && *f[2] && *f[4]) {
      fix.lat = toDeg(f[2], f[3]); fix.lon = toDeg(f[4], f[5]);
      fix.altM = atof(f[9]); fix.valid = true; fix.fixMs = millis();
    } else fix.valid = false;
  } else if (!strncmp(t, "RMC", 3) && n >= 8) {
    if (*f[2] == 'A') {
      fix.speedKmh = atof(f[7]) * 1.852f;
      if (n >= 9 && *f[8] && fix.speedKmh >= 2.0f) { fix.courseDeg = atof(f[8]); fix.courseMs = millis() | 1; }   // heading only means something while moving
      if (n >= 10 && strlen(f[1]) >= 6 && strlen(f[9]) == 6) {          // hhmmss.ss and ddmmyy
        int hh = (f[1][0] - '0') * 10 + (f[1][1] - '0'), mi = (f[1][2] - '0') * 10 + (f[1][3] - '0'), ss = (f[1][4] - '0') * 10 + (f[1][5] - '0');
        int dd = (f[9][0] - '0') * 10 + (f[9][1] - '0'), mo = (f[9][2] - '0') * 10 + (f[9][3] - '0'), yy = 2000 + (f[9][4] - '0') * 10 + (f[9][5] - '0');
        if (hh < 24 && mi < 60 && ss < 61 && dd >= 1 && dd <= 31 && mo >= 1 && mo <= 12) {
          timeEpoch = (uint32_t)daysFromCivil(yy, mo, dd) * 86400UL + hh * 3600UL + mi * 60UL + ss;
          timeSyncMs = millis(); timeSeen = true;
        }
      }
    }
  }
}

static void endOfLine() {
  line[len] = 0; len = 0;
  if (line[0] != '$') return;
  char *star = strchr(line, '*');
  if (!star || !star[1] || !star[2]) return;
  uint8_t sum = 0; for (char *p = line + 1; p < star; p++) sum ^= (uint8_t)*p;
  int hi = hexv(star[1]), lo = hexv(star[2]);
  if (hi < 0 || lo < 0 || sum != (uint8_t)(hi * 16 + lo)) return;   // bad checksum: ignore
  *star = 0; fix.sentences++;
  handle(line + 1);
}

void gpsBegin() {
  // TX pin is a spare pin that stays unconnected: -1 would fall back to GPIO 17, which is the blue LED on the 2.8".
  Serial2.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
}

void gpsLoop() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') { if (len) endOfLine(); }
    else if (len < sizeof(line) - 1) line[len++] = c;
    else len = 0;                                   // overlong line: drop it
  }
  if (fix.valid && millis() - fix.fixMs > 10000) fix.valid = false;   // stale
}

bool gpsPresent() { return true; }
GpsFix gpsGet() {
  GpsFix f = fix;
  if (f.valid) return f;                              // the module has a fix: it wins
  GpsFix p = f;
  if (phoneFresh(&p)) return p;                       // otherwise use the phone's position
  return f;
}
uint32_t gpsFixAgeS() {
  GpsFix g = gpsGet();
  return g.fixMs ? (millis() - g.fixMs) / 1000 : 0xFFFFFFFF;
}
#else
void gpsBegin() {}
void gpsLoop() {}
bool gpsPresent() { return phoneFresh(nullptr); }
GpsFix gpsGet() { GpsFix g{}; phoneFresh(&g); g.sentences = g.valid ? 1 : 0; return g; }
uint32_t gpsFixAgeS() {
  GpsFix g = gpsGet();
  return g.fixMs ? (millis() - g.fixMs) / 1000 : 0xFFFFFFFF;
}
#endif