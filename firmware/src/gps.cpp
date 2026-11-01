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
    if (*f[2] == 'A') fix.speedKmh = atof(f[7]) * 1.852f;
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