#ifndef MESH_H
#define MESH_H

#include <Arduino.h>
#include "config.h"

enum MeshType : uint8_t {
  MT_HELLO = 1,
  MT_PING  = 2,
  MT_PONG  = 3,
  MT_TEXT  = 4,
  MT_BUZZ  = 5
};

typedef struct __attribute__((packed)) {
  uint8_t  magic;
  uint8_t  version;
  uint8_t  type;
  uint8_t  ttl;
  uint8_t  hops;
  uint16_t srcId;
  uint16_t dstId;
  uint16_t seq;
  uint8_t  srcMac[6];
  uint8_t  payloadLen;
  uint8_t  payload[MESH_MAX_PAYLOAD];
} MeshPacket;

typedef struct {
  bool     used;
  uint16_t id;
  uint8_t  mac[6];
  int8_t   rssi;
  uint8_t  hops;
  bool     direct;
  uint32_t lastSeen;
  uint32_t lastDirect;
} MeshNode;

typedef struct {
  uint16_t from;
  uint16_t to;
  char     text[MESH_MAX_PAYLOAD + 1];
  uint32_t at;
} MeshMsgItem;
#define MSG_HIST_SIZE 8

typedef struct {
  uint32_t tx;
  uint32_t rx;
  uint32_t relay;
  uint32_t dup;
  uint16_t pps;
  int8_t   lastRssi;
} MeshMetrics;

void        meshBegin();
void        meshEnd();
void        meshLoop();
uint16_t    meshMyId();
const char* meshMyName();
void        meshFormatId(uint16_t id, char* out, size_t n);

void        meshSendText(const char* text, uint16_t dstId = 0xFFFF);
void        meshSendPing();

void        meshSendBuzz(uint16_t freqHz, uint16_t durMs, uint16_t dstId);

int         meshCannedCount();
const char* meshCannedAt(int idx);

MeshMetrics meshGetMetrics();
int         meshNodeCount();
int         meshDirectCount();
const MeshNode* meshNodeAt(int idx);
bool        meshNodeAlive(const MeshNode* n);

int         meshPingResponders();
uint16_t    meshPingResponderId(int idx);
bool        meshPingInProgress();
uint16_t    meshLastPingFrom();
uint32_t    meshLastPingFromAt();

bool        meshHasNewText();
const char* meshLastText();
uint16_t    meshLastTextFrom();
void        meshClearNewText();

int                 meshMsgHistCount();
const MeshMsgItem*  meshMsgHistAt(int idx);

#endif
