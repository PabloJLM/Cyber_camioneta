#ifndef MESH_H
#define MESH_H

#include <Arduino.h>
#include "config.h"

// ============================================================
//  Nucleo de mensajeria ESP-NOW (flooding controlado con TTL)
//  Portado del proyecto MeshNow para usarse desde las apps
//  "Msj. Preescritos" y "Terminal Chat" de camioneta.
//
//  Idea: no hay tabla de rutas. Cada paquete lleva un TTL y un
//  numero de secuencia unico por origen. Cuando un nodo recibe
//  un paquete que NO ha visto, lo procesa y, si TTL>0, lo vuelve
//  a emitir en broadcast (reenvio). Un cache anti-duplicados
//  evita que los paquetes den vueltas para siempre.
//
//  A diferencia de MeshNow, aqui no hay flag IS_MASTER: cualquier
//  camioneta puede mandar y recibir por igual. Tampoco corre todo
//  el tiempo -- lo prende/apaga cada app (meshBegin/meshEnd) para
//  no chocar con el WiFi que usan las otras apps (Captive Portal,
//  AP Flood, Con. Remota, etc).
// ============================================================

// Tipos de paquete.
enum MeshType : uint8_t {
  MT_HELLO = 1,   // "sigo vivo" (mantiene la tabla de nodos)
  MT_PING  = 2,   // barrido: pide a todos que respondan
  MT_PONG  = 3,   // respuesta a un ping
  MT_TEXT  = 4,   // mensaje (prehecho o custom), a todos o a uno
  MT_BUZZ  = 5    // hace sonar el buzzer de otro nodo (freq+dur)
};

// Estructura del paquete que viaja por el aire. __packed__ para
// que ocupe exactamente lo mismo en todas las placas.
typedef struct __attribute__((packed)) {
  uint8_t  magic;                       // 0xA5, identifica nuestros paquetes
  uint8_t  version;                     // version de protocolo
  uint8_t  type;                        // MeshType
  uint8_t  ttl;                         // saltos que le quedan
  uint8_t  hops;                        // saltos ya recorridos
  uint16_t srcId;                       // id del origen (16 bits, deriva de MAC)
  uint16_t dstId;                       // 0xFFFF = todos, si no = id especifico
  uint16_t seq;                         // secuencia por origen (anti-duplicado)
  uint8_t  srcMac[6];                   // MAC del origen
  uint8_t  payloadLen;                  // bytes utiles en payload
  uint8_t  payload[MESH_MAX_PAYLOAD];   // datos (texto, etc.)
} MeshPacket;

// Entrada de la tabla de nodos conocidos.
typedef struct {
  bool     used;
  uint16_t id;
  uint8_t  mac[6];
  int8_t   rssi;       // RSSI del ultimo salto directo (valido si direct)
  uint8_t  hops;       // saltos hasta ese nodo (1 = vecino directo)
  bool     direct;     // true si lo oimos directamente (y reciente)
  uint32_t lastSeen;   // millis() de cualquier noticia suya (directa o por salto)
  uint32_t lastDirect; // millis() de la ultima vez que lo oimos DIRECTO
} MeshNode;

// Item de historial de mensajes para la pantalla de Msj. Preescritos.
typedef struct {
  uint16_t from;
  uint16_t to;                           // 0xFFFF = fue a todos
  char     text[MESH_MAX_PAYLOAD + 1];
  uint32_t at;
} MeshMsgItem;
#define MSG_HIST_SIZE 8

// Metricas en vivo.
typedef struct {
  uint32_t tx;         // paquetes que originamos + reenviamos
  uint32_t rx;         // paquetes validos recibidos
  uint32_t relay;      // paquetes reenviados (multi-salto)
  uint32_t dup;        // duplicados descartados
  uint16_t pps;        // paquetes por segundo (rx)
  int8_t   lastRssi;   // RSSI del ultimo paquete oido
} MeshMetrics;

// ---- API publica ----
void        meshBegin();                        // init ESP-NOW (WiFi STA + canal fijo)
void        meshEnd();                           // apaga ESP-NOW, libera el WiFi para otras apps
void        meshLoop();                          // procesar cola + housekeeping
uint16_t    meshMyId();                          // id propio (16 bits)
const char* meshMyName();                        // nombre corto propio
void        meshFormatId(uint16_t id, char* out, size_t n); // id -> texto

// dstId: 0xFFFF (por defecto) = a todos; o el id de un nodo especifico.
void        meshSendText(const char* text, uint16_t dstId = 0xFFFF);
void        meshSendPing();                       // lanzar barrido de activos

// Suena el BUZZER de dstId (no el propio). dstId puede ser 0xFFFF
// para que suenen todos a la vez.
void        meshSendBuzz(uint16_t freqHz, uint16_t durMs, uint16_t dstId);

// Catalogo de mensajes prehechos (mismo catalogo para las dos apps).
int         meshCannedCount();
const char* meshCannedAt(int idx);

// Consultas para la UI:
MeshMetrics meshGetMetrics();
int         meshNodeCount();                      // nodos vivos conocidos (total)
int         meshDirectCount();                    // vecinos directos vivos
const MeshNode* meshNodeAt(int idx);              // acceso a la tabla (puede ser inactivo)
bool        meshNodeAlive(const MeshNode* n);     // vivo segun NODE_TIMEOUT

// Resultado del ultimo ping:
int         meshPingResponders();                 // cuantos respondieron al ultimo ping
uint16_t    meshPingResponderId(int idx);          // id del respondedor idx
bool        meshPingInProgress();
uint16_t    meshLastPingFrom();                    // quien nos hizo ping a nosotros
uint32_t    meshLastPingFromAt();                  // millis() de ese ping

// Ultimo texto recibido para MI (para avisos en pantalla):
bool        meshHasNewText();                     // hay texto sin leer
const char* meshLastText();                       // contenido
uint16_t    meshLastTextFrom();                    // id del emisor
void        meshClearNewText();

// Historial de mensajes (los que mando yo + los que recibi para mi/todos).
int                 meshMsgHistCount();
const MeshMsgItem*  meshMsgHistAt(int idx);        // idx=0 -> el mas reciente

#endif
