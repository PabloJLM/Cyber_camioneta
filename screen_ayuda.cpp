#include "Ayuda/screen_ayuda.h"
#include "Drivers/buzzer.h"
static const unsigned char image_download_1_bits[] = {0x01,0x03,0x07,0x0f,0x07,0x03,0x01};
static const unsigned char image_download_bits[] = {0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,0x7e,0x7e,0x7e,0x7e};
static const unsigned char image_Quest_bits[] = {0xfc,0x03,0xfc,0x03,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x00,0x0f,0x00,0x0f,0xc0,0x03,0xc0,0x03,0xf0,0x00,0xf0,0x00,0x00,0x00,0x00,0x00,0xf0,0x00,0xf0,0x00};


static const char* AYUDA_ITEMS[] = {
  "Ayuda GPS", "Ayuda SD", "Ayuda RGB", "Codigo QR", "PC-Mode", "Term. Ajustes"
};
static const int TOTAL_AYUDA   = 6;
static const int VISIBLE_ITEMS = 4;    
static const int ITEM_Y0   = 24;      
static const int ITEM_STEP = 12;       

static int ayudaSelection = 0;
static int scrollOffset   = 0;         

static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t lastReading[4]     = {HIGH, HIGH, HIGH, HIGH};
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};

  const int pins[4] = {PIN_SELECT, PIN_UP, PIN_DOWN, PIN_BACK};
  int index = -1;

  for (int i = 0; i < 4; i++) {
    if (pins[i] == pin) {
      index = i;
      break;
    }
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


static void clampScroll() {
  if (ayudaSelection < scrollOffset) {
    scrollOffset = ayudaSelection;
  } else if (ayudaSelection >= scrollOffset + VISIBLE_ITEMS) {
    scrollOffset = ayudaSelection - VISIBLE_ITEMS + 1;
  }
}

void screenAyudaLoop(){
  if (isButtonJustPressed(PIN_BACK)) {
    currentScreen = SCREEN_MENU;
    buzzerClick();
    return;
  }

  if (isButtonJustPressed(PIN_UP)) {
    buzzerClick();
    ayudaSelection--;
    if (ayudaSelection < 0) ayudaSelection = TOTAL_AYUDA - 1;
    clampScroll();
  }

  if (isButtonJustPressed(PIN_DOWN)) {
    buzzerClick();
    ayudaSelection++;
    if (ayudaSelection >= TOTAL_AYUDA) ayudaSelection = 0;
    clampScroll();
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    switch (ayudaSelection) {
      case 0: currentScreen = SCREEN_AYUDA_GPS;    break;
      case 1: currentScreen = SCREEN_AYUDA_SD;     break;
      case 2: currentScreen = SCREEN_AYUDA_RGB;    break;
      case 3: currentScreen = SCREEN_AYUDA_QR;     break;
      case 4: currentScreen = SCREEN_AYUDA_PCMODE; break;
      case 5: currentScreen = SCREEN_AYUDA_TERM;   break;
    }
    return;
  }

  clampScroll();

  u8g2.clearBuffer();

  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.drawXBM(0, 1, 16, 14, image_download_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_download_bits);
  u8g2.drawBox(16, 1, 96, 14);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(2);
  u8g2.drawStr(49, 11, "AYUDA");
  u8g2.setDrawColor(1);


  for (int row = 0; row < VISIBLE_ITEMS; row++) {
    int i = scrollOffset + row;
    if (i >= TOTAL_AYUDA) break;
    u8g2.drawStr(35, ITEM_Y0 + ITEM_STEP * row, AYUDA_ITEMS[i]);
  }


  int cursorRow = ayudaSelection - scrollOffset;
  u8g2.drawXBM(26, ITEM_Y0 + ITEM_STEP * cursorRow - 7, 4, 7, image_download_1_bits);

  if (scrollOffset > 0) {
    u8g2.drawStr(18, 20, "^");
  }
  if (scrollOffset + VISIBLE_ITEMS < TOTAL_AYUDA) {
    u8g2.drawStr(18, 63, "v");
  }

  u8g2.drawXBM(109, 44, 14, 16, image_Quest_bits);

  u8g2.sendBuffer();
}
