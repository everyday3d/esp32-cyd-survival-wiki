#include "mesh.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define MESH_MAGIC 0x53      // 'S'
#define MESH_VER   1
#define MESH_TTL   4         // hops a message may travel

#pragma pack(push, 1)
struct Pkt {
  uint8_t magic, ver, ttl, rsv;
  uint32_t id;
  char from[MESH_NAME_LEN];
  char text[MESH_TEXT_LEN];
};
#pragma pack(pop)

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool active = false;
static char myName[MESH_NAME_LEN] = "";
static MeshMsg logv[MESH_LOG];
static int logN = 0;
static int unread = 0;
static bool gotNew = false;
static uint32_t lastSendMs = 0;
static uint32_t nextHello = 0;
#define HELLO_MS 30000UL      // each board announces itself this often, so "heard" shows who is nearby
#define HEARD_MS 120000UL     // a board counts as nearby if we heard it within this long

// Receive queue (filled from the Wi-Fi task, drained in meshLoop)
#define RXQ 8
static Pkt rxq[RXQ];
static volatile int rxHead = 0, rxTail = 0;
static portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

// Duplicate filter
#define SEEN_N 64
static uint32_t seen[SEEN_N];
static int seenPos = 0;

// Re-broadcasts waiting for their random delay
struct Pending { Pkt p; uint32_t due; bool used; };
static Pending pend[6];

// Boards heard recently
struct Peer { char name[MESH_NAME_LEN]; uint32_t ms; };
static Peer peers[8];

static uint32_t keyOf(const Pkt &p) {
  uint32_t h = p.id;
  for (int i = 0; i < MESH_NAME_LEN && p.from[i]; i++) h = h * 31 + (uint8_t)p.from[i];
  return h | 1;     // never 0, so 0 means "empty slot"
}

static bool seenBefore(uint32_t k) {
  for (int i = 0; i < SEEN_N; i++) if (seen[i] == k) return true;
  return false;
}
static void markSeen(uint32_t k) { seen[seenPos] = k; seenPos = (seenPos + 1) % SEEN_N; }

static void addLog(const char *from, const char *text, bool mine) {
  if (logN == MESH_LOG) { memmove(&logv[0], &logv[1], sizeof(MeshMsg) * (MESH_LOG - 1)); logN--; }
  MeshMsg &m = logv[logN++];
  strlcpy(m.from, from, sizeof(m.from));
  strlcpy(m.text, text, sizeof(m.text));
  m.mine = mine;
  m.ms = millis();
}

static void notePeer(const char *name) {
  int free_i = -1, oldest = 0;
  for (int i = 0; i < 8; i++) {
    if (peers[i].name[0] && strcmp(peers[i].name, name) == 0) { peers[i].ms = millis(); return; }
    if (!peers[i].name[0] && free_i < 0) free_i = i;
    if (peers[i].ms < peers[oldest].ms) oldest = i;
  }
  int i = free_i >= 0 ? free_i : oldest;
  strlcpy(peers[i].name, name, sizeof(peers[i].name));
  peers[i].ms = millis();
}

static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != (int)sizeof(Pkt)) return;
  portENTER_CRITICAL(&rxMux);
  int next = (rxHead + 1) % RXQ;
  if (next != rxTail) { memcpy(&rxq[rxHead], data, sizeof(Pkt)); rxHead = next; }
  portEXIT_CRITICAL(&rxMux);
}

static void onSent(const uint8_t *mac, esp_now_send_status_t st) {}

static void sanitize(char *s, size_t n) {
  s[n - 1] = 0;
  for (size_t i = 0; i < n && s[i]; i++) if ((uint8_t)s[i] < 32 || (uint8_t)s[i] > 126) s[i] = '?';
}

static void defaultName() {
  if (myName[0]) return;
  uint64_t mac = ESP.getEfuseMac();
  snprintf(myName, sizeof(myName), "N%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
}

bool meshActive() { return active; }
const char *meshName() { defaultName(); return myName; }
void meshSetName(const char *name) {
  String t = String(name);
  t.trim();
  if (!t.length()) { myName[0] = 0; defaultName(); return; }
  strlcpy(myName, t.c_str(), sizeof(myName));
  sanitize(myName, sizeof(myName));
}

bool meshBegin(uint8_t channel, bool apActive) {
  meshEnd();
  defaultName();
  if (!apActive) {                       // hotspot off: bring the radio up ourselves, on the same channel
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
  }
  if (esp_now_init() != ESP_OK) { Serial.println("[mesh] esp_now_init failed"); return false; }
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;                      // current channel
  peer.encrypt = false;
  peer.ifidx = apActive ? WIFI_IF_AP : WIFI_IF_STA;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("[mesh] add_peer failed"); esp_now_deinit(); return false; }
  active = true;
  nextHello = millis() + 1500 + (esp_random() % 2000);
  Serial.printf("[mesh] on, name %s, channel %d, %s\n", myName, channel, apActive ? "with hotspot" : "radio only");
  return true;
}

