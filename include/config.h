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
#define PIN_CD          18   
#define PIN_SD_SCK      21
#define PIN_SD_MISO     20
#define PIN_SD_MOSI     19

#define SCREEN_W        128
#define SCREEN_H        64

#define NUM_PIXELS      9

#define MESH_CHANNEL    1

#define MESH_TTL        6

#define HELLO_INTERVAL  2000

#define NODE_TIMEOUT    8000

#define MAX_NODES       24

#define MESH_MAX_PAYLOAD  64

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
  SCREEN_MSGPRESET,
  SCREEN_MESHTERM,
  SCREEN_RISCVALU,
  SCREEN_AESDEMO
};

extern Screen currentScreen;

extern uint8_t neoR;
extern uint8_t neoG;
extern uint8_t neoB;

#endif
