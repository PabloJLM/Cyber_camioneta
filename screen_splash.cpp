#include "Estaticos/screen_splash.h"
#include "Estaticos/splash_bitmaps.h"
#include "Drivers/neopixel.h"
#include "Drivers/buzzer.h"
#include "Drivers/settings.h"

static bool anyButtonPressed() {
  return !digitalRead(PIN_SELECT) ||
         !digitalRead(PIN_UP)     ||
         !digitalRead(PIN_DOWN)   ||
         !digitalRead(PIN_BACK);
}

void screenSplashLoop() {
  static uint8_t splashIdx = 255;  // 255 = todavia no leido de NVS
  if (splashIdx == 255) {
    splashIdx = settingsGetSplashIndex();
    if (splashIdx >= SPLASH_COUNT) splashIdx = 0;
  }
  const SplashBitmap &splash = SPLASH_LIST[splashIdx];

  static uint8_t breathIdx = 255;
  if (breathIdx == 255) {
    breathIdx = settingsGetBreathColorIndex();
    if (breathIdx >= BREATH_PRESET_COUNT) breathIdx = 0;
  }
  const BreathColor &breath = BREATH_PRESETS[breathIdx];

 
  bool bounceVertical = (splash.h <= 45);

  static int x = (SCREEN_W - splash.w) / 2;
  static int y = (SCREEN_H - splash.h) / 2;
  static float vx = 1.5;//velocidad jsjs
  static float vy = bounceVertical ? 1.5 : 0;
  static int currentPixel = 0;
  static unsigned long lastPixelTime = 0;
  static uint8_t brightness = 50;
  static int8_t direction = 1;
  static unsigned long lastBreatheTime = 0;


  x += vx;
  if (bounceVertical) y += vy;

  if (x <= 0 || x >= SCREEN_W - splash.w) vx = -vx;
  if (bounceVertical && (y <= 0 || y >= SCREEN_H - splash.h)) vy = -vy;

  u8g2.clearBuffer();
  u8g2.setBitmapMode(1);
  u8g2.drawXBM(x, y, splash.w, splash.h, splash.bits);
  u8g2.sendBuffer();


  if (currentPixel < NUM_PIXELS) {
    neopixelSplashSequence(currentPixel, lastPixelTime, breath.r, breath.g, breath.b);
  } else {
    neopixelBreathe(brightness, direction, lastBreatheTime, breath.r, breath.g, breath.b);
  }


  if (anyButtonPressed()) {
    neopixelFadeOut(breath.r, breath.g, breath.b);
    buzzerWelcome();
    currentScreen = SCREEN_MENU;
    delay(200);
  }
}
