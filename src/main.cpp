#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "neopixel.h"
#include "buzzer.h"
#include "mesh.h"
#include "screen_splash.h"
#include "screen_menu.h"
#include "screen_apps.h"
#include "screen_ajustes.h"
#include "screen_creditos.h"
#include "screen_ayuda.h"
#include "screen_ayuda_gps.h"
#include "screen_ayuda_sd.h"
#include "screen_ayuda_rgb.h"
#include "screen_ayuda_qr.h"
#include "screen_ayuda_pcmode.h"
#include "screen_ayuda_term.h"
#include "screen_rgbneo.h"
#include "screen_ajustes_term.h"
#include "screen_pcmode.h"
#include "screen_captive.h"
#include "screen_apflood.h"
#include "screen_sniffer.h"
#include "screen_btspam.h"
#include "screen_gps.h"
#include "screen_sdbrowser.h"
#include "screen_config.h"
#include "settings.h"
#include "screen_blescan.h"
#include "screen_btspam_config.h"
#include "screen_remote.h"
#include "screen_ymodem.h"
#include "screen_msgpreset.h"
#include "screen_meshterm.h"
#include "screen_riscvalu.h"
#include "screen_aes.h"

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

    case SCREEN_YMODEM:
      screenYmodemLoop();
      break;

    case SCREEN_MSGPRESET:
      screenMsgPresetLoop();
      break;

    case SCREEN_MESHTERM:
      screenMeshTermLoop();
      break;

    case SCREEN_RISCVALU:
      screenRiscvAluLoop();
      break;

    case SCREEN_AESDEMO:
      screenAesLoop();
      break;
  }

  delay(10);
}
