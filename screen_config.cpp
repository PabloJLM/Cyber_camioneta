#include "Ajustes/screen_config.h"
#include "Drivers/buzzer.h"
#include "Drivers/neopixel.h"
#include "Drivers/settings.h"
#include "Estaticos/splash_bitmaps.h"
#include <string.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static const unsigned char image_arrow_bits[] PROGMEM = {
  0x01,0x03,0x07,0x0f,0x07,0x03,0x01
};

enum ConfigMode { CFG_LIST, CFG_SPLASH, CFG_BREATH, CFG_BRIGHTNESS, CFG_RESET_CONFIRM, CFG_SAVED };

static const char* BRIGHTNESS_NAMES[]  = { "Bajo", "Medio", "Alto" };
static const uint8_t BRIGHTNESS_VALUES[] = { 60, 150, 255 };
static const uint8_t BRIGHTNESS_COUNT = 3;

static const char* MENU_ITEMS[] = { "Splash", "Color Breath", "Brillo", "Reset de fabrica" };
static const uint8_t MENU_COUNT = 4;

static const uint8_t VISIBLE_ITEMS = 3;

static ConfigMode mode = CFG_LIST;
static int listSel = 0;
static int editSel = 0;
static unsigned long savedFlashUntil = 0;
static char savedMsg[24] = "";

static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t lastReading[4]     = {HIGH, HIGH, HIGH, HIGH};
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};

  const int pins[4] = {PIN_SELECT, PIN_UP, PIN_DOWN, PIN_BACK};
  int index = -1;

  for (int i = 0; i < 4; i++) {
    if (pins[i] == pin) { index = i; break; }
  }
  if (index == -1) return false;

  uint8_t reading = digitalRead(pin);

  if (reading != lastReading[index]) {
    lastDebounceTime[index] = millis();
    lastReading[index] = reading;
  }

  if ((millis() - lastDebounceTime[index]) > 50) {
    if (lastStableState[index] == HIGH && reading == LOW) {
      lastStableState[index] = reading;
      return true;
    }
    lastStableState[index] = reading;
  }
  return false;
}

static void enterEdit(ConfigMode m, int currentValue) {
  mode = m;
  editSel = currentValue;
}

static void confirmSave() {
  switch (mode) {
    case CFG_SPLASH:
      settingsSetSplashIndex((uint8_t)editSel);
      strcpy(savedMsg, "Splash guardado");
      break;
    case CFG_BREATH:
      settingsSetBreathColorIndex((uint8_t)editSel);
      strcpy(savedMsg, "Color guardado");
      break;
    case CFG_BRIGHTNESS:
      settingsSetBrightness(BRIGHTNESS_VALUES[editSel]);
      neopixelSetBrightness(BRIGHTNESS_VALUES[editSel]);
      strcpy(savedMsg, "Brillo guardado");
      break;
    default:
      break;
  }
  mode = CFG_SAVED;
  savedFlashUntil = millis() + 700;
}

