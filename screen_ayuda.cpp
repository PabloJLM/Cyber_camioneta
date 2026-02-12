#include "screen_ayuda.h"
#include "Drivers/buzzer.h"
static const unsigned char image_download_1_bits[] = {0x01,0x03,0x07,0x0f,0x07,0x03,0x01};
static const unsigned char image_download_bits[] = {0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,0x7e,0x7e,0x7e,0x7e};
static const unsigned char image_Quest_bits[] = {0xfc,0x03,0xfc,0x03,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x00,0x0f,0x00,0x0f,0xc0,0x03,0xc0,0x03,0xf0,0x00,0xf0,0x00,0x00,0x00,0x00,0x00,0xf0,0x00,0xf0,0x00};


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

void screenAyudaLoop(){
  if (isButtonJustPressed(PIN_BACK)) {
    currentScreen = SCREEN_MENU;
    buzzerClick();
    return;
  }
  u8g2.clearBuffer();

  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(42, 28, "");

  u8g2.drawStr(35, 36, "Ayuda SD");

  u8g2.drawStr(35, 24, "Ayuda GPS");

  u8g2.drawXBM(0, 1, 16, 14, image_download_bits);

  u8g2.drawXBM(112, 1, 16, 14, image_download_bits);

  u8g2.drawBox(16, 1, 96, 14);

  u8g2.setDrawColor(2);
  u8g2.drawStr(49, 11, "AYUDA");

  u8g2.setDrawColor(1);
  u8g2.drawXBM(26, 17, 4, 7, image_download_1_bits);

  u8g2.drawXBM(26, 29, 4, 7, image_download_1_bits);

  u8g2.drawXBM(26, 41, 4, 7, image_download_1_bits);

  u8g2.drawStr(35, 48, "Ayuda RGB");

  u8g2.drawStr(35, 60, "Codigo QR");

  u8g2.drawXBM(26, 53, 4, 7, image_download_1_bits);

  u8g2.drawXBM(109, 44, 14, 16, image_Quest_bits);

  u8g2.sendBuffer();


}