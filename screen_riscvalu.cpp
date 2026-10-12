#include "Apps/screen_riscvalu.h"
#include "Drivers/buzzer.h"
#include <stdint.h>
#include <string.h>

// ALU de 8 bits
extern "C" uint64_t riscv_alu_asm(uint32_t a, uint32_t b, uint32_t op);

#define ALU_EXEC_MS 350   

static const char *const ALU_MNEM[8] = {"ADD", "SUB", "AND", "OR", "XOR", "SLL", "SRL", "SLTU"};
static const char *const ALU_FORM[8] = {"A + B", "A - B", "A & B", "A | B", "A ^ B", "A << B", "A >> B", "A < B"};
static const char *const ALU_INST[8] = {"add a0,a1,a2", "sub a0,a1,a2", "and a0,a1,a2", "or a0,a1,a2",
                                        "xor a0,a1,a2", "sll a0,a1,a2", "srl a0,a1,a2", "sltu a0,a1,a2"};

enum AluView { VIEW_MAIN, VIEW_VALUE, VIEW_OP };
static AluView view = VIEW_MAIN;

static uint8_t aVal = 0, bVal = 0, aluOp = 0;
static uint8_t resVal = 0, carryFlag = 0, ovfFlag = 0;
static bool hasResult = false;
static unsigned long execMs = 0;

static uint8_t mainSel = 0;      // 0 = A, 1 = B, 2 = OP, 3 = LISTO
static uint8_t editTarget = 0;   // 0 = A, 1 = B
static uint8_t editVal = 0;
static uint8_t editOp = 0;

static unsigned long holdStart[2] = {0, 0}, lastRep[2] = {0, 0};

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


static void bin8(char *out, uint8_t v) {
  for (int i = 7, k = 0; i >= 0; i--) out[k++] = ((v >> i) & 1) ? '1' : '0';
  out[8] = 0;
}

static void asciiRender() {
  char ab[9], bb[9], rb[9];
  bin8(ab, aVal); bin8(bb, bVal); bin8(rb, resVal);

  Serial.println();
  Serial.printf("=== ALU RISC-V 8 bits ===   (%s)\n\n", ALU_INST[aluOp]);
  Serial.printf(" A  = %3u (%s) ---\\\n", aVal, ab);
  Serial.printf("                         \\\n");
  Serial.printf(" OP = %-4s ------------>  [ ALU ] ---> Zero     : %d\n", ALU_MNEM[aluOp], resVal == 0);
  Serial.printf("                         /           Result   : %u (%s)\n", resVal, rb);
  Serial.printf(" B  = %3u (%s) ---/            Overflow : %d   (acarreo: %d)\n", bVal, bb, ovfFlag, carryFlag);
}

static void aluExecute() {
  uint64_t r = riscv_alu_asm(aVal, bVal, aluOp);   
  resVal    = (uint8_t)(r & 0xFF);
  carryFlag = (uint8_t)((r >> 32) & 1);
  ovfFlag   = (uint8_t)((r >> 33) & 1);
  hasResult = true;
  execMs = millis();
  asciiRender();
}



static void drawItem(int x, int y, int w, int h, const char *t, bool sel) {
  if (sel) {
    u8g2.drawBox(x, y, w, h);
    u8g2.setDrawColor(0);
    u8g2.drawStr(x + 3, y + h - 3, t);
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawFrame(x, y, w, h);
    u8g2.drawStr(x + 3, y + h - 3, t);
  }
}

static void drawDot(int x, int y, bool on) {
  if (on) u8g2.drawDisc(x, y, 2);
  else    u8g2.drawCircle(x, y, 2);
}

static void drawMain() {
  char buf[16];
  bool show = hasResult && (millis() - execMs) > ALU_EXEC_MS;
  bool busy = hasResult && !show;

 
  u8g2.drawHLine(38, 17, 12);   // A -> ALU
  u8g2.drawHLine(38, 45, 12);   // B -> ALU
  u8g2.drawVLine(68, 11, 5);    // OP -> ALU
  u8g2.drawLine(65, 12, 68, 15);
  u8g2.drawLine(71, 12, 68, 15);

 
  u8g2.drawLine(50, 13, 50, 26);
  u8g2.drawLine(50, 26, 58, 33);
  u8g2.drawLine(58, 33, 50, 40);
  u8g2.drawLine(50, 40, 50, 52);
  u8g2.drawLine(50, 52, 86, 50);
  u8g2.drawLine(86, 50, 86, 20);
  u8g2.drawLine(86, 20, 50, 13);

  u8g2.setFont(u8g2_font_6x10_tr);
  if (busy) {
    u8g2.drawBox(61, 26, 22, 11);
    u8g2.setDrawColor(0);
    u8g2.drawStr(63, 35, "ALU");
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawStr(63, 35, "ALU");
  }
  u8g2.setFont(u8g2_font_5x7_tr);


  snprintf(buf, sizeof(buf), "A %3d", aVal);
  drawItem(2, 12, 37, 11, buf, mainSel == 0);
  snprintf(buf, sizeof(buf), "B %3d", bVal);
  drawItem(2, 40, 37, 11, buf, mainSel == 1);
  snprintf(buf, sizeof(buf), "OP %s", ALU_MNEM[aluOp]);
  drawItem(44, 0, 48, 11, buf, mainSel == 2);
  drawItem(2, 53, 37, 11, "LISTO", mainSel == 3);


  const int ys[3] = {25, 35, 45};
  for (int i = 0; i < 3; i++) {
    u8g2.drawHLine(87, ys[i], 3);
    u8g2.drawLine(88, ys[i] - 2, 90, ys[i]);
    u8g2.drawLine(88, ys[i] + 2, 90, ys[i]);
  }
  u8g2.drawStr(92, 28, "Zero");
  drawDot(122, 25, show && resVal == 0);
  u8g2.drawStr(92, 38, "Res");
  if (show) snprintf(buf, sizeof(buf), "%3d", resVal);
  else      snprintf(buf, sizeof(buf), hasResult ? "..." : "---");
  u8g2.drawStr(110, 38, buf);
  u8g2.drawStr(92, 48, "Ovf");
  drawDot(122, 45, show && ovfFlag);

  u8g2.drawStr(46, 61, "UP/DN  SEL");
}

