#include "Apps/screen_msgpreset.h"
#include "Drivers/buzzer.h"
#include "Drivers/mesh.h"

// App "Msj. Preescritos": manda mensajes ESP-NOW a otra(s) camioneta(s)
// (o a todas). Primero se elige destino, y luego queda en una pantalla
// partida: arriba un catalogo scrolleable de mensajes prehechos (SEL
// manda), abajo el ultimo mensaje recibido (o mandado) de la red, en
// vivo. La mensajeria (ESP-NOW) solo esta prendida mientras esta app
// esta activa, para no chocar con el WiFi que usan otras apps.

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static bool meshActive = false;
static int      msgState    = 0;   // 0 = eligiendo destino, 1 = catalogo + recepcion
static uint16_t msgTargetId = 0xFFFF;
static int      listOffset  = 0;

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

static void targetLabel(uint16_t id, char* out, size_t n) {
  if (id == 0xFFFF) snprintf(out, n, "Todos");
  else              meshFormatId(id, out, n);
}

static int collectTargets(uint16_t* ids, int maxN) {
  int n = 0;
  if (n < maxN) ids[n++] = 0xFFFF;
  for (int i = 0; i < MAX_NODES && n < maxN; i++) {
    const MeshNode* nd = meshNodeAt(i);
    if (meshNodeAlive(nd)) ids[n++] = nd->id;
  }
  return n;
}

void screenMsgPresetLoop() {
  if (!meshActive) {
    meshBegin();
    meshActive  = true;
    msgState    = 0;
    listOffset  = 0;
    msgTargetId = 0xFFFF;
  }

  meshLoop();

  uint16_t targets[MAX_NODES + 1];
  int tCount = collectTargets(targets, MAX_NODES + 1);

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (msgState == 1) {
      msgState   = 0;
      listOffset = 0;
    } else {
      meshEnd();
      meshActive = false;
      currentScreen = SCREEN_APPS;
      return;
    }
  } else if (msgState == 0) {
    if (listOffset >= tCount) listOffset = tCount - 1;
    if (listOffset < 0) listOffset = 0;
    if (isButtonJustPressed(PIN_UP))   { buzzerClick(); listOffset = (listOffset - 1 + tCount) % tCount; }
    if (isButtonJustPressed(PIN_DOWN)) { buzzerClick(); listOffset = (listOffset + 1) % tCount; }
    if (isButtonJustPressed(PIN_SELECT)) {
      buzzerClick();
      msgTargetId = targets[listOffset];
      msgState    = 1;
      listOffset  = 0;
    }
  } else {
    int cCount = meshCannedCount();
    if (isButtonJustPressed(PIN_UP))   { buzzerClick(); listOffset = (listOffset - 1 + cCount) % cCount; }
    if (isButtonJustPressed(PIN_DOWN)) { buzzerClick(); listOffset = (listOffset + 1) % cCount; }
    if (isButtonJustPressed(PIN_SELECT)) {
      buzzerBeep();
      meshSendText(meshCannedAt(listOffset), msgTargetId);
      // se queda en el catalogo para mandar otro rapido
    }
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  drawHeader("Msj. Preescritos");
  u8g2.setFont(u8g2_font_5x7_tr);

  if (msgState == 0) {
    u8g2.drawStr(2, 20, "Destino:");
    const int rows = 4;
    int top = listOffset - 1; if (top < 0) top = 0;
    if (top > tCount - rows) top = (tCount > rows) ? tCount - rows : 0;
    for (int i = 0; i < rows && (top + i) < tCount; i++) {
      int idx = top + i;
      int y = 30 + i * 9;
      char lbl[10]; targetLabel(targets[idx], lbl, sizeof(lbl));
      u8g2.drawStr(2, y, (idx == listOffset) ? ">" : " ");
      u8g2.drawStr(10, y, lbl);
    }
    u8g2.drawStr(2, 62, "SEL: elegir  BACK: salir");
  } else {
    char dstLbl[10]; targetLabel(msgTargetId, dstLbl, sizeof(dstLbl));
    char hdr[20]; snprintf(hdr, sizeof(hdr), "A: %s", dstLbl);
    u8g2.drawStr(2, 20, hdr);

    int cCount = meshCannedCount();
    int cur = listOffset;
    if (cur >= cCount) cur = cCount - 1;
    if (cur < 0) cur = 0;
    const int rows = 2;
    int top = cur - 1; if (top < 0) top = 0;
    if (top > cCount - rows) top = (cCount > rows) ? cCount - rows : 0;
    for (int i = 0; i < rows && (top + i) < cCount; i++) {
      int idx = top + i;
      int y = 29 + i * 9;
      u8g2.drawStr(2, y, (idx == cur) ? ">" : " ");
      u8g2.drawStr(10, y, meshCannedAt(idx));
    }
    if (cCount > rows) {
      char c[10]; snprintf(c, sizeof(c), "%d/%d", cur + 1, cCount);
      int w = u8g2.getStrWidth(c);
      u8g2.drawStr(SCREEN_W - w - 2, 20, c);
    }

    u8g2.drawHLine(0, 41, 128);

    u8g2.drawStr(2, 50, "Recepcion:");
    int hCount = meshMsgHistCount();
    if (hCount == 0) {
      u8g2.drawStr(2, 60, "sin mensajes aun");
    } else {
      const MeshMsgItem* it = meshMsgHistAt(0);
      char from[10]; meshFormatId(it->from, from, sizeof(from));
      char to[10]; targetLabel(it->to, to, sizeof(to));
      char l[30]; snprintf(l, sizeof(l), "%s>%s: %s", from, to, it->text);
      u8g2.drawStr(2, 60, l);
    }
  }

  u8g2.sendBuffer();
}
