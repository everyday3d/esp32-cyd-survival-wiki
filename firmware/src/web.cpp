#include "web.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SD.h>

static WebServer server(80);
static DNSServer dns;
static TaskHandle_t webTaskHandle = nullptr;
static volatile bool running = false;
static volatile bool stopRequested = false;
static bool routesAdded = false;
static const IPAddress apIP(192, 168, 4, 1);

// Send the visitor to the wiki home page. Phones open this automatically
// when their connectivity check gets redirected (the "captive portal" pop-up).
static void redirectHome() {
  server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
  server.sendHeader("Cache-Control", "no-cache");
  server.send(302, "text/plain", "");
}

static bool streamPath(const char *path, const char *type, const char *cache) {
  String gz = String(path) + ".gz";
  File f;
  bool zipped = false;
  if (SD.exists(gz)) { f = SD.open(gz, FILE_READ); zipped = true; }
  else if (SD.exists(path)) { f = SD.open(path, FILE_READ); }
  if (!f) return false;
  if (cache) server.sendHeader("Cache-Control", cache);
  if (zipped) server.sendHeader("Content-Encoding", "gzip");
  server.setContentLength(f.size());
  server.send(200, type, "");
  uint8_t buf[2048];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    if (!n) break;
    if (server.client().write(buf, n) != n) break;
  }
  f.close();
  return true;
}

static void handleIndex() {
  if (!streamPath(F_WEB_INDEX, "text/html", "no-cache")) {
    server.send(500, "text/html",
                "<h1>Survival Wiki</h1><p>SD card missing or /web/index.html not found.</p>");
  }
}

static void handleStatus() {
  String j = "{\"version\":\"" FW_VERSION "\",\"clients\":";
  j += WiFi.softAPgetStationNum();
  j += ",\"uptime\":";
  j += millis() / 1000;
  j += ",\"sdTotalMB\":";
  j += (uint32_t)(SD.totalBytes() / (1024 * 1024));
  j += ",\"sdUsedMB\":";
  j += (uint32_t)(SD.usedBytes() / (1024 * 1024));
  j += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", j);
}

static void handleNotFound() {
  // Any request for another host name (google.com, captive.apple.com, ...) goes home.
  String host = server.hostHeader();
  if (host.length() && host != apIP.toString() && host != "survival.wiki") {
    redirectHome();
    return;
  }
  server.send(404, "text/html",
              "<meta name=viewport content='width=device-width'><h2>Not found</h2>"
              "<p><a href='/'>Back to the Survival Wiki</a></p>");
}

static void webTask(void *) {
  while (!stopRequested) {
    dns.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  webTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void webStart(const WifiConfig &cfg) {
  if (running) return;
  static bool eventsAdded = false;
  if (!eventsAdded) {
    eventsAdded = true;
    WiFi.onEvent([](WiFiEvent_t e, WiFiEventInfo_t info) {
      if (e == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("[wifi] phone connected");
      else if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("[wifi] phone disconnected");
      else if (e == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)
        Serial.printf("[wifi] gave phone address " IPSTR "\n", IP2STR(&info.wifi_ap_staipassigned.ip));
    });
  }
  // Start the hotspot with the chip's defaults (address 192.168.4.1, DHCP on).
  // Setting the address before the AP is up can leave phones without an IP
  // address, which phones report as "unable to join".
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  const char *pass = cfg.password.length() >= 8 ? cfg.password.c_str() : nullptr;
  bool ok = WiFi.softAP(cfg.ssid.c_str(), pass, cfg.channel, 0, 4);
  delay(300);
  Serial.printf("[wifi] hotspot '%s' %s, %s, channel %d, address %s\n", cfg.ssid.c_str(),
                ok ? "started" : "FAILED", pass ? "password set" : "open",
                cfg.channel, WiFi.softAPIP().toString().c_str());

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", apIP);

  if (!routesAdded) {
  routesAdded = true;
  // Connectivity checks used by Android, iOS/macOS and Windows -> portal pop-up
  const char *probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html",
                          "/library/test/success.html", "/connecttest.txt",
                          "/ncsi.txt", "/redirect", "/success.txt", "/canonical.html",
                          "/fwlink"};
  for (const char *p : probes) server.on(p, redirectHome);

  server.on("/", handleIndex);
  server.on("/index.html", handleIndex);
  server.on("/api/status", handleStatus);
  server.serveStatic("/web", SD, "/web", "max-age=86400");
  server.serveStatic("/wiki", SD, "/wiki", "max-age=3600");
  server.serveStatic("/library", SD, "/library", "max-age=86400");
  server.onNotFound(handleNotFound);
  }
  server.begin();

  stopRequested = false;
  running = true;
  xTaskCreatePinnedToCore(webTask, "web", 8192, nullptr, 1, &webTaskHandle, 0);
}

void webStop() {
  if (!running) return;
  stopRequested = true;
  for (int i = 0; i < 200 && webTaskHandle; i++) delay(5);
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  running = false;
}

bool webRunning() { return running; }
int webClientCount() { return running ? WiFi.softAPgetStationNum() : 0; }
String webIP() { return apIP.toString(); }

static String qrEscape(const String &s) {
  String o;
  for (char c : s) {
    if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') o += '\\';
    o += c;
  }
  return o;
}

String webQRText(const WifiConfig &cfg) {
  if (cfg.password.length() >= 8)
    return "WIFI:T:WPA;S:" + qrEscape(cfg.ssid) + ";P:" + qrEscape(cfg.password) + ";;";
  return "WIFI:T:nopass;S:" + qrEscape(cfg.ssid) + ";;";
}