static void drawValueEdit() {
  char buf[24];
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0);
  snprintf(buf, sizeof(buf), "Operando %c  (0-255)", editTarget ? 'B' : 'A');
  u8g2.drawStr(3, 8, buf);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_logisoso24_tn);
  snprintf(buf, sizeof(buf), "%d", editVal);
  u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, 42, buf);
  u8g2.setFont(u8g2_font_5x7_tr);

  
  u8g2.drawTriangle(12, 28, 24, 28, 18, 20);
  u8g2.drawTriangle(12, 34, 24, 34, 18, 42);
  u8g2.drawTriangle(104, 28, 116, 28, 110, 20);
  u8g2.drawTriangle(104, 34, 116, 34, 110, 42);

  
  for (int i = 7; i >= 0; i--) {
    int x = 36 + (7 - i) * 7;
    if ((editVal >> i) & 1) u8g2.drawBox(x, 48, 6, 6);
    else                    u8g2.drawFrame(x, 48, 6, 6);
  }

  snprintf(buf, sizeof(buf), "0x%02X", editVal);
  u8g2.drawStr(3, 62, buf);
  u8g2.drawStr(36, 62, "SEL ok  BACK no");
}

static void drawOpEdit() {
  char buf[24];
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(3, 8, "Operacion");
  u8g2.setDrawColor(1);

  int first = editOp - 2;
  if (first < 0) first = 0;
  if (first > 3) first = 3;
  for (int r = 0; r < 5; r++) {
    int i = first + r;
    int y = 11 + r * 10;
    snprintf(buf, sizeof(buf), "%-5s %s", ALU_MNEM[i], ALU_FORM[i]);
    if (i == editOp) {
      u8g2.drawBox(0, y, 122, 10);
      u8g2.setDrawColor(0);
      u8g2.drawStr(4, y + 8, buf);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(4, y + 8, buf);
    }
  }
  
  u8g2.drawVLine(125, 11, 52);
  u8g2.drawBox(124, 11 + editOp * 6, 3, 10);
}


static void holdRepeat(int pin, int idx, int dir) {
  if (digitalRead(pin) != LOW) { holdStart[idx] = 0; return; }
  unsigned long now = millis();
  if (holdStart[idx] == 0) { holdStart[idx] = now; lastRep[idx] = now; return; }
  unsigned long held = now - holdStart[idx];
  if (held > 450 && (now - lastRep[idx]) >= (held > 1600 ? 40UL : 90UL)) {
    lastRep[idx] = now;
    int step = (held > 1600) ? 8 : 1;
    editVal = (uint8_t)((editVal + dir * step) & 0xFF);
  }
}

void screenRiscvAluLoop() {
  bool sel  = isButtonJustPressed(PIN_SELECT);
  bool up   = isButtonJustPressed(PIN_UP);
  bool down = isButtonJustPressed(PIN_DOWN);
  bool back = isButtonJustPressed(PIN_BACK);

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'w' || c == 'W') up = true;
    else if (c == 's' || c == 'S') down = true;
    else if (c == 'e' || c == 'E') sel = true;
    else if (c == 'q' || c == 'Q') back = true;
  }

  if (sel) buzzerBeep();
  else if (up || down || back) buzzerClick();

  switch (view) {
    case VIEW_MAIN:
      if (back) { currentScreen = SCREEN_APPS; return; }
      if (up)   mainSel = (mainSel + 3) % 4;
      if (down) mainSel = (mainSel + 1) % 4;
      if (sel) {
        if (mainSel <= 1) {
          editTarget = mainSel;
          editVal = mainSel ? bVal : aVal;
          holdStart[0] = holdStart[1] = 0;
          view = VIEW_VALUE;
        } else if (mainSel == 2) {
          editOp = aluOp;
          view = VIEW_OP;
        } else {
          aluExecute();
        }
      }
      break;

    case VIEW_VALUE:
      if (back) { view = VIEW_MAIN; break; }         
      if (up)   editVal++;
      if (down) editVal--;
      holdRepeat(PIN_UP, 0, +1);                       
      holdRepeat(PIN_DOWN, 1, -1);
      if (sel) {
        if (editTarget) bVal = editVal; else aVal = editVal;
        hasResult = false;
        view = VIEW_MAIN;
      }
      break;

    case VIEW_OP:
      if (back) { view = VIEW_MAIN; break; }
      if (up)   editOp = (editOp + 7) & 7;
      if (down) editOp = (editOp + 1) & 7;
      if (sel) {
        aluOp = editOp;
        hasResult = false;
        view = VIEW_MAIN;
      }
      break;
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setDrawColor(1);
  switch (view) {
    case VIEW_MAIN:  drawMain();      break;
    case VIEW_VALUE: drawValueEdit(); break;
    case VIEW_OP:    drawOpEdit();    break;
  }
  u8g2.sendBuffer();
}