void meshEnd() {
  if (!active) return;
  esp_now_deinit();
  active = false;
  for (auto &p : pend) p.used = false;
}

static void txPkt(const Pkt &p) { esp_now_send(BCAST, (const uint8_t *)&p, sizeof(Pkt)); }

bool meshSend(const char *text) {
  if (!active) return false;
  uint32_t now = millis();
  if (now - lastSendMs < 700) return false;      // simple rate limit
  lastSendMs = now;
  Pkt p;
  memset(&p, 0, sizeof(p));
  p.magic = MESH_MAGIC; p.ver = MESH_VER; p.ttl = MESH_TTL;
  p.id = esp_random();
  strlcpy(p.from, myName, sizeof(p.from));
  strlcpy(p.text, text, sizeof(p.text));
  sanitize(p.text, sizeof(p.text));
  markSeen(keyOf(p));
  addLog(p.from, p.text, true);
  txPkt(p);
  return true;
}

void meshLoop() {
  if (!active) return;
  uint32_t now = millis();
  // drain received packets
  while (true) {
    Pkt p;
    portENTER_CRITICAL(&rxMux);
    bool have = rxTail != rxHead;
    if (have) { memcpy(&p, &rxq[rxTail], sizeof(Pkt)); rxTail = (rxTail + 1) % RXQ; }
    portEXIT_CRITICAL(&rxMux);
    if (!have) break;
    if (p.magic != MESH_MAGIC || p.ver != MESH_VER) continue;
    sanitize(p.from, sizeof(p.from));
    sanitize(p.text, sizeof(p.text));
    if (p.rsv == 1) {                              // hello ping: note the board, nothing else
      if (strcmp(p.from, myName) != 0) notePeer(p.from);
      continue;
    }
    uint32_t k = keyOf(p);
    if (seenBefore(k)) continue;
    markSeen(k);
    if (strcmp(p.from, myName) == 0) continue;   // someone using our name: don't show our own echo
    notePeer(p.from);
    addLog(p.from, p.text, false);
    Serial.printf("[mesh] from %s: %s (ttl %d)\n", p.from, p.text, p.ttl);
    unread++;
    gotNew = true;
    if (p.ttl > 1) {                              // pass it on after a short random delay
      for (auto &q : pend) {
        if (q.used) continue;
        q.p = p; q.p.ttl--;
        q.due = now + 30 + (esp_random() % 120);
        q.used = true;
        break;
      }
    }
  }
  for (auto &q : pend) {
    if (q.used && (int32_t)(now - q.due) >= 0) { txPkt(q.p); q.used = false; }
  }
  if ((int32_t)(now - nextHello) >= 0) {           // hello ping: direct neighbours only, not passed on
    nextHello = now + HELLO_MS;
    Pkt h;
    memset(&h, 0, sizeof(h));
    h.magic = MESH_MAGIC; h.ver = MESH_VER; h.ttl = 1; h.rsv = 1;
    h.id = esp_random();
    strlcpy(h.from, myName, sizeof(h.from));
    txPkt(h);
  }
}

#ifdef SHOT_SERIAL
// Screenshot builds only: fill the inbox with a believable conversation.
void meshDemo() {
  logN = 0;
  addLog("Jim", "Made it to the ridge. Water at the creek.", false);
  logv[logN - 1].ms = millis() - 14 * 60000UL;
  addLog("Sam", "Copy. Heading your way.", false);
  logv[logN - 1].ms = millis() - 9 * 60000UL;
  addLog("me", "Need help", true);
  strlcpy(logv[logN - 1].from, myName, sizeof(logv[0].from));
  logv[logN - 1].ms = millis() - 3 * 60000UL;
  addLog("Sam", "On my way, 10 minutes out.", false);
  logv[logN - 1].ms = millis() - 20000UL;
  for (auto &p : peers) p.name[0] = 0;
  notePeer("Jim");
  notePeer("Sam");
}
#endif

int meshCount() { return logN; }
const MeshMsg &meshAt(int i) { return logv[i]; }
bool meshTakeNew() { bool g = gotNew; gotNew = false; return g; }
int meshUnread() { return unread; }
void meshMarkRead() { unread = 0; }

int meshPeersHeard() {
  int n = 0;
  uint32_t now = millis();
  for (auto &p : peers) if (p.name[0] && now - p.ms < HEARD_MS) n++;
  return n;
}
