#include "Ajustes/screen_ajustes_term.h"
#include "Drivers/buzzer.h"
#include "Apps/screen_apflood.h"
#include "Apps/screen_sniffer.h"
#include "Apps/screen_captive.h"
#include "Apps/screen_btspam.h"
#include <esp_system.h>


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
  Serial.print(F("Ajustes:~$ "));
}


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

static void processFloodIntervalCommand(const String& cmd) {
  String arg = cmd.substring(16);   // largo de "apflood interval"
  arg.trim();
  if (arg.length() == 0) {
    Serial.println(F("  uso: apflood interval <ms>   (0 = lo mas rapido)"));
    return;
  }
  long ms = arg.toInt();
  if (ms < 0) ms = 0;
  if (ms > 65535) ms = 65535;
  apFloodSetInterval((uint16_t)ms);
  Serial.print(F("  ok: intervalo de AP Flood -> "));
  if (ms == 0) Serial.println(F("maximo (sin espera)"));
  else { Serial.print(ms); Serial.println(F(" ms")); }
}


static void processSnifNameCommand(const String& cmd) {
  String name = cmd.substring(9);
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
  printSection("Sniffer: ajustes");
  printRow("nombre base", snifferGetBaseName());
  char ch[4];
  snprintf(ch, sizeof(ch), "%d", snifferGetChannel());
  printRow("canal inicial", ch);
  Serial.print(F("  siguiente:    "));
  Serial.print(snifferGetBaseName());
  Serial.println(F("<N>.pcap (el numero sube solo si ya existe en la SD)"));
}

static void processSnifChannelCommand(const String& cmd) {
  String arg = cmd.substring(15);   // largo de "sniffer channel"
  arg.trim();
  if (arg.length() == 0) {
    Serial.println(F("  uso: sniffer channel <1-13>"));
    return;
  }
  int ch = arg.toInt();
  if (ch < 1 || ch > 13) {
    Serial.println(F("  error: el canal debe ser 1-13"));
    return;
  }
  snifferSetChannel((uint8_t)ch);
  Serial.print(F("  ok: canal inicial del sniffer -> "));
  Serial.println(ch);
}


