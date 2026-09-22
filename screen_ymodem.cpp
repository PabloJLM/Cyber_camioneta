#include "Apps/screen_ymodem.h"
#include "Drivers/buzzer.h"
#include "Drivers/ymodem.h"
#include <SD.h>
#include <SPI.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static const unsigned char image_arrow_bits[] PROGMEM = {
  0x01,0x03,0x07,0x0f,0x07,0x03,0x01
};

static const int MAX_FILES = 30;
static char fileNames[MAX_FILES][32];
static int fileCount = 0;
static int selection = 0;
static bool listLoaded = false;
static bool showingStatus = false;
static String statusMsg = "";

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

static void loadFileList() {
  fileCount = 0;
  if (!SD.begin(PIN_CD)) return;

  File root = SD.open("/");
  if (!root) return;

  File entry = root.openNextFile();
  while (entry && fileCount < MAX_FILES) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      name.toCharArray(fileNames[fileCount], sizeof(fileNames[fileCount]));
      fileCount++;
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

static void drawHeader(const char* title) {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  int tw = u8g2.getStrWidth(title);
  int x = 16 + (96 - tw) / 2;
  u8g2.drawStr(x, 11, title);
  u8g2.setDrawColor(1);
  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);
}

static void doSend(int idx) {
  String path = "/";
  path += fileNames[idx];

  u8g2.clearBuffer();
  drawHeader("YModem");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(4, 30, "Enviando por YMODEM...");
  u8g2.drawStr(4, 42, "Inicia la recepcion");
  u8g2.drawStr(4, 52, "en tu PC ahora.");
  u8g2.sendBuffer();

  bool ok = ymodemSendFile(SD, path);
  statusMsg = ok ? "OK: enviado" : "Error al enviar";
  showingStatus = true;
}

static void doReceive() {
  u8g2.clearBuffer();
  drawHeader("YModem");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(4, 30, "Esperando envio...");
  u8g2.drawStr(4, 42, "Inicia el envio");
  u8g2.drawStr(4, 52, "en tu PC ahora.");
  u8g2.sendBuffer();

  String name = ymodemReceiveFile(SD, "/");
  if (name.length() > 0) {
    statusMsg = "OK: " + name;
  } else {
    statusMsg = "Error al recibir";
  }
  showingStatus = true;
  listLoaded = false;
}

void screenYmodemLoop() {
  if (!listLoaded) {
    loadFileList();
    listLoaded = true;
    if (selection >= fileCount + 1) selection = 0;
  }

  if (showingStatus) {
    if (isButtonJustPressed(PIN_SELECT) || isButtonJustPressed(PIN_BACK)) {
      buzzerClick();
      showingStatus = false;
      loadFileList();
      listLoaded = true;
      selection = 0;
    }
    u8g2.clearBuffer();
    drawHeader("YModem");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 35, statusMsg.c_str());
    u8g2.drawStr(4, 55, "SEL/BACK: continuar");
    u8g2.sendBuffer();
    return;
  }

  int totalItems = fileCount + 1;

  if (isButtonJustPressed(PIN_UP)) {
    buzzerClick();
    selection--;
    if (selection < 0) selection = totalItems - 1;
  }
  if (isButtonJustPressed(PIN_DOWN)) {
    buzzerClick();
    selection++;
    if (selection >= totalItems) selection = 0;
  }
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    currentScreen = SCREEN_APPS;
    listLoaded = false;
    return;
  }
  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (selection == 0) {
      doReceive();
    } else {
      doSend(selection - 1);
    }
    return;
  }

  u8g2.clearBuffer();
  drawHeader("YModem");
  u8g2.setFont(u8g2_font_5x7_tr);

  const int VISIBLE_ITEMS = 4;
  int scrollTop = selection - (VISIBLE_ITEMS - 1);
  if (scrollTop < 0) scrollTop = 0;
  int maxTop = totalItems - VISIBLE_ITEMS;
  if (maxTop < 0) maxTop = 0;
  if (scrollTop > selection) scrollTop = selection;
  if (scrollTop > maxTop) scrollTop = maxTop;

  const int rowSpacing = 9;
  const int firstY = 24;

  for (int row = 0; row < VISIBLE_ITEMS; row++) {
    int i = scrollTop + row;
    if (i >= totalItems) break;
    int yy = firstY + row * rowSpacing;
    if (i == selection) {
      u8g2.drawXBM(2, yy - 6, 4, 7, image_arrow_bits);
    }
    if (i == 0) {
      u8g2.drawStr(10, yy, "[ Recibir archivo ]");
    } else {
      u8g2.drawStr(10, yy, fileNames[i - 1]);
    }
  }

  if (scrollTop > 0) {
    u8g2.drawTriangle(122, 20, 126, 20, 124, 17);
  }
  if (scrollTop + VISIBLE_ITEMS < totalItems) {
    u8g2.drawTriangle(122, 60, 126, 60, 124, 63);
  }

  u8g2.sendBuffer();
}
