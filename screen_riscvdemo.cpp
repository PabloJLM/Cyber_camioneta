#include "Apps/screen_riscvdemo.h"
#include "Drivers/buzzer.h"
#include <stdint.h>

// Subrutina real en RISC-V puro (ver riscv_fib.S) -- fibonacci recursivo.
// No es C compilado a RISC-V "de fondo": es la rutina que el chip ejecuta
// tal cual, instruccion por instruccion (jal/addi/sw/lw/ret).
extern "C" int riscv_fib_asm(int n);

#define RISCV_DEMO_N      5   // fib(5): 15 llamadas -> entra justo en el buffer
#define RISCV_TRACE_MAX   32

struct RiscvTraceEntry {
  bool     isExit;
  int      n;
  int      result;   // solo valido si isExit
  uint32_t sp;        // solo valido si !isExit (sp al entrar)
};

static RiscvTraceEntry riscvTrace[RISCV_TRACE_MAX];
static volatile int riscvTraceCount = 0;
static int riscvCallDepth = 0;   // profundidad actual, para indentar

// Estas dos funciones las llama DIRECTO el ensamblador (riscv_fib.S hace
// "call riscv_trace_enter" / "call riscv_trace_exit"), asi que cada
// entrada/salida de la subrutina real queda grabada aqui.
extern "C" void riscv_trace_enter(int n, uint32_t sp) {
  if (riscvTraceCount < RISCV_TRACE_MAX) {
    riscvTrace[riscvTraceCount].isExit = false;
    riscvTrace[riscvTraceCount].n = n;
    riscvTrace[riscvTraceCount].sp = sp;
    riscvTraceCount++;
  }
  riscvCallDepth++;
}

extern "C" void riscv_trace_exit(int n, int result) {
  if (riscvTraceCount < RISCV_TRACE_MAX) {
    riscvTrace[riscvTraceCount].isExit = true;
    riscvTrace[riscvTraceCount].n = n;
    riscvTrace[riscvTraceCount].result = result;
    riscvTraceCount++;
  }
  riscvCallDepth--;
}

static int stepIndex = 0;
static int finalResult = -1;
static bool hasRun = false;

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

static void runDemo() {
  riscvTraceCount = 0;
  riscvCallDepth = 0;
  stepIndex = 0;
  finalResult = riscv_fib_asm(RISCV_DEMO_N);   // <-- corre de verdad en el core RISC-V
  hasRun = true;
}

// Profundidad de pila en el paso "stepIndex" (cuantos CALL sin su RET
// todavia, recorriendo la traza desde el principio hasta ese paso).
static int depthAtStep(int step) {
  int depth = 0;
  for (int i = 0; i <= step && i < riscvTraceCount; i++) {
    if (riscvTrace[i].isExit) depth--;
    else depth++;
  }
  return depth;
}

void screenRiscvDemoLoop() {
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    runDemo();
  }

  if (hasRun && riscvTraceCount > 0) {
    if (isButtonJustPressed(PIN_DOWN)) {
      buzzerClick();
      stepIndex++;
      if (stepIndex >= riscvTraceCount) stepIndex = riscvTraceCount - 1;
    }
    if (isButtonJustPressed(PIN_UP)) {
      buzzerClick();
      stepIndex--;
      if (stepIndex < 0) stepIndex = 0;
    }
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(8, 1, 112, 14);
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
    RiscvTraceEntry &e = riscvTrace[stepIndex];
    int depth = depthAtStep(stepIndex);
    if (depth < 0) depth = 0;

    char line1[26];
    char line2[26];
    if (!e.isExit) {
      snprintf(line1, sizeof(line1), "CALL fib(%d)", e.n);
      snprintf(line2, sizeof(line2), "sp=0x%05lX", (unsigned long)(e.sp & 0xFFFFF));
    } else {
      snprintf(line1, sizeof(line1), "RET  fib(%d)", e.n);
      snprintf(line2, sizeof(line2), "a0=%d", e.result);
    }
    u8g2.drawStr(4, 26, line1);
    u8g2.drawStr(4, 36, line2);

    // barra de profundidad de pila (cuantos marcos abiertos hay ahorita)
    char dbuf[20];
    snprintf(dbuf, sizeof(dbuf), "pila: %d", depth);
    u8g2.drawStr(4, 48, dbuf);
    for (int i = 0; i < depth && i < 8; i++) {
      u8g2.drawBox(60 + i * 6, 42, 4, 8);
    }

    char cnt[24];
    snprintf(cnt, sizeof(cnt), "paso %d/%d  res=%d", stepIndex + 1, riscvTraceCount, finalResult);
    u8g2.drawStr(4, 62, cnt);
  }

  u8g2.sendBuffer();
}