static void processPortalSrcCommand(const String& cmd) {
  String mode = cmd.substring(10);
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

static void processPortalSSIDCommand(const String& cmd) {
  String name = cmd.substring(12);   // largo de "portal ssid "
  name.trim();
  if (name.length() == 0) {
    Serial.println(F("  uso: portal ssid <nombre>   (ej: portal ssid WiFi_Casa)"));
    return;
  }
  captiveSetSSID(name.c_str());
  Serial.print(F("  ok: SSID del portal -> "));
  Serial.println(captiveGetSSID());
}

static void showPortalSrc() {
  printSection("Captive Portal: ajustes");
  printRow("SSID", captiveGetSSID());
  printRow("fuente html", captivePortalSourceName());
  Serial.println(F("  archivos esperados: /portal/index.html, /portal/style.css,"));
  Serial.println(F("                      /portal/success.html (SD o FS interno)"));
}


static void showBtSpamList() {
  printSection("BT Spam: fabricantes en rotacion");
  int total = btSpamGetTypeCount();
  for (int i = 0; i < total; i++) {
    char line[40];
    const char* mark = btSpamIsTypeEnabled(i) ? "x" : " ";
    const char* warn = btSpamIsTypeUnstable(i) ? "  (puede reiniciar el equipo)" : "";
    snprintf(line, sizeof(line), "  [%s] %s%s", mark, btSpamGetTypeName(i), warn);
    Serial.println(line);
  }
}

static int findBtSpamTypeByName(const String& name) {
  int total = btSpamGetTypeCount();
  String lower = name;
  lower.toLowerCase();
  for (int i = 0; i < total; i++) {
    String candidate = btSpamGetTypeName(i);
    candidate.toLowerCase();
    if (candidate == lower) return i;
  }
  return -1;
}

static void processBtSpamCommand(const String& cmd, bool enable) {
  String name = cmd.substring(enable ? 14 : 15);   // "btspam enable "/"btspam disable "
  name.trim();
  if (name.length() == 0) {
    Serial.println(F("  uso: btspam enable|disable <fabricante>"));
    return;
  }
  int idx = findBtSpamTypeByName(name);
  if (idx < 0) {
    Serial.print(F("  error: fabricante desconocido: "));
    Serial.println(name);
    Serial.println(F("  usa 'btspam list' para ver los nombres validos"));
    return;
  }
  btSpamSetTypeEnabled(idx, enable);
  Serial.print(F("  ok: "));
  Serial.print(btSpamGetTypeName(idx));
  Serial.println(enable ? F(" -> activado") : F(" -> desactivado"));
}


static void processEchoCommand(const String& cmd) {
  Serial.println(cmd.substring(5));
}

static void showUptime() {
  unsigned long s = millis() / 1000;
  char up[32];
  snprintf(up, sizeof(up), "%luh %02lum %02lus", s / 3600, (s % 3600) / 60, s % 60);
  printSection("uptime");
  printRow("activo desde", String(up));
}

static void showFree() {
  printSection("memoria");
  char v[16];
  snprintf(v, sizeof(v), "%u", ESP.getFreeHeap());
  printRow("heap libre", String(v) + " bytes");
  snprintf(v, sizeof(v), "%u", ESP.getHeapSize());
  printRow("heap total", String(v) + " bytes");
  snprintf(v, sizeof(v), "%u", ESP.getMinFreeHeap());
  printRow("heap minimo", String(v) + " bytes");
}


static void processSerialCommand() {
  static String commandBuffer = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        commandBuffer.trim();
        Serial.println();

        if (commandBuffer == "help") {
          printSection("comandos de ajustes (terminal avanzada)");
          Serial.println(F("  help                    esta ayuda"));
          Serial.println(F("  status                  resumen de toda la configuracion"));
          Serial.println(F("  flood{...}              mensajes del AP Flood (max 6)"));
          Serial.println(F("  floodshow               lista los mensajes activos"));
          Serial.println(F("  apflood interval <ms>   intervalo entre tandas de beacons"));
          Serial.println(F("  snifname <nombre>       nombre base de los .pcap del sniffer"));
          Serial.println(F("  sniffer channel <1-13>  canal inicial del sniffer"));
          Serial.println(F("  snifshow                ajustes actuales del sniffer"));
          Serial.println(F("  portalsrc <modo>        auto|embebido|sd|fs"));
          Serial.println(F("  portal ssid <nombre>    nombre de red del captive portal"));
          Serial.println(F("  portalshow              ajustes actuales del captive portal"));
          Serial.println(F("  btspam list             fabricantes activos en BT Spam"));
          Serial.println(F("  btspam enable <nombre>  activa un fabricante"));
          Serial.println(F("  btspam disable <nombre> desactiva un fabricante"));
          Serial.println(F("  uptime                  tiempo activo desde el ultimo reinicio"));
          Serial.println(F("  free                    memoria heap disponible"));
          Serial.println(F("  echo <texto>            repite el texto"));
          Serial.println(F("  reboot                  reinicia el equipo"));
          Serial.println(F("  clear                   limpiar pantalla"));
          Serial.println(F("  exit                    salir de la terminal de ajustes"));
        }
        else if (commandBuffer == "status") {
          printSection("resumen de ajustes");
          char n[8];
          snprintf(n, sizeof(n), "%d", apFloodGetMessageCount());
          printRow("flood: msgs", n);
          uint16_t interval = apFloodGetInterval();
          printRow("flood: intervalo", interval == 0 ? String("max") : String(interval) + " ms");
          printRow("sniffer: base", snifferGetBaseName());
          printRow("sniffer: canal", String(snifferGetChannel()));
          printRow("portal: SSID", captiveGetSSID());
          printRow("portal: fuente", captivePortalSourceName());
          int enabled = 0;
          int total = btSpamGetTypeCount();
          for (int i = 0; i < total; i++) if (btSpamIsTypeEnabled(i)) enabled++;
          printRow("btspam: activos", String(enabled) + "/" + String(total));
        }
        else if (commandBuffer.startsWith("flood{") && commandBuffer.endsWith("}")) {
          processFloodCommand(commandBuffer);
        }
        else if (commandBuffer == "floodshow") {
          showFloodMessages();
        }
        else if (commandBuffer.startsWith("apflood interval")) {
          processFloodIntervalCommand(commandBuffer);
        }
        else if (commandBuffer.startsWith("snifname ")) {
          processSnifNameCommand(commandBuffer);
        }
        else if (commandBuffer.startsWith("sniffer channel")) {
          processSnifChannelCommand(commandBuffer);
        }
        else if (commandBuffer == "snifshow") {
          showSnifName();
        }
        else if (commandBuffer.startsWith("portalsrc ")) {
          processPortalSrcCommand(commandBuffer);
        }
        else if (commandBuffer.startsWith("portal ssid ")) {
          processPortalSSIDCommand(commandBuffer);
        }
        else if (commandBuffer == "portalshow") {
          showPortalSrc();
        }
        else if (commandBuffer == "btspam list") {
          showBtSpamList();
        }
        else if (commandBuffer.startsWith("btspam enable ")) {
          processBtSpamCommand(commandBuffer, true);
        }
        else if (commandBuffer.startsWith("btspam disable ")) {
          processBtSpamCommand(commandBuffer, false);
        }
        else if (commandBuffer == "uptime") {
          showUptime();
        }
        else if (commandBuffer == "free") {
          showFree();
        }
        else if (commandBuffer.startsWith("echo ")) {
          processEchoCommand(commandBuffer);
        }
        else if (commandBuffer == "reboot") {
          Serial.println(F("  reiniciando..."));
          delay(200);
          ESP.restart();
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
  Serial.println(F("   Terminal de Ajustes (avanzada)"));
  Serial.println(F("  Configuracion de cada app: AP Flood, Sniffer,"));
  Serial.println(F("  Captive Portal y BT Spam."));
  Serial.println(F("  Escribe 'help' para ver los comandos."));
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

  u8g2.drawXBM(47, 16, 32, 32, image_gear_bits);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(20, 54, "Ver Monitor Serial");

  u8g2.sendBuffer();
}
