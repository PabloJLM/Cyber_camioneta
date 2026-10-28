#include "mesh.h"
#include "buzzer.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#define MESH_MAGIC    0xA5
#define MESH_VERSION  1

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#define HEADER_SIZE   (sizeof(MeshPacket) - MESH_MAX_PAYLOAD)

#define PING_WINDOW   1500

static uint8_t  myMac[6];
static uint16_t myId  = 0;
static char     myName[8];
static bool     meshRunning = false;

static uint16_t idFromMac(const uint8_t* mac) {
  return ((uint16_t)mac[4] << 8) | mac[5];
}

uint16_t meshMyId() { return myId; }

const char* meshMyName() { return myName; }

void meshFormatId(uint16_t id, char* out, size_t n) {
  snprintf(out, n, "%04X", id);
}

#define RXQ_SIZE 16
typedef struct {
  MeshPacket pkt;
  uint8_t    senderMac[6];
  int8_t     rssi;
  uint8_t    len;
} RxItem;

static volatile RxItem   rxq[RXQ_SIZE];
static volatile uint8_t  rxHead = 0;
static volatile uint8_t  rxTail = 0;

#define SEEN_SIZE 64
typedef struct { uint16_t id; uint16_t seq; } SeenEntry;
static SeenEntry seen[SEEN_SIZE];
static uint8_t   seenPos = 0;

static bool seenBefore(uint16_t id, uint16_t seq) {
  for (int i = 0; i < SEEN_SIZE; i++) {
    if (seen[i].id == id && seen[i].seq == seq) return true;
  }
  return false;
}
static void markSeen(uint16_t id, uint16_t seq) {
  seen[seenPos].id  = id;
  seen[seenPos].seq = seq;
  seenPos = (seenPos + 1) % SEEN_SIZE;
}

static MeshNode nodes[MAX_NODES];

static MeshNode* findNode(uint16_t id) {
  for (int i = 0; i < MAX_NODES; i++)
    if (nodes[i].used && nodes[i].id == id) return &nodes[i];
  return nullptr;
}
static MeshNode* newNode() {
  for (int i = 0; i < MAX_NODES; i++)
    if (!nodes[i].used) return &nodes[i];
  MeshNode* oldest = &nodes[0];
  for (int i = 1; i < MAX_NODES; i++)
    if (nodes[i].lastSeen < oldest->lastSeen) oldest = &nodes[i];
  return oldest;
}

static void updateNode(uint16_t id, const uint8_t* mac,
                       bool direct, int8_t rssi, uint8_t hops) {
  uint32_t now = millis();
  MeshNode* n = findNode(id);
  if (!n) {
    n = newNode();
    memset(n, 0, sizeof(MeshNode));
    n->used = true;
    n->id   = id;
    n->hops = hops;
  }
  memcpy(n->mac, mac, 6);
  n->lastSeen = now;

  if (direct) {
    n->direct     = true;
    n->rssi       = rssi;
    n->hops       = 1;
    n->lastDirect = now;
  } else {

    if (now - n->lastDirect > NODE_TIMEOUT) n->direct = false;
    if (!n->direct) {
      if (hops < n->hops || n->hops == 0) n->hops = hops;
    }
  }
}

static MeshMetrics metrics;
static uint32_t ppsWindowStart = 0;
static uint16_t ppsCounter     = 0;

static uint16_t txSeq = 0;
static uint32_t lastHello = 0;

static bool     pingActive     = false;
static uint32_t pingStart      = 0;
static uint16_t pingNonce      = 0;
static uint16_t pingResponders[MAX_NODES];
static int      pingRespCount  = 0;

static bool     pongPending    = false;
static uint32_t pongDue        = 0;
static uint16_t pongNonce      = 0;

static bool     newText        = false;
static char     lastText[MESH_MAX_PAYLOAD + 1];
static uint16_t lastTextFrom   = 0;

static uint16_t lastPingFromId = 0;
static uint32_t lastPingFromAt = 0;

static MeshMsgItem msgHist[MSG_HIST_SIZE];
static int         msgHistCount = 0;
static int         msgHistHead  = 0;

static void pushHist(uint16_t from, uint16_t to, const char* text) {
  MeshMsgItem* it = &msgHist[msgHistHead];
  it->from = from;
  it->to   = to;
  strncpy(it->text, text, MESH_MAX_PAYLOAD);
  it->text[MESH_MAX_PAYLOAD] = '\0';
  it->at = millis();
  msgHistHead = (msgHistHead + 1) % MSG_HIST_SIZE;
  if (msgHistCount < MSG_HIST_SIZE) msgHistCount++;
}

static const char* CANNED_MSGS[] = {
  "Todo OK", "Necesito ayuda", "Reagrupar aqui",
  "Objetivo visto", "Retirada", "Cargando bateria"
};
static const int CANNED_COUNT = sizeof(CANNED_MSGS) / sizeof(CANNED_MSGS[0]);

int meshCannedCount() { return CANNED_COUNT; }
const char* meshCannedAt(int idx) {
  if (idx < 0 || idx >= CANNED_COUNT) return "";
  return CANNED_MSGS[idx];
}

