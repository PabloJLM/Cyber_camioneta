#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "Drivers/neopixel.h"
#include "Drivers/buzzer.h"
#include "Estaticos/screen_splash.h"
#include "Apps/screen_menu.h"
#include "Apps/screen_apps.h"
#include "Ajustes/screen_ajustes.h"
#include "Estaticos/screen_creditos.h"
#include "Ayuda/screen_ayuda.h"
#include "Ayuda/screen_ayuda_gps.h"
#include "Ayuda/screen_ayuda_sd.h"
#include "Ayuda/screen_ayuda_rgb.h"
#include "Ayuda/screen_ayuda_qr.h"
#include "Ayuda/screen_ayuda_pcmode.h"
#include "Ayuda/screen_ayuda_term.h"
#include "Ajustes/screen_rgbneo.h"
#include "Ajustes/screen_ajustes_term.h"
#include "Apps/screen_pcmode.h"
#include "Apps/screen_captive.h"
#include "Apps/screen_apflood.h"
#include "Apps/screen_sniffer.h"
#include "Apps/screen_btspam.h"
#include "Apps/screen_gps.h"
#include "Ajustes/screen_sdbrowser.h"
#include "Ajustes/screen_config.h"
#include "Drivers/settings.h"
#include "Apps/screen_blescan.h"
#include "Ajustes/screen_btspam_config.h"
#include "Ajustes/screen_remote.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);

Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

uint8_t neoR = 128;
uint8_t neoG = 0;
uint8_t neoB = 255;

Screen currentScreen = SCREEN_SPLASH;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_SELECT, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BACK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  neopixelInit();
  neopixelSetBrightness(settingsGetBrightness());

  u8g2.begin();
}

void loop() {
  switch(currentScreen) {
    case SCREEN_SPLASH:
      screenSplashLoop();
      break;

    case SCREEN_MENU:
      screenMenuLoop();
      break;

    case SCREEN_APPS:
      screenAppsLoop();
      break;

    case SCREEN_AJUSTES:
      screenAjustesLoop();
      break;

    case SCREEN_CREDITOS:
      screenCreditosLoop();
      break;

    case SCREEN_AYUDA:
      screenAyudaLoop();
      break;

    case SCREEN_RGBNEO:
      screenRGBNeoLoop();
      break;

    case SCREEN_PCMODE:
      screenPCModeLoop();
      break;

    case SCREEN_CAPTIVE:
      screenCaptiveLoop();
      break;

    case SCREEN_APFLOOD:
      screenApFloodLoop();
      break;

    case SCREEN_SNIFFER:
      screenSnifferLoop();
      break;

    case SCREEN_BTSPAM:
      screenBtSpamLoop();
      break;

    case SCREEN_GPS:
      screenGPSLoop();
      break;

    case SCREEN_SDBROWSER:
      screenSDBrowserLoop();
      break;

    case SCREEN_CONFIG:
      screenConfigLoop();
      break;

    case SCREEN_BLESCAN:
      screenBleScanLoop();
      break;

    case SCREEN_AYUDA_GPS:
      screenAyudaGPSLoop();
      break;

    case SCREEN_AYUDA_SD:
      screenAyudaSDLoop();
      break;

    case SCREEN_AYUDA_RGB:
      screenAyudaRGBLoop();
      break;

    case SCREEN_AYUDA_QR:
      screenAyudaQRLoop();
      break;

    case SCREEN_AJUSTES_TERM:
      screenAjustesTermLoop();
      break;

    case SCREEN_AYUDA_PCMODE:
      screenAyudaPCModeLoop();
      break;

    case SCREEN_AYUDA_TERM:
      screenAyudaTermLoop();
      break;

    case SCREEN_BTSPAM_CONFIG:
      screenBtSpamConfigLoop();
      break;

    case SCREEN_REMOTE:
      screenRemoteLoop();
      break;
  }

  delay(10);
}
