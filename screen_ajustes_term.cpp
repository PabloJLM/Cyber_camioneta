#include "Ajustes/screen_ajustes_term.h"
#include "Drivers/buzzer.h"
#include "Apps/screen_apflood.h"
#include "Apps/screen_sniffer.h"
#include "Apps/screen_captive.h"


static const unsigned char image_gear_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x00,
  0x00,0x04,0x1f,0x00,0x00,0x0f,0x0f,0x00,0x80,0x0f,0x0f,0x00,0x80,0x0f,0x0f,0x03,
  0x00,0xff,0x87,0x07,0x00,0xfe,0xdf,0x07,0x00,0xfc,0xff,0x0f,0x10,0xfe,0xff,0x07,
  0xf8,0xfe,0xff,0x00,0xf8,0xff,0xff,0x00,0xf8,0xff,0xff,0x00,0xf8,0xff,0xff,0x00,
  0x00,0xff,0xff,0x1f,0x00,0xff,0xff,0x1f,0x00,0xff,0xff,0x1f,0x00,0xff,0x7f,0x1f,
  0xe0,0xff,0x7f,0x08,0xf0,0xff,0x3f,0x00,0xe0,0xfb,0x7f,0x00,0xe0,0xe1,0xff,0x00,
  0xc0,0xf0,0xf0,0x01,0x00,0xf0,0xf0,0x01,0x00,0xf0,0xf0,0x00,0x00,0xf8,0x20,0x00,
  0x00,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static bool termInitialized = false;

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

static void printSection(const char* title) {
  Serial.println();
  Serial.print(F("== "));
  Serial.println(title);
}

static void printRow(const char* key, const String& value) {
  Serial.print(F("  "));
  Serial.print(key);
  int pad = 14 - strlen(key);
  for (int i = 0; i < pad; i++) Serial.print(' ');
  Serial.println(value);
}

static void printPrompt() {
  Serial.print(F("ajustes:~$ "));
}

// ---------- flood{...} (mismo parser que PC-Mode) ----------

static void processFloodCommand(const String& cmd) {
  int open  = cmd.indexOf('{');
  int close = cmd.lastIndexOf('}');
  if (open < 0 || close < 0 || close <= open) {
    Serial.println(F("  uso: flood{mensaje1,mensaje2,...}"));
    return;
  }

  String inner = cmd.substring(open + 1, close);

  String msgs[APFLOOD_MAX_MSGS];
  int  count    = 0;
  bool overflow = false;
  int  start    = 0;

  while (start <= (int)inner.length()) {
    int comma = inner.indexOf(',', start);
    if (comma < 0) comma = inner.length();

    String tok = inner.substring(start, comma);
    tok.trim();

    if (tok.length() > 0) {
      if (count < APFLOOD_MAX_MSGS) {
        bool trimmed = tok.length() > APFLOOD_MAX_SSIDLEN;
        if (trimmed) tok = tok.substring(0, APFLOOD_MAX_SSIDLEN);
        msgs[count++] = tok;
        if (trimmed) {
          Serial.print(F("  aviso: recortado a 32 bytes -> "));
          Serial.println(tok);
        }
      } else {
        overflow = true;
      }
    }
    start = comma + 1;
  }

  if (count == 0) {
    Serial.println(F("  no se encontraron mensajes validos"));
    return;
  }

  const char* ptrs[APFLOOD_MAX_MSGS];
  for (int i = 0; i < count; i++) ptrs[i] = msgs[i].c_str();

  int applied = apFloodSetMessages(ptrs, count);

  Serial.print(F("  ok: AP Flood actualizado con "));
  Serial.print(applied);
  Serial.println(F(" mensajes"));

  if (overflow) {
    Serial.print(F("  aviso: se ignoraron mensajes extra (max "));
    Serial.print(APFLOOD_MAX_MSGS);
    Serial.println(F(")"));
  }
}

static void showFloodMessages() {
  printSection("AP Flood: mensajes activos");
  int n = apFloodGetMessageCount();
  if (n == 0) {
    Serial.println(F("  (ninguno)"));
    return;
  }
  for (int i = 0; i < n; i++) {
    char idx[8];
    snprintf(idx, sizeof(idx), "  [%d]", i + 1);
    Serial.print(idx);
    Serial.print(F("  "));
    Serial.println(apFloodGetMessage(i));
  }
}

// ---------- snifname ----------

static void processSnifNameCommand(const String& cmd) {
  String name = cmd.substring(9);  // largo de "snifname "
  name.trim();
  if (name.length() == 0) {
    Serial.println(F("  uso: snifname <nombre>   (ej: snifname captura)"));
    return;
  }
  snifferSetBaseName(name.c_str());
  Serial.print(F("  ok: nombre base -> "));
  Serial.println(snifferGetBaseName());
  Serial.print(F("  proximas capturas: "));
  Serial.print(snifferGetBaseName());
  Serial.print(F("1.pcap, "));
  Serial.print(snifferGetBaseName());
  Serial.println(F("2.pcap, ..."));
}

static void showSnifName() {
  printSection("Sniffer: nombre base");
  printRow("actual", snifferGetBaseName());
  Serial.print(F("  siguiente:    "));
  Serial.print(snifferGetBaseName());
  Serial.println(F("<N>.pcap (el numero sube solo si ya existe en la SD)"));
}