static void rawSend(MeshPacket* p, uint8_t payloadLen) {
  uint8_t total = HEADER_SIZE + payloadLen;
  if (total > sizeof(MeshPacket)) total = sizeof(MeshPacket);
  esp_now_send(BROADCAST_MAC, (uint8_t*)p, total);
  metrics.tx++;
}

static void originate(uint8_t type, uint16_t dstId,
                      const uint8_t* payload, uint8_t payloadLen) {
  MeshPacket p;
  memset(&p, 0, sizeof(p));
  p.magic   = MESH_MAGIC;
  p.version = MESH_VERSION;
  p.type    = type;
  p.ttl     = MESH_TTL;
  p.hops    = 0;
  p.srcId   = myId;
  p.dstId   = dstId;
  p.seq     = ++txSeq;
  memcpy(p.srcMac, myMac, 6);
  if (payloadLen > MESH_MAX_PAYLOAD) payloadLen = MESH_MAX_PAYLOAD;
  p.payloadLen = payloadLen;
  if (payload && payloadLen) memcpy(p.payload, payload, payloadLen);

  markSeen(p.srcId, p.seq);
  rawSend(&p, payloadLen);
}

static void onRecv(const esp_now_recv_info_t* info,
                   const uint8_t* data, int len) {
  if (len < (int)HEADER_SIZE || len > (int)sizeof(MeshPacket)) return;
  if (data[0] != MESH_MAGIC) return;

  uint8_t next = (rxHead + 1) % RXQ_SIZE;
  if (next == rxTail) return;

  RxItem* it = (RxItem*)&rxq[rxHead];
  memcpy(&it->pkt, data, len);
  memcpy(it->senderMac, info->src_addr, 6);
  it->rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  it->len  = len;
  rxHead = next;
}

static void handleApp(const MeshPacket* p);

static void processPacket(RxItem* it) {
  MeshPacket* p = &it->pkt;

  if (p->srcId == myId) return;

  if (seenBefore(p->srcId, p->seq)) { metrics.dup++; return; }
  markSeen(p->srcId, p->seq);

  metrics.rx++;
  ppsCounter++;
  metrics.lastRssi = it->rssi;

  uint16_t senderId = idFromMac(it->senderMac);

  updateNode(senderId, it->senderMac, true, it->rssi, 1);

  if (p->srcId != senderId) {
    updateNode(p->srcId, p->srcMac, false, 0, p->hops + 1);
  }

  handleApp(p);

  if (p->ttl > 0) {
    MeshPacket fwd = *p;
    fwd.ttl--;
    fwd.hops++;
    rawSend(&fwd, fwd.payloadLen);
    metrics.relay++;
  }
}

static void handleApp(const MeshPacket* p) {
  switch (p->type) {
    case MT_HELLO:

      break;

    case MT_PING: {

      uint16_t nonce = (p->payloadLen >= 2)
                       ? ((uint16_t)p->payload[0] | ((uint16_t)p->payload[1] << 8))
                       : 0;
      pongNonce      = nonce;
      pongDue        = millis() + random(20, 220);
      pongPending    = true;
      lastPingFromId = p->srcId;
      lastPingFromAt = millis();
      break;
    }

    case MT_PONG: {
      if (pingActive) {
        uint16_t nonce = (p->payloadLen >= 2)
                         ? ((uint16_t)p->payload[0] | ((uint16_t)p->payload[1] << 8))
                         : 0;
        if (nonce == pingNonce) {

          bool known = false;
          for (int i = 0; i < pingRespCount; i++)
            if (pingResponders[i] == p->srcId) { known = true; break; }
          if (!known && pingRespCount < MAX_NODES)
            pingResponders[pingRespCount++] = p->srcId;
        }
      }
      break;
    }

    case MT_TEXT: {

      bool forMe = (p->dstId == 0xFFFF || p->dstId == myId);
      if (!forMe) break;
      uint8_t n = p->payloadLen;
      if (n > MESH_MAX_PAYLOAD) n = MESH_MAX_PAYLOAD;
      memcpy(lastText, p->payload, n);
      lastText[n]  = '\0';
      lastTextFrom = p->srcId;
      newText      = true;
      pushHist(p->srcId, p->dstId, lastText);
      break;
    }

    case MT_BUZZ: {

      bool forMe = (p->dstId == 0xFFFF || p->dstId == myId);
      if (!forMe) break;
      if (p->payloadLen >= 4) {
        uint16_t freq = (uint16_t)p->payload[0] | ((uint16_t)p->payload[1] << 8);
        uint16_t dur  = (uint16_t)p->payload[2] | ((uint16_t)p->payload[3] << 8);
        buzzerNote(freq, dur);
      }
      break;
    }
  }
}

