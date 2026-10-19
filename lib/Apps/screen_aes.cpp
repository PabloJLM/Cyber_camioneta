#include "screen_aes.h"
#include "buzzer.h"
#include "aes_demo_terminal.h"
#include <stdint.h>

// Pantalla simbolica + terminal serie propia. El OLED solo anima un
// candado (sin amontonar texto), pero esta pantalla SI escucha el puerto
// serie: mientras esta abierta, se puede escribir "help", "msg <texto>",
// "key 0|1|<hex>", "show", "run", "attempts", "avalanche" (sin el prefijo
// "aes", ya que aca el contexto ya es AES) y se despachan con el mismo
// nucleo compartido que usan las terminales de Ajustes y Remota
// (Drivers/aes_demo_terminal), asi que el comportamiento es identico.

#define AES_ANIM_MS 900   // cada cuanto el candado abre/cierra

static bool termInitialized = false;
static String lineBuffer;

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

static void drawLock(int cx, int top, bool closed) {
  const int w = 26, h = 18, shackleW = 14;
  int bodyY = top + 8;

  // arco del candado: cerrado = completo arriba del cuerpo; abierto = un
  // lado levantado, como si se acabara de abrir.
  int archTop = closed ? top : top - 5;
  u8g2.drawVLine(cx - shackleW / 2, archTop, bodyY - archTop);
  u8g2.drawVLine(cx + shackleW / 2, top, bodyY - top);
  u8g2.drawCircle(cx, archTop + 6, 7, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);

  // cuerpo
  u8g2.drawFrame(cx - w / 2, bodyY, w, h);
  u8g2.drawBox(cx - 2, bodyY + h / 2 - 2, 4, h / 2);   // ojo de la cerradura
}

static void printPrompt() {
  Serial.print(F("AES:~$ "));
}

static void printBanner() {
  Serial.println();
  Serial.println(F(" █████╗ ███████╗███████╗"));
  Serial.println(F("██╔══██╗██╔════╝██╔════╝"));
  Serial.println(F("███████║█████╗  ███████╗"));
  Serial.println(F("██╔══██║██╔══╝  ╚════██║"));
  Serial.println(F("██║  ██║███████╗███████║"));
  Serial.println(F("╚═╝  ╚═╝╚══════╝╚══════╝"));
  Serial.println(F("Escribe help para ver los comandos"));
  Serial.println();

}

// Igual que un comando de las otras terminales, pero sin exigir el
// prefijo "aes " porque esta pantalla ya es exclusivamente de AES.
static void dispatchLine(const String &line) {
  if (line == "clear" || line == "cls") {
    Serial.print(F("\033[2J\033[H"));
    printPrompt();
    return;
  }

  Serial.println();
  if (line.length() == 0) { printPrompt(); return; }

  String full = line;
  if (!full.startsWith("aes")) {
    full = "aes " + full;
  }
  if (!aesTerminalHandle(Serial, full)) {
    Serial.print(F("   comando no reconocido: "));
    Serial.println(line.c_str());
    Serial.println(F("   escribi 'help' para ver los comandos"));
  }
  Serial.println();
  printPrompt();
}

static void processSerialLine() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (lineBuffer.length() > 0) {
        String line = lineBuffer;
        line.trim();
        lineBuffer = "";
        dispatchLine(line);
      }
    } else if (c == 8 || c == 127) {
      if (lineBuffer.length() > 0) {
        lineBuffer.remove(lineBuffer.length() - 1);
        Serial.write(8); Serial.write(' '); Serial.write(8);
      }
    } else if (c >= 32 && c <= 126) {
      lineBuffer += c;
      Serial.write(c);
    }
  }
}

void screenAesLoop() {
  if (!termInitialized) {
    printBanner();
    printPrompt();
    termInitialized = true;
    lineBuffer = "";
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    termInitialized = false;
    currentScreen = SCREEN_APPS;
    return;
  }

  processSerialLine();

  bool closed = ((millis() / AES_ANIM_MS) % 2) == 0;

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawBox(0, 0, 128, 11);
  u8g2.setDrawColor(0);
  u8g2.drawStr(10, 9, "Demo AES-128");
  u8g2.setDrawColor(1);

  drawLock(64, 16, closed);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(4, 52, "Ver puerto serial");
  u8g2.drawStr(4, 61, "Escribe help");

  u8g2.sendBuffer();
}
