#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "Drivers/neopixel.h"
#include "Drivers/buzzer.h"
#include "screen_splash.h"
#include "screen_menu.h"
#include "screen_apps.h"
#include "screen_ajustes.h"
#include "screen_creditos.h"
#include "screen_ayuda.h"
#include "screen_rgbneo.h"
#include "screen_pcmode.h"
#include "screen_captive.h"  // Nueva pantalla

// Pantalla 
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);

// Neopixeles
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Variables globales para RGB NeoPixel control
uint8_t neoR = 128;
uint8_t neoG = 0;
uint8_t neoB = 255;

// siempre iniciar en splash
Screen currentScreen = SCREEN_SPLASH;

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_SELECT, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BACK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  
  neopixelInit();
  
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
      
    case SCREEN_CAPTIVE:  // Nueva pantalla
      screenCaptiveLoop();
      break;
  }
  
  delay(10);
}