// ---------- portalsrc ----------

static void processPortalSrcCommand(const String& cmd) {
  String mode = cmd.substring(10);  // largo de "portalsrc "
  mode.trim();
  mode.toLowerCase();

  if (mode == "auto") {
    captiveSetPortalSource(PORTAL_SRC_AUTO);
  } else if (mode == "embebido" || mode == "embedded" || mode == "interno") {
    captiveSetPortalSource(PORTAL_SRC_EMBEDDED);
  } else if (mode == "sd") {
    captiveSetPortalSource(PORTAL_SRC_SD);
  } else if (mode == "fs" || mode == "littlefs" || mode == "interno-fs") {
    captiveSetPortalSource(PORTAL_SRC_FS);
  } else {
    Serial.println(F("  uso: portalsrc auto|embebido|sd|fs"));
    return;
  }
  Serial.print(F("  ok: fuente del portal -> "));
  Serial.println(captivePortalSourceName());
}

static void showPortalSrc() {
  printSection("Captive Portal: fuente del html/css");
  printRow("modo", captivePortalSourceName());
  Serial.println(F("  archivos esperados: /portal/index.html, /portal/style.css,"));
  Serial.println(F("                      /portal/success.html (SD o FS interno)"));
}

// ---------- Procesamiento de comandos ----------

static void processSerialCommand() {
  static String commandBuffer = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        commandBuffer.trim();
        Serial.println();

        if (commandBuffer == "help") {
          printSection("comandos de ajustes");
          Serial.println(F("  help              esta ayuda"));
          Serial.println(F("  status            resumen de la configuracion"));
          Serial.println(F("  flood{...}        mensajes del AP Flood (max 6)"));
          Serial.println(F("  floodshow         lista los mensajes activos"));
          Serial.println(F("  snifname <nombre> nombre base de los .pcap del sniffer"));
          Serial.println(F("  snifshow          muestra el nombre base actual"));
          Serial.println(F("  portalsrc <modo>  auto|embebido|sd|fs"));
          Serial.println(F("  portalshow        muestra la fuente activa del portal"));
          Serial.println(F("  clear             limpiar pantalla"));
          Serial.println(F("  exit              salir de la terminal de ajustes"));
        }
        else if (commandBuffer == "status") {
          printSection("resumen de ajustes");
          char n[8];
          snprintf(n, sizeof(n), "%d", apFloodGetMessageCount());
          printRow("flood: msgs", n);
          printRow("sniffer: base", snifferGetBaseName());
          printRow("portal: fuente", captivePortalSourceName());
        }
        else if (commandBuffer.startsWith("flood{") && commandBuffer.endsWith("}")) {
          processFloodCommand(commandBuffer);
        }
        else if (commandBuffer == "floodshow") {
          showFloodMessages();
        }
        else if (commandBuffer.startsWith("snifname ")) {
          processSnifNameCommand(commandBuffer);
        }
        else if (commandBuffer == "snifshow") {
          showSnifName();
        }
        else if (commandBuffer.startsWith("portalsrc ")) {
          processPortalSrcCommand(commandBuffer);
        }
        else if (commandBuffer == "portalshow") {
          showPortalSrc();
        }
        else if (commandBuffer == "clear" || commandBuffer == "cls") {
          Serial.print(F("\033[2J\033[H"));
        }
        else if (commandBuffer == "exit") {
          Serial.println(F("  saliendo de la terminal de ajustes..."));
          termInitialized = false;
          currentScreen = SCREEN_AJUSTES;
          return;
        }
        else {
          Serial.print(F("  command not found: "));
          Serial.println(commandBuffer);
          Serial.println(F("  escribe 'help' para ver los comandos"));
        }

        commandBuffer = "";
        Serial.println();
        printPrompt();
      }
    }
    else if (c == 8 || c == 127) {
      if (commandBuffer.length() > 0) {
        commandBuffer.remove(commandBuffer.length() - 1);
        Serial.write(8);
        Serial.write(' ');
        Serial.write(8);
      }
    }
    else if (c >= 32 && c <= 126) {
      commandBuffer += c;
      Serial.write(c);
    }
  }
}

static void printBanner() {
  Serial.println();
  Serial.println(F("  ================================"));
  Serial.println(F("   CAMIONETA :: Terminal de Ajustes"));
  Serial.println(F("  ================================"));
  Serial.println(F("  Solo configuracion: AP Flood, nombre del sniffer"));
  Serial.println(F("  y fuente del captive portal. Para el juguete de"));
  Serial.println(F("  siempre (piano, ASCII art, ls/cat SD) anda a"));
  Serial.println(F("  Apps -> PC-Mode."));
  Serial.println(F("  escribe 'help' para ver los comandos."));
}

void screenAjustesTermLoop() {
  if (!termInitialized) {
    printBanner();
    printPrompt();
    termInitialized = true;
  }

  processSerialCommand();

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    termInitialized = false;
    Serial.println();
    Serial.println(F("  saliendo de la terminal de ajustes..."));
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
  u8g2.drawStr(28, 11, "TERM. AJUSTES");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  u8g2.drawXBM(48, 24, 32, 32, image_gear_bits);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(20, 60, "Ver Monitor Serial");

  u8g2.sendBuffer();
}
