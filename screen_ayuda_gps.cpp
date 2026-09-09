#include "Ayuda/screen_ayuda_gps.h"
#include "Drivers/buzzer.h"


static const unsigned char image_download_bits[] = {0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,0x7e,0x7e,0x7e,0x7e};

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

void screenAyudaGPSLoop() {
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    currentScreen = SCREEN_AYUDA;
    return;
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBM(0, 1, 16, 14, image_download_bits);

  u8g2.drawXBM(112, 1, 16, 14, image_download_bits);

  u8g2.drawBox(16, 1, 96, 14);

  u8g2.setDrawColor(2);
  u8g2.drawStr(39, 11, "AYUDA GPS");

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 24, "Lat/Lon: posicion actual");
  u8g2.drawStr(2, 34, "Alt/Vel: altura y veloc.");
  u8g2.drawStr(2, 44, "UTC/Sat: hora y # de sats");
  u8g2.drawStr(2, 54, "Sin fix: busca senal afuera");

  u8g2.drawStr(4, 62, "BACK:Volver");

  u8g2.sendBuffer();
}
