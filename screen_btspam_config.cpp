#include "Ajustes/screen_btspam_config.h"
#include "Drivers/buzzer.h"
#include "Apps/screen_btspam.h"

// Pantalla de checkbox: elegir que fabricantes rota el BT Spam
// (Apps -> Bluetooth Spam). Lee y escribe directo via la API publica
// de screen_btspam.h/.cpp, que a su vez guarda la mascara en NVS
// (Drivers/settings.h) -- asi el estado sobrevive un reinicio.

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static const unsigned char image_arrow_bits[] PROGMEM = {
  0x01,0x03,0x07,0x0f,0x07,0x03,0x01
};

static int selection = 0;

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

void screenBtSpamConfigLoop() {
  int total = btSpamGetTypeCount();

  if (isButtonJustPressed(PIN_UP)) {
    buzzerClick();
    selection--;
    if (selection < 0) selection = total - 1;
  }

  if (isButtonJustPressed(PIN_DOWN)) {
    buzzerClick();
    selection++;
    if (selection >= total) selection = 0;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    bool enabled = btSpamIsTypeEnabled(selection);
    btSpamSetTypeEnabled(selection, !enabled);
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    selection = 0;
    currentScreen = SCREEN_AJUSTES;
    return;
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(38, 11, "BT SPAM");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  u8g2.setFont(u8g2_font_5x7_tr);
  int y = 23;
  for (int i = 0; i < total; i++) {
    if (i == selection) {
      u8g2.drawXBM(2, y - 6, 4, 7, image_arrow_bits);
    }

    char line[28];
    const char* mark = btSpamIsTypeEnabled(i) ? "x" : " ";
    const char* warn = btSpamIsTypeUnstable(i) ? " !" : "";
    snprintf(line, sizeof(line), "[%s] %s%s", mark, btSpamGetTypeName(i), warn);
    u8g2.drawStr(10, y, line);

    y += 8;
  }

  u8g2.drawStr(2, 62, "! = puede reiniciar el equipo");

  u8g2.sendBuffer();
}