void meshBegin() {
  if (meshRunning) return;

  memset(nodes,   0, sizeof(nodes));
  memset(seen,    0, sizeof(seen));
  memset(&metrics,0, sizeof(metrics));
  msgHistCount = 0;
  msgHistHead  = 0;
  rxHead = 0;
  rxTail = 0;
  pingActive  = false;
  pongPending = false;
  newText     = false;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(MESH_CHANNEL, WIFI_SECOND_CHAN_NONE);

  WiFi.macAddress(myMac);
  myId = idFromMac(myMac);
  if (strlen(NODE_NAME) > 0) {
    strncpy(myName, NODE_NAME, sizeof(myName) - 1);
    myName[sizeof(myName) - 1] = '\0';
  } else {
    meshFormatId(myId, myName, sizeof(myName));
  }

  randomSeed(myId ^ millis());

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("ESP-NOW init FALLO"));
    return;
  }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.channel = MESH_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  ppsWindowStart = millis();
  meshRunning = true;
  Serial.print(F("Mensajeria ESP-NOW lista. id="));
  Serial.print(myName);
  Serial.print(F(" canal="));
  Serial.println(MESH_CHANNEL);
}

void meshEnd() {
  if (!meshRunning) return;
  esp_now_deinit();
  meshRunning = false;
}

void meshLoop() {
  if (!meshRunning) return;
  uint32_t now = millis();

  while (rxTail != rxHead) {
    RxItem local;
    memcpy(&local, (const void*)&rxq[rxTail], sizeof(RxItem));
    rxTail = (rxTail + 1) % RXQ_SIZE;
    processPacket(&local);
  }

  if (now - lastHello >= HELLO_INTERVAL) {
    lastHello = now;
    originate(MT_HELLO, 0xFFFF, nullptr, 0);
  }

  if (pongPending && now >= pongDue) {
    pongPending = false;
    uint8_t pl[2] = { (uint8_t)(pongNonce & 0xFF), (uint8_t)(pongNonce >> 8) };
    originate(MT_PONG, 0xFFFF, pl, 2);
  }

  if (pingActive && now - pingStart > PING_WINDOW) {
    pingActive = false;
  }

  if (now - ppsWindowStart >= 1000) {
    metrics.pps    = ppsCounter;
    ppsCounter     = 0;
    ppsWindowStart = now;
  }
}

void meshSendText(const char* text, uint16_t dstId) {
  if (!meshRunning) return;
  uint8_t n = strlen(text);
  if (n > MESH_MAX_PAYLOAD) n = MESH_MAX_PAYLOAD;
  originate(MT_TEXT, dstId, (const uint8_t*)text, n);
  pushHist(myId, dstId, text);
}

void meshSendBuzz(uint16_t freqHz, uint16_t durMs, uint16_t dstId) {
  if (!meshRunning) return;
  uint8_t pl[4] = {
    (uint8_t)(freqHz & 0xFF), (uint8_t)(freqHz >> 8),
    (uint8_t)(durMs  & 0xFF), (uint8_t)(durMs  >> 8)
  };
  originate(MT_BUZZ, dstId, pl, 4);
}

void meshSendPing() {
  if (!meshRunning) return;
  pingNonce     = (uint16_t)random(1, 65535);
  pingActive    = true;
  pingStart     = millis();
  pingRespCount = 0;
  uint8_t pl[2] = { (uint8_t)(pingNonce & 0xFF), (uint8_t)(pingNonce >> 8) };
  originate(MT_PING, 0xFFFF, pl, 2);
}

MeshMetrics meshGetMetrics() { return metrics; }

bool meshNodeAlive(const MeshNode* n) {
  if (!n || !n->used) return false;
  return (millis() - n->lastSeen) < NODE_TIMEOUT;
}

int meshNodeCount() {
  int c = 0;
  for (int i = 0; i < MAX_NODES; i++)
    if (meshNodeAlive(&nodes[i])) c++;
  return c;
}

int meshDirectCount() {
  uint32_t now = millis();
  int c = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (meshNodeAlive(&nodes[i]) && nodes[i].direct &&
        (now - nodes[i].lastDirect) < NODE_TIMEOUT) c++;
  }
  return c;
}

const MeshNode* meshNodeAt(int idx) {
  if (idx < 0 || idx >= MAX_NODES) return nullptr;
  return &nodes[idx];
}

int  meshPingResponders() { return pingRespCount; }
uint16_t meshPingResponderId(int idx) {
  if (idx < 0 || idx >= pingRespCount) return 0;
  return pingResponders[idx];
}
bool meshPingInProgress() { return pingActive; }
uint16_t meshLastPingFrom()   { return lastPingFromId; }
uint32_t meshLastPingFromAt() { return lastPingFromAt; }

bool        meshHasNewText()   { return newText; }
const char* meshLastText()     { return lastText; }
uint16_t    meshLastTextFrom() { return lastTextFrom; }
void        meshClearNewText() { newText = false; }

int meshMsgHistCount() { return msgHistCount; }

const MeshMsgItem* meshMsgHistAt(int idx) {

  if (idx < 0 || idx >= msgHistCount) return nullptr;
  int i = (msgHistHead - 1 - idx + MSG_HIST_SIZE * 4) % MSG_HIST_SIZE;
  return &msgHist[i];
}
