// Off-grid text messaging between boards over ESP-NOW (no router, no internet).
// Messages are broadcast with a hop count and re-broadcast by every board that hears them,
// so they can hop across more than two units. They are NOT encrypted.
#pragma once
#include <Arduino.h>

#define MESH_NAME_LEN 12
#define MESH_TEXT_LEN 64
#define MESH_LOG      12

struct MeshMsg {
  char from[MESH_NAME_LEN];
  char text[MESH_TEXT_LEN];
  bool mine;
  uint32_t ms;       // millis() when sent or received
};

bool meshBegin(uint8_t channel, bool apActive);   // (re)start; apActive = the SurvivalWiki hotspot is running
void meshEnd();
bool meshActive();
void meshLoop();                                  // call from loop()
bool meshSend(const char *text);
int  meshCount();
const MeshMsg &meshAt(int i);                     // 0 = oldest
const char *meshName();
void meshSetName(const char *name);
bool meshTakeNew();                               // true once per received message
int  meshUnread();
void meshMarkRead();
int  meshPeersHeard();                            // boards heard in the last 2 minutes (hello pings every 30 s)
#ifdef SHOT_SERIAL
void meshDemo();                                  // screenshot builds only
#endif
