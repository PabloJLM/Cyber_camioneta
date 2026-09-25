#include "Apps/screen_meshterm.h"
#include "Drivers/buzzer.h"
#include "Drivers/mesh.h"
#include <stdlib.h>

// App "Terminal Chat": misma idea que PC-Mode/Con.Remota (dueña total
// de la pantalla mientras esta activa), pero para chatear por ESP-NOW
// con una o mas camionetas usando el monitor serial como interfaz.

static const unsigned char image_terminal_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0xfe,0xff,0xff,0x7f,0x02,0x00,0x00,0x40,0x02,0x00,0x00,0x40,
  0x02,0x00,0x00,0x40,0x02,0x00,0x00,0x40,0x72,0x00,0x00,0x40,0x8a,0x00,0x00,0x40,
  0x8a,0x00,0x00,0x40,0x8a,0x00,0x00,0x40,0x72,0x00,0x00,0x40,0x02,0x00,0x00,0x40,
  0x02,0x00,0x00,0x40,0x02,0x00,0x00,0x40,0xfe,0xff,0xff,0x7f,0x00,0x00,0x00,0x00
};
static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static bool meshActive     = false;
static bool termInitialized = false;

static void printPrompt() {
  Serial.print(F("mesh:~$ "));
}

static void printBanner() {
  Serial.println();
  Serial.println(F("=== Terminal de mensajes ESP-NOW ==="));
  char id[10]; meshFormatId(meshMyId(), id, sizeof(id));
  Serial.print(F("Yo: id="));
  Serial.println(id);
  Serial.println(F("Comandos:"));
  Serial.println(F("  send <texto>             manda a TODAS"));
  Serial.println(F("  sendto <id_hex> <texto>  manda a una camioneta especifica"));
  Serial.println(F("  nodes / ls               camionetas vivas"));
  Serial.println(F("  clear / cls              limpia pantalla"));
  Serial.println(F("  exit                     vuelve al menu"));
  Serial.println();
}

static void printNodesList() {
  int n = meshNodeCount();
  if (n == 0) { Serial.println(F("  ninguna camioneta vista todavia")); return; }
  for (int i = 0; i < MAX_NODES; i++) {
    const MeshNode* nd = meshNodeAt(i);
    if (!meshNodeAlive(nd)) continue;
    char id[10]; meshFormatId(nd->id, id, sizeof(id));
    Serial.print(F("  "));
    Serial.print(id);
    if (nd->direct) {
      Serial.print(F("  directo  rssi="));
      Serial.println(nd->rssi);
    } else {
      Serial.print(F("  saltos="));
      Serial.println(nd->hops);
    }
  }
}

// parte el siguiente token separado por espacio de 'p'; deja *p
// apuntando al resto de la linea.
static void nextToken(char*& p, char* buf, size_t n) {
  while (*p == ' ') p++;
  size_t i = 0;
  while (*p && *p != ' ' && i < n - 1) buf[i++] = *p++;
  buf[i] = '\0';
  while (*p == ' ') p++;
}

static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState = HIGH;
  static uint8_t lastReading     = HIGH;
  static unsigned long lastDebounceTime = 0;
  uint8_t reading = digitalRead(pin);
  if (reading != lastReading) { lastDebounceTime = millis(); lastReading = reading; }
  if ((millis() - lastDebounceTime) > 50) {
    if (lastStableState == HIGH && reading == LOW) { lastStableState = reading; return true; }
    lastStableState = reading;
  }
  return false;
}

static void teardown() {
  meshEnd();
  meshActive      = false;
  termInitialized = false;
}

static void processChatCommand() {
  static String commandBuffer = "";
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        commandBuffer.trim();
        Serial.println();

        if (commandBuffer == "help" || commandBuffer == "?") {
          printBanner();

        } else if (commandBuffer == "nodes" || commandBuffer == "ls") {
          printNodesList();

        } else if (commandBuffer.startsWith("send ")) {
          String txt = commandBuffer.substring(5);
          meshSendText(txt.c_str());
          Serial.print(F("  yo -> Todas: "));
          Serial.println(txt);

        } else if (commandBuffer.startsWith("sendto ")) {
          char line[96];
          commandBuffer.substring(7).toCharArray(line, sizeof(line));
          char* p = line;
          char idBuf[8];
          nextToken(p, idBuf, sizeof(idBuf));
          uint16_t dst = (uint16_t)strtoul(idBuf, nullptr, 16);
          meshSendText(p, dst);
          Serial.print(F("  yo -> "));
          Serial.print(idBuf);
          Serial.print(F(": "));
          Serial.println(p);

        } else if (commandBuffer == "clear" || commandBuffer == "cls") {
          Serial.print(F("\033[2J\033[H"));

        } else if (commandBuffer == "exit") {
          Serial.println(F("  saliendo de Terminal Chat..."));
          teardown();
          currentScreen = SCREEN_APPS;
          commandBuffer = "";
          return;

        } else {
          Serial.print(F("  command not found: "));
          Serial.println(commandBuffer);
          Serial.println(F("  escribe 'help' para ver los comandos"));
        }

        commandBuffer = "";
        Serial.println();
        printPrompt();
      }
    } else if (c == 8 || c == 127) {
      if (commandBuffer.length() > 0) {
        commandBuffer.remove(commandBuffer.length() - 1);
        Serial.write(8); Serial.write(' '); Serial.write(8);
      }
    } else if (c >= 32 && c <= 126) {
      commandBuffer += c;
      Serial.write(c);
    }
  }
}

// Imprime en vivo lo que llega dirigido a mi/todas (mesh.cpp ya lo filtra).
static void checkIncoming() {
  if (meshHasNewText()) {
    char from[10]; meshFormatId(meshLastTextFrom(), from, sizeof(from));
    Serial.println();
    Serial.print(F("  "));
    Serial.print(from);
    Serial.print(F(" -> yo: "));
    Serial.println(meshLastText());
    meshClearNewText();
    buzzerBeep();
    Serial.println();
    printPrompt();
  }
}

static void drawTermScreen() {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(26, 11, "TERMINAL CHAT");
  u8g2.setDrawColor(1);
  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(48, 24, 32, 16, image_terminal_bits);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(15, 48, "Chat ESP-NOW");
  u8g2.drawStr(15, 58, "Ver monitor serial");
  u8g2.sendBuffer();
}

void screenMeshTermLoop() {
  if (!meshActive) {
    meshBegin();
    meshActive = true;
  }

  if (!termInitialized) {
    printBanner();
    printPrompt();
    termInitialized = true;
  }

  meshLoop();
  processChatCommand();
  if (currentScreen != SCREEN_MESHTERM) return;   // 'exit' ya nos saco

  checkIncoming();

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    Serial.println();
    Serial.println(F("  saliendo de Terminal Chat..."));
    teardown();
    currentScreen = SCREEN_APPS;
    return;
  }

  drawTermScreen();
}
