#include "Apps/screen_riscvdemo.h"
#include "Drivers/buzzer.h"
#include <stdint.h>

// Subrutina real en RISC-V (riscv_fib.S): fibonacci recursivo.
// Cada CALL/RET queda registrado en un trace; esta pantalla lo reproduce
// como pila de cajas apiladas, en el OLED y en ASCII por el puerto serie.
extern "C" int riscv_fib_asm(int n);

#define RISCV_DEMO_N      5     // fib(5): 15 llamadas -> 30 eventos en el trace
#define RISCV_TRACE_MAX   32
#define RISCV_STACK_MAX   8
#define RISCV_STEP_MS     700   // velocidad de la animacion automatica
#define RISCV_ANSI_CLEAR  0     // 1 = limpia la terminal en cada paso (no sirve en el monitor del Arduino IDE)

struct RiscvTraceEntry {
  bool     isExit;
  int      n;
  int      result;   // solo valido si isExit
  uint32_t sp;       // solo valido si !isExit (sp al entrar)
};

struct RiscvFrame {
  int      n;
  uint32_t sp;
};

static RiscvTraceEntry riscvTrace[RISCV_TRACE_MAX];
static volatile int riscvTraceCount = 0;

// Las llama DIRECTO el ensamblador (riscv_fib.S hace "call riscv_trace_enter" /
// "call riscv_trace_exit"): cada entrada/salida de la subrutina se guarda aca.
extern "C" void riscv_trace_enter(int n, uint32_t sp) {
  if (riscvTraceCount < RISCV_TRACE_MAX) {
    riscvTrace[riscvTraceCount].isExit = false;
    riscvTrace[riscvTraceCount].n = n;
    riscvTrace[riscvTraceCount].sp = sp;
    riscvTraceCount++;
  }
}

extern "C" void riscv_trace_exit(int n, int result) {
  if (riscvTraceCount < RISCV_TRACE_MAX) {
    riscvTrace[riscvTraceCount].isExit = true;
    riscvTrace[riscvTraceCount].n = n;
    riscvTrace[riscvTraceCount].result = result;
    riscvTraceCount++;
  }
}

static int  stepIndex = 0;
static int  finalResult = -1;
static bool hasRun = false;
static bool autoPlay = false;
static bool dirty = false;               // hay que reimprimir el ASCII
static unsigned long lastStepMs = 0;

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

// ---------- logica de la demo ----------

static void runDemo() {
  riscvTraceCount = 0;
  stepIndex = 0;
  finalResult = riscv_fib_asm(RISCV_DEMO_N);
  hasRun = true;
  autoPlay = true;
  lastStepMs = millis();
  dirty = true;
}

static void stepForward() {
  if (!hasRun) return;
  if (stepIndex < riscvTraceCount - 1) { stepIndex++; dirty = true; }
}

static void stepBack() {
  if (!hasRun) return;
  if (stepIndex > 0) { stepIndex--; dirty = true; }
}

static void toggleAuto() {
  if (!hasRun) { runDemo(); return; }
  if (!autoPlay && stepIndex >= riscvTraceCount - 1) { runDemo(); return; }  // termino: reiniciar
  autoPlay = !autoPlay;
  lastStepMs = millis();
  dirty = true;
}

// Reconstruye la pila hasta 'step' (inclusive). Devuelve la profundidad.
// Tras un RET, st[depth] sigue guardando el marco recien sacado.
static int replayStack(int step, RiscvFrame *st) {
  int depth = 0;
  for (int i = 0; i <= step && i < riscvTraceCount; i++) {
    if (riscvTrace[i].isExit) {
      if (depth > 0) depth--;
    } else if (depth < RISCV_STACK_MAX) {
      st[depth].n = riscvTrace[i].n;
      st[depth].sp = riscvTrace[i].sp;
      depth++;
    }
  }
  return depth;
}

// ---------- vista ASCII (puerto serie) ----------

static void asciiBorder(bool dotted) {
  char b[40];
  b[0] = '+';
  for (int i = 1; i <= 30; i++) b[i] = (dotted && (i % 2 == 0)) ? ' ' : '-';
  b[31] = '+';
  b[32] = 0;
  Serial.print("   ");
  Serial.println(b);
}

static void asciiRender() {
  if (!hasRun || riscvTraceCount == 0) return;

  RiscvFrame st[RISCV_STACK_MAX];
  int depth = replayStack(stepIndex, st);
  const RiscvTraceEntry &e = riscvTrace[stepIndex];
  uint32_t base = riscvTrace[0].sp;

  if (RISCV_ANSI_CLEAR) Serial.print("\033[2J\033[H");
  else Serial.println();

  Serial.printf("=== RISC-V: fib(%d) en ensamblador ===   paso %d/%d\n",
                RISCV_DEMO_N, stepIndex + 1, riscvTraceCount);
  Serial.println("[r] reiniciar  [n] sig  [b] atras  [a] auto/pausa");
  Serial.println();

  if (!e.isExit) {
    Serial.printf(" CALL fib(%d)    sp=0x%08lX\n\n", e.n, (unsigned long)e.sp);
  } else {
    Serial.printf(" RET  fib(%d)    a0=%d\n\n", e.n, e.result);
    asciiBorder(true);
    Serial.printf("   : ret fib(%-2d) = %-4d (pop)     :\n", e.n, e.result);
    asciiBorder(true);
  }

  if (depth == 0) {
    Serial.println("   (pila vacia)");
  } else {
    asciiBorder(false);
    for (int i = depth - 1; i >= 0; i--) {
      Serial.printf("   | fib(%-2d) sp-%-3lu | ra s0=%-2d s1 |%s\n",
                    st[i].n, (unsigned long)(base - st[i].sp), st[i].n,
                    (i == depth - 1) ? "  <- tope (sp)" : "");
      asciiBorder(false);
    }
  }
  Serial.printf("   pila: %d marco(s) x 16B = %dB\n", depth, depth * 16);

  if (stepIndex == riscvTraceCount - 1)
    Serial.printf("\n >> resultado: fib(%d) = %d\n", RISCV_DEMO_N, finalResult);
}

