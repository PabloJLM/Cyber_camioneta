#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== PINES ====================
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

// ==================== PANTALLA ====================
#define SCREEN_W        128
#define SCREEN_H        64

// ==================== NEOPIXELES ====================
#define NUM_PIXELS      9

// ==================== ESTADOS ====================
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
  SCREEN_CAPTIVE,  // Nueva pantalla para captive portal
  SCREEN_APFLOOD,  // Beacon flood
  SCREEN_SNIFFER,  // Sniffer de paquetes (PCAP)
  SCREEN_BTSPAM,   // Publicidad por BLE advertising
  SCREEN_GPS,      // GPS Position (lee NMEA del ATGM336H-6N-74 por UART0)
  SCREEN_AYUDA_GPS,
  SCREEN_AYUDA_SD,
  SCREEN_AYUDA_RGB,
  SCREEN_AYUDA_QR,
  SCREEN_AJUSTES_TERM,  // Terminal serial exclusiva de Ajustes (flood, sniffer, portal)
  SCREEN_AYUDA_PCMODE,  // Ayuda: comandos de PC-Mode (Apps)
  SCREEN_AYUDA_TERM     // Ayuda: comandos de la terminal de Ajustes
};

extern Screen currentScreen;

// ==================== VARIABLES RGB NEOPIXEL ====================
extern uint8_t neoR;
extern uint8_t neoG;
extern uint8_t neoB;

#endif