void screenConfigLoop() {
  if (mode == CFG_SAVED && millis() >= savedFlashUntil) {
    mode = CFG_LIST;
  }

  if (mode == CFG_LIST) {
    if (isButtonJustPressed(PIN_UP))   { listSel = (listSel - 1 + MENU_COUNT) % MENU_COUNT; buzzerClick(); }
    if (isButtonJustPressed(PIN_DOWN)) { listSel = (listSel + 1) % MENU_COUNT; buzzerClick(); }
    if (isButtonJustPressed(PIN_BACK)) { buzzerClick(); currentScreen = SCREEN_AJUSTES; return; }
    if (isButtonJustPressed(PIN_SELECT)) {
      buzzerBeep();
      switch (listSel) {
        case 0: {
          uint8_t cur = settingsGetSplashIndex();
          if (cur >= SPLASH_COUNT) cur = 0;
          enterEdit(CFG_SPLASH, cur);
          break;
        }
        case 1: {
          uint8_t cur = settingsGetBreathColorIndex();
          if (cur >= BREATH_PRESET_COUNT) cur = 0;
          enterEdit(CFG_BREATH, cur);
          break;
        }
        case 2: {
          uint8_t stored = settingsGetBrightness();
          int idx = 1;  // "Medio" por defecto si no matchea ningun preset
          for (int i = 0; i < BRIGHTNESS_COUNT; i++) {
            if (BRIGHTNESS_VALUES[i] == stored) { idx = i; break; }
          }
          enterEdit(CFG_BRIGHTNESS, idx);
          break;
        }
        case 3:
          mode = CFG_RESET_CONFIRM;
          break;
      }
    }
  } else if (mode == CFG_SPLASH || mode == CFG_BREATH || mode == CFG_BRIGHTNESS) {
    uint8_t count = (mode == CFG_SPLASH) ? SPLASH_COUNT
                  : (mode == CFG_BREATH) ? BREATH_PRESET_COUNT
                  : BRIGHTNESS_COUNT;
    if (isButtonJustPressed(PIN_UP))   { editSel = (editSel - 1 + count) % count; buzzerClick(); }
    if (isButtonJustPressed(PIN_DOWN)) { editSel = (editSel + 1) % count; buzzerClick(); }
    if (isButtonJustPressed(PIN_BACK)) { buzzerClick(); mode = CFG_LIST; return; }
    if (isButtonJustPressed(PIN_SELECT)) { confirmSave(); return; }
  } else if (mode == CFG_RESET_CONFIRM) {
    if (isButtonJustPressed(PIN_BACK)) { buzzerClick(); mode = CFG_LIST; return; }
    if (isButtonJustPressed(PIN_SELECT)) {
      buzzerBeep();
      settingsFactoryReset();
      neopixelSetBrightness(BRIGHTNESS_VALUES[1]);  // vuelve a "Medio"
      strcpy(savedMsg, "Config. reiniciada");
      mode = CFG_SAVED;
      savedFlashUntil = millis() + 900;
    }
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(25, 11, "Configuracion");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  if (mode == CFG_LIST) {
    int scrollTop = listSel - (VISIBLE_ITEMS - 1);
    if (scrollTop < 0) scrollTop = 0;
    int maxTop = MENU_COUNT - VISIBLE_ITEMS;
    if (maxTop < 0) maxTop = 0;
    if (scrollTop > listSel) scrollTop = listSel;
    if (scrollTop > maxTop) scrollTop = maxTop;

    const int rowSpacing = 13;
    const int firstY = 25;

    for (int row = 0; row < VISIBLE_ITEMS; row++) {
      int i = scrollTop + row;
      if (i >= MENU_COUNT) break;
      int yy = firstY + row * rowSpacing;
      if (i == listSel) {
        u8g2.drawXBM(2, yy - 6, 4, 7, image_arrow_bits);
      }
      u8g2.drawStr(12, yy, MENU_ITEMS[i]);
    }

    if (scrollTop > 0) {
      u8g2.drawTriangle(122, 20, 126, 20, 124, 17);
    }
    if (scrollTop + VISIBLE_ITEMS < MENU_COUNT) {
      u8g2.drawTriangle(122, 54, 126, 54, 124, 57);
    }

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 62, "SEL:Entrar BACK:Salir");
  } else if (mode == CFG_SPLASH) {
    u8g2.drawStr(4, 26, "Splash:");
    u8g2.drawStr(4, 40, SPLASH_LIST[editSel].name);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 55, "UP/DOWN:Cambiar");
    u8g2.drawStr(2, 63, "SEL:Guardar BACK:Cancela");
  } else if (mode == CFG_BREATH) {
    u8g2.drawStr(4, 26, "Color breath:");
    u8g2.drawStr(4, 40, BREATH_PRESETS[editSel].name);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 55, "UP/DOWN:Cambiar");
    u8g2.drawStr(2, 63, "SEL:Guardar BACK:Cancela");
  } else if (mode == CFG_BRIGHTNESS) {
    u8g2.drawStr(4, 26, "Brillo:");
    u8g2.drawStr(4, 40, BRIGHTNESS_NAMES[editSel]);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 55, "UP/DOWN:Cambiar");
    u8g2.drawStr(2, 63, "SEL:Guardar BACK:Cancela");
  } else if (mode == CFG_RESET_CONFIRM) {
    u8g2.drawStr(4, 28, "Borrar toda la");
    u8g2.drawStr(4, 40, "configuracion?");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 55, "SEL:Si  BACK:No");
  } else if (mode == CFG_SAVED) {
    u8g2.drawStr(8, 36, savedMsg);
  }

  u8g2.sendBuffer();
}