static void serialKeys() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'r': case 'R': runDemo();      break;
      case 'n': case 'N': case ' ': autoPlay = false; stepForward(); break;
      case 'b': case 'B': autoPlay = false; stepBack(); break;
      case 'a': case 'A': toggleAuto();   break;
      default: break;
    }
  }
}

// ---------- vista OLED ----------

static void drawDottedFrame(int x, int y, int w, int h) {
  for (int i = 0; i < w; i += 2) { u8g2.drawPixel(x + i, y); u8g2.drawPixel(x + i, y + h - 1); }
  for (int j = 0; j < h; j += 2) { u8g2.drawPixel(x, y + j); u8g2.drawPixel(x + w - 1, y + j); }
}

static void drawStack() {
  const int X = 2, W = 74, H = 9, PITCH = 8, BASE_Y = 55, TOP_MIN_Y = 15;

  RiscvFrame st[RISCV_STACK_MAX];
  int depth = replayStack(stepIndex, st);
  const RiscvTraceEntry &e = riscvTrace[stepIndex];
  uint32_t base = riscvTrace[0].sp;
  char t[20];

  for (int i = 0; i < depth; i++) {
    int y = BASE_Y - PITCH * i;
    if (y < TOP_MIN_Y) break;
    snprintf(t, sizeof(t), "fib(%d) sp-%lu", st[i].n, (unsigned long)(base - st[i].sp));
    bool isTop = (i == depth - 1) && !e.isExit;   // marco recien empujado: resaltado
    if (isTop) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(X, y, W, H);
      u8g2.setDrawColor(0);
      u8g2.drawStr(X + 3, y + 7, t);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(X, y, W, H);
      u8g2.drawStr(X + 3, y + 7, t);
    }
  }

  if (e.isExit) {   // marco que se esta sacando: punteado, con su valor de retorno
    int y = BASE_Y - PITCH * depth;
    if (y >= TOP_MIN_Y) {
      drawDottedFrame(X, y, W, H);
      snprintf(t, sizeof(t), "ret fib(%d)=%d", e.n, e.result);
      u8g2.drawStr(X + 3, y + 7, t);
    }
  }
}

void screenRiscvDemoLoop() {
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    autoPlay = false;
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) { buzzerBeep();  toggleAuto(); }
  if (isButtonJustPressed(PIN_DOWN))   { buzzerClick(); autoPlay = false; stepForward(); }
  if (isButtonJustPressed(PIN_UP))     { buzzerClick(); autoPlay = false; stepBack(); }

  serialKeys();

  if (hasRun && autoPlay && (millis() - lastStepMs) >= RISCV_STEP_MS) {
    lastStepMs = millis();
    if (stepIndex < riscvTraceCount - 1) stepForward();
    else { autoPlay = false; dirty = true; }
  }

  if (dirty) { asciiRender(); dirty = false; }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(8, 1, 112, 13);
  u8g2.setDrawColor(2);
  u8g2.drawStr(17, 11, "RISC-V: fib(5) asm");
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_5x7_tr);

  if (!hasRun) {
    u8g2.drawStr(4, 28, "Subrutina real RV32,");
    u8g2.drawStr(4, 38, "recursiva (jal/ret).");
    u8g2.drawStr(4, 52, "SEL: ejecutar");
    u8g2.drawStr(4, 62, "BACK: salir");
  } else {
    drawStack();

    const RiscvTraceEntry &e = riscvTrace[stepIndex];
    RiscvFrame tmp[RISCV_STACK_MAX];
    int depth = replayStack(stepIndex, tmp);
    char buf[16];

    u8g2.drawVLine(78, 16, 48);
    u8g2.drawStr(81, 23, e.isExit ? "RET" : "CALL");
    snprintf(buf, sizeof(buf), "n=%d", e.n);
    u8g2.drawStr(81, 31, buf);
    if (e.isExit) snprintf(buf, sizeof(buf), "a0=%d", e.result);
    else          snprintf(buf, sizeof(buf), "+16B");
    u8g2.drawStr(81, 39, buf);
    snprintf(buf, sizeof(buf), "pila:%d", depth);
    u8g2.drawStr(81, 47, buf);

    bool finished = (stepIndex == riscvTraceCount - 1);
    if (finished && !autoPlay) snprintf(buf, sizeof(buf), "res=%d", finalResult);
    else snprintf(buf, sizeof(buf), "%s", autoPlay ? "PLAY" : "PAUSA");
    u8g2.drawStr(81, 55, buf);

    snprintf(buf, sizeof(buf), "%d/%d", stepIndex + 1, riscvTraceCount);
    u8g2.drawStr(81, 63, buf);
  }

  u8g2.sendBuffer();
}
