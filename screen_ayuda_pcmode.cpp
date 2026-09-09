#include "Ayuda/screen_ayuda_pcmode.h"
#include "Drivers/buzzer.h"

static const unsigned char image_download_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};


static const int TOTAL_PAGES = 4;// numero de paginas de ayuda 
static const char* PAGES[TOTAL_PAGES][4] = {
  {
    "help   - esta ayuda",
    "status - estado sistema",
    "info   - info del device",
    "rgb/R/G/B - color led"
  },
  {
    "buzzer - prueba buzzer",
    "piano  - mini piano serial",
    "ls/cat - listar/ver SD",
    "flood{...} - SSIDs AP Flood"
  },
  {
    "logcap   - ult. 5 logs portal",
    "logfull  - todos los logs",
    "logclear - borrar log",
    "logstats - estadisticas"
  },
  {
    "clear - limpiar pantalla",
    "exit  - salir de PC-Mode",
    "",
    ""
  }
};

static int page = 0;

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

void screenAyudaPCModeLoop() {
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    page = 0;
    currentScreen = SCREEN_AYUDA;
    return;
  }

  if (isButtonJustPressed(PIN_UP)) {
    buzzerClick();
    page--;
    if (page < 0) page = TOTAL_PAGES - 1;
  }

  if (isButtonJustPressed(PIN_DOWN)) {
    buzzerClick();
    page++;
    if (page >= TOTAL_PAGES) page = 0;
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_6x10_tr);
  char title[24];
  snprintf(title, sizeof(title), "PC-Mode (%d/%d)", page + 1, TOTAL_PAGES);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(64 - u8g2.getStrWidth(title) / 2, 11, title);
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_download_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_download_bits);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 24, PAGES[page][0]);
  u8g2.drawStr(2, 34, PAGES[page][1]);
  u8g2.drawStr(2, 44, PAGES[page][2]);
  u8g2.drawStr(2, 54, PAGES[page][3]);

  u8g2.drawStr(2, 62, "UP/DN:Pag  BACK:Volver");

  u8g2.sendBuffer();
}
