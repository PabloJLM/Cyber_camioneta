#include "Ayuda/screen_ayuda_rgb.h"
#include "Drivers/buzzer.h"

// Pantalla de ayuda: texto plano, edita las cadenas de drawStr como
// quieras (o abri el proyecto en u8g2 Studio y editalas ahi mismo).

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

void screenAyudaRGBLoop() {
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    currentScreen = SCREEN_AYUDA;
    return;
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(38, 10, "Ayuda RGB");
  u8g2.drawLine(0, 12, 127, 12);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 24, "Texto de ejemplo linea 1");
  u8g2.drawStr(2, 34, "Texto de ejemplo linea 2");
  u8g2.drawStr(2, 44, "Texto de ejemplo linea 3");
  u8g2.drawStr(2, 54, "Texto de ejemplo linea 4");

  u8g2.drawStr(6, 62, "BACK:Volver");

  u8g2.sendBuffer();
}
