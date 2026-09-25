#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define PIN_SDA         6
#define PIN_SCL         7
#define PIN_SELECT      1
#define PIN_UP          15
#define PIN_DOWN        23
#define PIN_BACK        22
#define PIN_NEOPIXEL    11
#define PIN_BUZZER      2
#define PIN_LED1        3
#define PIN_GPSON       8
#define PIN_CD          40

#define SCREEN_W        128
#define SCREEN_H        64

#define NUM_PIXELS      9

// ==================== MENSAJERIA ESP-NOW (Msj. Preescritos / Terminal Chat) ====================
// Todas las camionetas DEBEN estar en el mismo canal WiFi para verse.
#define MESH_CHANNEL    1

// Saltos maximos que puede recorrer un mensaje antes de morir.
#define MESH_TTL        6

// Cada cuanto se anuncia "sigo viva" mientras la mensajeria esta activa (ms).
#define HELLO_INTERVAL  2000

// Tras cuanto sin oir a una camioneta se considera "caida" (ms).
#define NODE_TIMEOUT    8000

// Capacidad de la tabla de nodos conocidos.
#define MAX_NODES       24

// Tamano maximo de payload de texto (para mensajes multi-salto).
#define MESH_MAX_PAYLOAD  64

// Nombre corto opcional de esta camioneta. Si se deja "" se usa el id
// hexadecimal derivado de la MAC (unico por placa, no hay que tocar
// el codigo por unidad).
#define NODE_NAME       ""

enum Screen {
  SCREEN_SPLASH,
  SCREEN_MENU,
  SCREEN_APPS,
  SCREEN_AJUSTES,
  SCREEN_CREDITOS,
  SCREEN_AYUDA,
  SCREEN_RGBNEO,
  SCREEN_PCMODE,
  SCREEN_FRASES,
  SCREEN_DESTINOS,
  SCREEN_NOMBRES,
  SCREEN_CAPTIVE,
  SCREEN_APFLOOD,
  SCREEN_SNIFFER,
  SCREEN_BTSPAM,
  SCREEN_GPS,
  SCREEN_SDBROWSER,
  SCREEN_CONFIG,
  SCREEN_BLESCAN,
  SCREEN_AYUDA_GPS,
  SCREEN_AYUDA_SD,
  SCREEN_AYUDA_RGB,
  SCREEN_AYUDA_QR,
  SCREEN_AJUSTES_TERM,
  SCREEN_AYUDA_PCMODE,
  SCREEN_AYUDA_TERM,
  SCREEN_BTSPAM_CONFIG,
  SCREEN_REMOTE,
  SCREEN_YMODEM,
  SCREEN_MSGPRESET,
  SCREEN_MESHTERM
};

extern Screen currentScreen;

extern uint8_t neoR;
extern uint8_t neoG;
extern uint8_t neoB;

#endif
