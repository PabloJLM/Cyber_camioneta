#include "Ajustes/screen_remote.h"
#include "Drivers/buzzer.h"
#include "Drivers/neopixel.h"
#include "Drivers/settings.h"
#include "Drivers/ftp_server.h"
#include "Apps/screen_apflood.h"
#include "Apps/screen_sniffer.h"
#include "Apps/screen_captive.h"
#include "Apps/screen_btspam.h"
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static const uint16_t REMOTE_TERM_PORT = 2323;

enum RemoteTermMode { RTERM_SELECT, RTERM_REGULAR, RTERM_ADVANCED };

static WiFiServer termServer(REMOTE_TERM_PORT);
static WiFiClient termClient;
static String cmdBuffer;
static bool remoteRunning = false;
static bool staConnected = false;
static RemoteTermMode termMode = RTERM_SELECT;
static bool pianoMode = false;

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

// ---------- helpers compartidos ----------

static void printSection(WiFiClient& c, const char* title) {
  c.println();
  c.print("== ");
  c.println(title);
}

static void printRow(WiFiClient& c, const char* key, const String& value) {
  c.print("  ");
  c.print(key);
  int pad = 14 - strlen(key);
  for (int i = 0; i < pad; i++) c.print(' ');
  c.println(value);
}

static void printPromptRegular(WiFiClient& c) {
  c.print(pianoMode ? "piano> " : "camioneta:~$ ");
}

static void printPromptAdvanced(WiFiClient& c) {
  c.print("camioneta-adv:~$ ");
}

static void listSD(WiFiClient& c) {
  printSection(c, "sd: contenido de /");
  if (!SD.begin(PIN_CD)) { c.println("  SD no disponible"); return; }

  File root = SD.open("/");
  if (!root) { c.println("  no se pudo abrir /"); return; }

  int n = 0;
  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      c.printf("  [dir]  %s\r\n", entry.name());
    } else {
      c.printf("  %8lu  %s\r\n", (unsigned long)entry.size(), entry.name());
    }
    entry.close();
    entry = root.openNextFile();
    n++;
  }
  root.close();
  if (n == 0) c.println("  (vacio)");
}

static void catFile(WiFiClient& c, String path) {
  path.trim();
  if (path.length() == 0) { c.println("  uso: cat <archivo>"); return; }
  if (!path.startsWith("/")) path = "/" + path;

  printSection(c, "sd: cat");
  c.print("  archivo: ");
  c.println(path);

  if (!SD.begin(PIN_CD)) { c.println("  SD no disponible"); return; }
  if (!SD.exists(path)) { c.println("  no existe"); return; }

  File f = SD.open(path, FILE_READ);
  if (!f) { c.println("  no se pudo abrir"); return; }

  c.println("  ---- inicio ----");
  while (f.available()) c.write(f.read());
  f.close();
  c.println();
  c.println("  ---- fin ----");
}

static void showUptime(WiFiClient& c) {
  unsigned long s = millis() / 1000;
  char up[32];
  snprintf(up, sizeof(up), "%luh %02lum %02lus", s / 3600, (s % 3600) / 60, s % 60);
  printSection(c, "uptime");
  printRow(c, "activo desde", String(up));
}

static void showFree(WiFiClient& c) {
  printSection(c, "memoria");
  char v[16];
  snprintf(v, sizeof(v), "%u", ESP.getFreeHeap());
  printRow(c, "heap libre", String(v) + " bytes");
  snprintf(v, sizeof(v), "%u", ESP.getHeapSize());
  printRow(c, "heap total", String(v) + " bytes");
  snprintf(v, sizeof(v), "%u", ESP.getMinFreeHeap());
  printRow(c, "heap minimo", String(v) + " bytes");
}

static void showWhoami(WiFiClient& c) {
  c.println("  root@camioneta (remoto)");
}

static void showUname(WiFiClient& c) {
  printSection(c, "uname");
  printRow(c, "sistema", "camioneta-fw");
  printRow(c, "mcu", "ESP32-C6");
  printRow(c, "core", "arduino-esp32");
}

static void processEchoCommand(WiFiClient& c, const String& cmd) {
  c.println(cmd.substring(5));
}

static void processFloodCommand(WiFiClient& c, const String& cmd) {
  int open  = cmd.indexOf('{');
  int close = cmd.lastIndexOf('}');
  if (open < 0 || close < 0 || close <= open) {
    c.println("  uso: flood{mensaje1,mensaje2,...}");
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
          c.print("  aviso: recortado a 32 bytes -> ");
          c.println(tok);
        }
      } else {
        overflow = true;
      }
    }
    start = comma + 1;
  }

  if (count == 0) {
    c.println("  no se encontraron mensajes validos");
    return;
  }

  const char* ptrs[APFLOOD_MAX_MSGS];
  for (int i = 0; i < count; i++) ptrs[i] = msgs[i].c_str();

  int applied = apFloodSetMessages(ptrs, count);

  c.print("  ok: AP Flood actualizado con ");
  c.print(applied);
  c.println(" mensajes");

  if (overflow) {
    c.print("  aviso: se ignoraron mensajes extra (max ");
    c.print(APFLOOD_MAX_MSGS);
    c.println(")");
  }
}

// ---------- terminal REGULAR (equivalente a PC-Mode) ----------

static int noteFreq(String n) {
  n.trim();
  n.toUpperCase();
  if (n == "DO")  return 262;
  if (n == "RE")  return 294;
  if (n == "MI")  return 330;
  if (n == "FA")  return 349;
  if (n == "SOL") return 392;
  if (n == "LA")  return 440;
  if (n == "SI")  return 494;
  if (n == "DO2" || n == "DO+") return 523;
  if (n == "-" || n == "_")     return 0;
  return -1;
}

static void printPianoHelp(WiFiClient& c) {
  printSection(c, "modo piano");
  c.println("  toca escribiendo notas separadas por espacio.");
  c.println("  ej:  do re mi fa sol la si do2");
  c.println("  notas: DO RE MI FA SOL LA SI DO2 (do agudo)");
  c.println("  '-' = silencio   |   exit = salir del piano");
}

static void processPiano(WiFiClient& c, String in) {
  in.trim();

  if (in == "exit" || in == "salir") {
    pianoMode = false;
    c.println("  saliendo del modo piano");
    return;
  }
  if (in == "help" || in == "?") {
    printPianoHelp(c);
    return;
  }

  int start = 0;
  bool played = false;
  while (start < (int)in.length()) {
    int sp = in.indexOf(' ', start);
    if (sp < 0) sp = in.length();
    String tok = in.substring(start, sp);
    tok.trim();
    if (tok.length() > 0) {
      int f = noteFreq(tok);
      if (f < 0) {
        c.print("  nota desconocida: ");
        c.println(tok);
      } else {
        buzzerNote(f, 300);
        played = true;
      }
    }
    start = sp + 1;
  }

  if (!played) c.println("  (nada que tocar) escribe 'help'");
}

static void showCaptiveLog(WiFiClient& c) {
  printSection(c, "captive log (ultimos 5)");

  if (!SD.begin(PIN_CD)) { c.println("  SD no disponible"); return; }
  if (!SD.exists("/captive_log.txt")) {
    c.println("  sin datos: el portal no ha capturado nada");
    return;
  }

  File logFile = SD.open("/captive_log.txt", FILE_READ);
  if (!logFile) { c.println("  no se pudo abrir el archivo"); return; }

  logFile.readStringUntil('\n');
  logFile.readStringUntil('\n');

  c.println("  time   correo                  ip");

  int count = 0;
  while (logFile.available() && count < 5) {
    String line = logFile.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int comma1 = line.indexOf(',');
      int comma2 = line.indexOf(',', comma1 + 1);
      int comma3 = line.indexOf(',', comma2 + 1);

      if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
        String timestamp = line.substring(0, comma1);
        String correo = line.substring(comma1 + 1, comma2);
        String ip = line.substring(comma3 + 1);

        unsigned long segundos = timestamp.toInt();
        int mins = segundos / 60;
        int secs = segundos % 60;

        c.printf("  %02d:%02d  %-22s  %s\r\n", mins, secs, correo.c_str(), ip.c_str());
        count++;
      }
    }
  }

  if (count == 0) c.println("  sin registros");
  logFile.close();
}

static void showFullCaptiveLog(WiFiClient& c) {
  printSection(c, "captive log (completo)");

  if (!SD.begin(PIN_CD) || !SD.exists("/captive_log.txt")) {
    c.println("  sin datos");
    return;
  }

  File logFile = SD.open("/captive_log.txt", FILE_READ);
  if (logFile) {
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        c.print("  ");
        c.println(line);
      }
    }
    logFile.close();
  }
}

static void clearCaptiveLog(WiFiClient& c) {
  printSection(c, "borrar captive log");

  if (!SD.begin(PIN_CD)) { c.println("  SD no disponible"); return; }

  if (SD.exists("/captive_log.txt")) {
    if (SD.exists("/captive_log_old.txt")) SD.remove("/captive_log_old.txt");
    SD.rename("/captive_log.txt", "/captive_log_old.txt");
    c.println("  backup: captive_log_old.txt");
  }

  File logFile = SD.open("/captive_log.txt", FILE_WRITE);
  if (logFile) {
    logFile.println("=== CAPTIVE PORTAL LOG ===");
    logFile.println("Timestamp,Correo,Telefono,IP");
    logFile.close();
    c.println("  ok: log borrado y reiniciado");
  } else {
    c.println("  error: no se pudo crear el log");
  }
}

static void showCaptiveStats(WiFiClient& c) {
  printSection(c, "estadisticas captive");

  if (!SD.begin(PIN_CD) || !SD.exists("/captive_log.txt")) {
    c.println("  sin datos");
    return;
  }

  File logFile = SD.open("/captive_log.txt", FILE_READ);
  if (logFile) {
    logFile.readStringUntil('\n');
    logFile.readStringUntil('\n');

    int total = 0;
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) total++;
    }
    logFile.close();

    printRow(c, "total", String(total));

    logFile = SD.open("/captive_log.txt", FILE_READ);
    logFile.readStringUntil('\n');
    logFile.readStringUntil('\n');

    String lastLine;
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) lastLine = line;
    }
    logFile.close();

    if (lastLine.length() > 0) {
      int comma1 = lastLine.indexOf(',');
      int comma2 = lastLine.indexOf(',', comma1 + 1);
      int comma3 = lastLine.indexOf(',', comma2 + 1);

      if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
        String correo = lastLine.substring(comma1 + 1, comma2);
        String telefono = lastLine.substring(comma2 + 1, comma3);
        printRow(c, "ult. correo", correo.substring(0, 24));
        printRow(c, "ult. tel", telefono);
      }
    }
  }
}

static void printHelpRegular(WiFiClient& c) {
  printSection(c, "comandos (terminal regular)");
  c.println("  help       esta ayuda");
  c.println("  status     estado del sistema");
  c.println("  info       informacion del device");
  c.println("  rgb/R/G/B  control de NeoPixels");
  c.println("  buzzer     prueba del buzzer");
  c.println("  piano      mini piano por terminal");
  c.println("  ls         listar archivos de la SD");
  c.println("  cat <arch> ver un archivo de la SD");
  c.println("  flood{...} SSIDs del AP Flood (max 6)");
  c.println("  logcap     ultimos 5 logs del portal");
  c.println("  logfull    todos los logs");
  c.println("  logclear   borrar log");
  c.println("  logstats   estadisticas");
  c.println("  uptime     tiempo activo");
  c.println("  free       memoria heap disponible");
  c.println("  whoami     usuario actual");
  c.println("  uname      info del sistema/mcu");
  c.println("  echo <txt> repite el texto");
  c.println("  reboot     reinicia el equipo");
  c.println("  clear      limpiar pantalla");
  c.println("  exit       volver al menu de terminal");
}

static void handleRegularCommand(WiFiClient& c, String cmd) {
  cmd.trim();

  if (pianoMode) {
    if (cmd.length() > 0) {
      c.println();
      processPiano(c, cmd);
      c.println();
      printPromptRegular(c);
    }
    return;
  }

  if (cmd.length() == 0) return;
  c.println();

  if (cmd == "help") {
    printHelpRegular(c);
  }
  else if (cmd == "status") {
    printSection(c, "estado del sistema");
    char rgb[16];
    snprintf(rgb, sizeof(rgb), "%d, %d, %d", neoR, neoG, neoB);
    printRow(c, "rgb", String(rgb));
    unsigned long s = millis() / 1000;
    char up[24];
    snprintf(up, sizeof(up), "%luh %02lum %02lus", s / 3600, (s % 3600) / 60, s % 60);
    printRow(c, "uptime", String(up));
    printRow(c, "neopixels", String(NUM_PIXELS));
  }
  else if (cmd == "info") {
    printSection(c, "device info");
    printRow(c, "proyecto", "Camioneta");
    printRow(c, "version", "1.0");
    printRow(c, "autor", "Pablo Lopez");
    printRow(c, "lab", "Tesla Lab");
    printRow(c, "neopixels", String(NUM_PIXELS));
    printRow(c, "display", "SH1106 128x64");
    printRow(c, "mcu", "ESP32");
  }
  else if (cmd.startsWith("rgb/")) {
    int r, g, b;
    if (sscanf(cmd.c_str(), "rgb/%d/%d/%d", &r, &g, &b) == 3) {
      r = constrain(r, 0, 255);
      g = constrain(g, 0, 255);
      b = constrain(b, 0, 255);
      neoR = r; neoG = g; neoB = b;
      for (int i = 0; i < NUM_PIXELS; i++) neopixelSetPixel(i, r, g, b);
      neopixelShow();
      c.printf("  ok: rgb -> %d,%d,%d\r\n", r, g, b);
    } else {
      c.println("  error: usa rgb/R/G/B");
    }
  }
  else if (cmd == "buzzer") {
    c.println("  probando buzzer...");
    buzzerBeep();
    delay(750);
    buzzerBeep();
    c.println("  ok: test completado");
  }
  else if (cmd == "piano") {
    pianoMode = true;
    printPianoHelp(c);
  }
  else if (cmd == "ls" || cmd == "dir") {
    listSD(c);
  }
  else if (cmd.startsWith("cat ")) {
    catFile(c, cmd.substring(4));
  }
  else if (cmd.startsWith("flood{") && cmd.endsWith("}")) {
    processFloodCommand(c, cmd);
  }
  else if (cmd == "logcap")   { showCaptiveLog(c); }
  else if (cmd == "logfull")  { showFullCaptiveLog(c); }
  else if (cmd == "logclear") { clearCaptiveLog(c); }
  else if (cmd == "logstats") { showCaptiveStats(c); }
  else if (cmd == "uptime")   { showUptime(c); }
  else if (cmd == "free")     { showFree(c); }
  else if (cmd == "whoami")   { showWhoami(c); }
  else if (cmd == "uname")    { showUname(c); }
  else if (cmd.startsWith("echo ")) {
    processEchoCommand(c, cmd);
  }
  else if (cmd == "reboot") {
    c.println("  reiniciando...");
    c.flush();
    delay(200);
    ESP.restart();
  }
  else if (cmd == "clear" || cmd == "cls") {
    c.print("\033[2J\033[H");
  }
  else if (cmd == "exit") {
    c.println("  volviendo al menu de terminal...");
    termMode = RTERM_SELECT;
    c.println();
    c.println("Elegi terminal:");
    c.println("  1) Regular   (PC-Mode: piano, ls, cat, flood...)");
    c.println("  2) Avanzada  (Ajustes: wifi, portal, sniffer...)");
    c.print("Opcion (1/2): ");
    return;
  }
  else {
    c.print("  command not found: ");
    c.println(cmd);
    c.println("  escribe 'help' para ver los comandos");
  }

  c.println();
  printPromptRegular(c);
}

// ---------- terminal AVANZADA (equivalente a Ajustes Term) ----------

static void showFloodMessages(WiFiClient& c) {
  printSection(c, "AP Flood: mensajes activos");
  int n = apFloodGetMessageCount();
  if (n == 0) { c.println("  (ninguno)"); return; }
  for (int i = 0; i < n; i++) {
    char idx[8];
    snprintf(idx, sizeof(idx), "  [%d]", i + 1);
    c.print(idx);
    c.print("  ");
    c.println(apFloodGetMessage(i));
  }
}

static void processFloodIntervalCommand(WiFiClient& c, const String& cmd) {
  String arg = cmd.substring(16);
  arg.trim();
  if (arg.length() == 0) {
    c.println("  uso: apflood interval <ms>   (0 = lo mas rapido)");
    return;
  }
  long ms = arg.toInt();
  if (ms < 0) ms = 0;
  if (ms > 65535) ms = 65535;
  apFloodSetInterval((uint16_t)ms);
  c.print("  ok: intervalo de AP Flood -> ");
  if (ms == 0) c.println("maximo (sin espera)");
  else { c.print(ms); c.println(" ms"); }
}

static void processSnifNameCommand(WiFiClient& c, const String& cmd) {
  String name = cmd.substring(9);
  name.trim();
  if (name.length() == 0) {
    c.println("  uso: snifname <nombre>   (ej: snifname captura)");
    return;
  }
  snifferSetBaseName(name.c_str());
  c.print("  ok: nombre base -> ");
  c.println(snifferGetBaseName());
}

static void showSnifName(WiFiClient& c) {
  printSection(c, "Sniffer: ajustes");
  printRow(c, "nombre base", snifferGetBaseName());
  printRow(c, "canal inicial", String(snifferGetChannel()));
}

static void processSnifChannelCommand(WiFiClient& c, const String& cmd) {
  String arg = cmd.substring(15);
  arg.trim();
  if (arg.length() == 0) {
    c.println("  uso: sniffer channel <1-13>");
    return;
  }
  int ch = arg.toInt();
  if (ch < 1 || ch > 13) {
    c.println("  error: el canal debe ser 1-13");
    return;
  }
  snifferSetChannel((uint8_t)ch);
  c.print("  ok: canal inicial del sniffer -> ");
  c.println(ch);
}

static void processPortalSrcCommand(WiFiClient& c, const String& cmd) {
  String mode = cmd.substring(10);
  mode.trim();
  mode.toLowerCase();

  if (mode == "auto") captiveSetPortalSource(PORTAL_SRC_AUTO);
  else if (mode == "embebido" || mode == "embedded" || mode == "interno") captiveSetPortalSource(PORTAL_SRC_EMBEDDED);
  else if (mode == "sd") captiveSetPortalSource(PORTAL_SRC_SD);
  else if (mode == "fs" || mode == "littlefs" || mode == "interno-fs") captiveSetPortalSource(PORTAL_SRC_FS);
  else { c.println("  uso: portalsrc auto|embebido|sd|fs"); return; }

  c.print("  ok: fuente del portal -> ");
  c.println(captivePortalSourceName());
}

static void processPortalSSIDCommand(WiFiClient& c, const String& cmd) {
  String name = cmd.substring(12);
  name.trim();
  if (name.length() == 0) {
    c.println("  uso: portal ssid <nombre>   (ej: portal ssid WiFi_Casa)");
    return;
  }
  captiveSetSSID(name.c_str());
  c.print("  ok: SSID del portal -> ");
  c.println(captiveGetSSID());
}

static void showPortalSrc(WiFiClient& c) {
  printSection(c, "Captive Portal: ajustes");
  printRow(c, "SSID", captiveGetSSID());
  printRow(c, "fuente html", captivePortalSourceName());
}

static void showBtSpamList(WiFiClient& c) {
  printSection(c, "BT Spam: fabricantes en rotacion");
  int total = btSpamGetTypeCount();
  for (int i = 0; i < total; i++) {
    char line[40];
    const char* mark = btSpamIsTypeEnabled(i) ? "x" : " ";
    const char* warn = btSpamIsTypeUnstable(i) ? "  (puede reiniciar el equipo)" : "";
    snprintf(line, sizeof(line), "  [%s] %s%s", mark, btSpamGetTypeName(i), warn);
    c.println(line);
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

static void processBtSpamCommand(WiFiClient& c, const String& cmd, bool enable) {
  String name = cmd.substring(enable ? 14 : 15);
  name.trim();
  if (name.length() == 0) {
    c.println("  uso: btspam enable|disable <fabricante>");
    return;
  }
  int idx = findBtSpamTypeByName(name);
  if (idx < 0) {
    c.print("  error: fabricante desconocido: ");
    c.println(name);
    c.println("  usa 'btspam list' para ver los nombres validos");
    return;
  }
  btSpamSetTypeEnabled(idx, enable);
  c.print("  ok: ");
  c.print(btSpamGetTypeName(idx));
  c.println(enable ? " -> activado" : " -> desactivado");
}

static void processWifiSSIDCommand(WiFiClient& c, const String& cmd) {
  String name = cmd.substring(10);
  name.trim();
  if (name.length() == 0) {
    c.println("  uso: wifi ssid <nombre>   (red a la que se une en modo STA)");
    return;
  }
  settingsSetWifiSSID(name.c_str());
  c.print("  ok: SSID WiFi -> ");
  c.println(name);
}

static void processWifiPassCommand(WiFiClient& c, const String& cmd) {
  String pass = cmd.substring(10);
  pass.trim();
  if (pass.length() == 0) { c.println("  uso: wifi pass <clave>"); return; }
  settingsSetWifiPass(pass.c_str());
  c.println("  ok: clave WiFi guardada");
}

static void processWifiModeCommand(WiFiClient& c, const String& cmd) {
  String mode = cmd.substring(10);
  mode.trim();
  mode.toLowerCase();
  if (mode == "ap") {
    settingsSetRemoteMode(0);
    c.println("  ok: modo remoto -> AP propio");
  } else if (mode == "sta") {
    settingsSetRemoteMode(1);
    c.println("  ok: modo remoto -> STA (se une a tu WiFi)");
  } else {
    c.println("  uso: wifi mode ap|sta");
  }
}

static void processWifiApSSIDCommand(WiFiClient& c, const String& cmd) {
  String name = cmd.substring(12);
  name.trim();
  if (name.length() == 0) { c.println("  uso: wifi apssid <nombre>"); return; }
  settingsSetRemoteApSSID(name.c_str());
  c.print("  ok: SSID del AP propio -> ");
  c.println(name);
}

static void processWifiApPassCommand(WiFiClient& c, const String& cmd) {
  String pass = cmd.substring(12);
  pass.trim();
  if (pass.length() == 0) { c.println("  uso: wifi appass <clave>   (min 8 caracteres)"); return; }
  settingsSetRemoteApPass(pass.c_str());
  c.println("  ok: clave del AP propio guardada");
}

static void showWifi(WiFiClient& c) {
  printSection(c, "WiFi remoto: ajustes");
  char ssid[32];
  char apSsid[32];
  settingsGetWifiSSID(ssid, sizeof(ssid));
  settingsGetRemoteApSSID(apSsid, sizeof(apSsid));
  printRow(c, "modo", settingsGetRemoteMode() == 1 ? "STA (WiFi existente)" : "AP propio");
  printRow(c, "SSID AP propio", apSsid);
  printRow(c, "SSID (modo STA)", strlen(ssid) ? String(ssid) : String("(sin configurar)"));
}

static void printHelpAdvanced(WiFiClient& c) {
  printSection(c, "comandos (terminal avanzada)");
  c.println("  help                    esta ayuda");
  c.println("  status                  resumen de configuracion");
  c.println("  flood{...}              mensajes del AP Flood (max 6)");
  c.println("  floodshow               lista los mensajes activos");
  c.println("  apflood interval <ms>   intervalo entre tandas de beacons");
  c.println("  snifname <nombre>       nombre base de los .pcap");
  c.println("  sniffer channel <1-13>  canal inicial del sniffer");
  c.println("  snifshow                ajustes actuales del sniffer");
  c.println("  portalsrc <modo>        auto|embebido|sd|fs");
  c.println("  portal ssid <nombre>    nombre de red del captive portal");
  c.println("  portalshow              ajustes actuales del portal");
  c.println("  btspam list             fabricantes activos");
  c.println("  btspam enable <nombre>  activa un fabricante");
  c.println("  btspam disable <nombre> desactiva un fabricante");
  c.println("  wifi ssid <nombre>      red WiFi a usar en modo STA");
  c.println("  wifi pass <clave>       clave de esa red");
  c.println("  wifi mode ap|sta        modo del terminal/FTP remoto");
  c.println("  wifi apssid <nombre>    SSID del AP propio");
  c.println("  wifi appass <clave>     clave del AP propio");
  c.println("  wifishow                ajustes actuales de WiFi remoto");
  c.println("  uptime                  tiempo activo");
  c.println("  free                    memoria heap disponible");
  c.println("  echo <texto>            repite el texto");
  c.println("  reboot                  reinicia el equipo");
  c.println("  clear                   limpiar pantalla");
  c.println("  exit                    volver al menu de terminal");
}

static void handleAdvancedCommand(WiFiClient& c, String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  c.println();

  if (cmd == "help") {
    printHelpAdvanced(c);
  }
  else if (cmd == "status") {
    printSection(c, "resumen de ajustes");
    printRow(c, "flood: msgs", String(apFloodGetMessageCount()));
    uint16_t interval = apFloodGetInterval();
    printRow(c, "flood: intervalo", interval == 0 ? String("max") : String(interval) + " ms");
    printRow(c, "sniffer: base", snifferGetBaseName());
    printRow(c, "sniffer: canal", String(snifferGetChannel()));
    printRow(c, "portal: SSID", captiveGetSSID());
    printRow(c, "portal: fuente", captivePortalSourceName());
    int enabled = 0;
    int total = btSpamGetTypeCount();
    for (int i = 0; i < total; i++) if (btSpamIsTypeEnabled(i)) enabled++;
    printRow(c, "btspam: activos", String(enabled) + "/" + String(total));
    printRow(c, "wifi remoto", settingsGetRemoteMode() == 1 ? "STA" : "AP");
  }
  else if (cmd.startsWith("flood{") && cmd.endsWith("}")) processFloodCommand(c, cmd);
  else if (cmd == "floodshow") showFloodMessages(c);
  else if (cmd.startsWith("apflood interval")) processFloodIntervalCommand(c, cmd);
  else if (cmd.startsWith("snifname ")) processSnifNameCommand(c, cmd);
  else if (cmd.startsWith("sniffer channel")) processSnifChannelCommand(c, cmd);
  else if (cmd == "snifshow") showSnifName(c);
  else if (cmd.startsWith("portalsrc ")) processPortalSrcCommand(c, cmd);
  else if (cmd.startsWith("portal ssid ")) processPortalSSIDCommand(c, cmd);
  else if (cmd == "portalshow") showPortalSrc(c);
  else if (cmd == "btspam list") showBtSpamList(c);
  else if (cmd.startsWith("btspam enable "))  processBtSpamCommand(c, cmd, true);
  else if (cmd.startsWith("btspam disable ")) processBtSpamCommand(c, cmd, false);
  else if (cmd.startsWith("wifi ssid "))   processWifiSSIDCommand(c, cmd);
  else if (cmd.startsWith("wifi pass "))   processWifiPassCommand(c, cmd);
  else if (cmd.startsWith("wifi mode "))   processWifiModeCommand(c, cmd);
  else if (cmd.startsWith("wifi apssid ")) processWifiApSSIDCommand(c, cmd);
  else if (cmd.startsWith("wifi appass ")) processWifiApPassCommand(c, cmd);
  else if (cmd == "wifishow") showWifi(c);
  else if (cmd == "uptime") showUptime(c);
  else if (cmd == "free") showFree(c);
  else if (cmd.startsWith("echo ")) processEchoCommand(c, cmd);
  else if (cmd == "reboot") {
    c.println("  reiniciando...");
    c.flush();
    delay(200);
    ESP.restart();
  }
  else if (cmd == "clear" || cmd == "cls") {
    c.print("\033[2J\033[H");
  }
  else if (cmd == "exit") {
    c.println("  volviendo al menu de terminal...");
    termMode = RTERM_SELECT;
    c.println();
    c.println("Elegi terminal:");
    c.println("  1) Regular   (PC-Mode: piano, ls, cat, flood...)");
    c.println("  2) Avanzada  (Ajustes: wifi, portal, sniffer...)");
    c.print("Opcion (1/2): ");
    return;
  }
  else {
    c.print("  command not found: ");
    c.println(cmd);
    c.println("  escribe 'help' para ver los comandos");
  }

  c.println();
  printPromptAdvanced(c);
}

// ---------- seleccion de terminal ----------

static void printSelectMenu(WiFiClient& c) {
  c.println();
  c.println("Elegi terminal:");
  c.println("  1) Regular   (PC-Mode: piano, ls, cat, flood...)");
  c.println("  2) Avanzada  (Ajustes: wifi, portal, sniffer...)");
  c.print("Opcion (1/2): ");
}

static void handleSelect(WiFiClient& c, String raw) {
  raw.trim();
  raw.toLowerCase();
  c.println();

  if (raw == "1" || raw == "regular") {
    termMode = RTERM_REGULAR;
    pianoMode = false;
    c.println("Terminal Regular (PC-Mode). Escribe 'help'.");
    c.println();
    printPromptRegular(c);
  } else if (raw == "2" || raw == "avanzada" || raw == "avz" || raw == "advanced") {
    termMode = RTERM_ADVANCED;
    c.println("Terminal Avanzada (Ajustes). Escribe 'help'.");
    c.println();
    printPromptAdvanced(c);
  } else {
    c.println("Opcion invalida. Escribe 1 o 2.");
    c.print("Opcion (1/2): ");
  }
}

// ---------- servicio de conexion ----------

static void serviceTerminal() {
  if (!termClient || !termClient.connected()) {
    WiFiClient incoming = termServer.available();
    if (incoming) {
      if (termClient && termClient.connected()) {
        incoming.println("camioneta: ya hay una sesion activa, intenta luego.");
        incoming.stop();
      } else {
        termClient = incoming;
        cmdBuffer = "";
        termMode = RTERM_SELECT;
        pianoMode = false;
        termClient.println();
        termClient.println("Camioneta - Terminal remota");
        printSelectMenu(termClient);
      }
    }
    return;
  }

  while (termClient.available() > 0) {
    char c = termClient.read();

    if (c == '\n' || c == '\r') {
      if (cmdBuffer.length() > 0) {
        String cmd = cmdBuffer;
        cmdBuffer = "";
        if (termMode == RTERM_SELECT) {
          handleSelect(termClient, cmd);
        } else if (termMode == RTERM_REGULAR) {
          handleRegularCommand(termClient, cmd);
        } else {
          handleAdvancedCommand(termClient, cmd);
        }
        if (!termClient.connected()) return;
      }
    }
    else if (c == 8 || c == 127) {
      if (cmdBuffer.length() > 0) {
        cmdBuffer.remove(cmdBuffer.length() - 1);
        termClient.write((uint8_t)8);
        termClient.write((uint8_t)' ');
        termClient.write((uint8_t)8);
      }
    }
    else if (c >= 32 && c <= 126) {
      cmdBuffer += c;
      termClient.write((uint8_t)c);
    }
  }
}

static void startRemote() {
  if (remoteRunning) return;

  Serial.println(F("\nRemoto: iniciando"));

  uint8_t mode = settingsGetRemoteMode();
  staConnected = false;

  if (mode == 1) {
    char ssid[32];
    char pass[64];
    settingsGetWifiSSID(ssid, sizeof(ssid));
    settingsGetWifiPass(pass, sizeof(pass));

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
      delay(200);
    }
    staConnected = (WiFi.status() == WL_CONNECTED);

    if (!staConnected) {
      Serial.println(F("Remoto: no se pudo conectar al WiFi configurado"));
      WiFi.mode(WIFI_OFF);
      return;
    }
    Serial.print(F("IP:   ")); Serial.println(WiFi.localIP());
  } else {
    char apSsid[32];
    char apPass[64];
    settingsGetRemoteApSSID(apSsid, sizeof(apSsid));
    settingsGetRemoteApPass(apPass, sizeof(apPass));

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPass);
    delay(100);
    Serial.print(F("IP:   ")); Serial.println(WiFi.softAPIP());
    Serial.print(F("SSID: ")); Serial.println(apSsid);
  }

  termServer.begin();
  ftpServerBegin();
  remoteRunning = true;
  Serial.println(F("Remoto: activo"));
}

static void stopRemote() {
  if (!remoteRunning) return;

  if (termClient) termClient.stop();
  termServer.end();
  ftpServerEnd();

  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  remoteRunning = false;
  staConnected = false;
  Serial.println(F("Remoto: detenido"));
}

void screenRemoteLoop() {
  if (remoteRunning) {
    serviceTerminal();
    ftpServerLoop();
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (remoteRunning) stopRemote();
    currentScreen = SCREEN_AJUSTES;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (remoteRunning) stopRemote();
    else startRemote();
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  {
    const char* title = "Con. Remota";
    int tw = u8g2.getStrWidth(title);
    u8g2.drawStr(16 + (96 - tw) / 2, 11, title);
  }
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  uint8_t mode = settingsGetRemoteMode();

  if (remoteRunning) {
    u8g2.drawStr(10, 24, "Estado: ACTIVO");
    u8g2.drawStr(10, 36, mode == 1 ? "Modo: STA" : "Modo: AP");

    u8g2.setFont(u8g2_font_5x7_tr);

    IPAddress ip = (mode == 1) ? WiFi.localIP() : WiFi.softAPIP();
    char ipBuf[32];
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d:%u", ip[0], ip[1], ip[2], ip[3], REMOTE_TERM_PORT);
    u8g2.drawStr(8, 48, ipBuf);

    u8g2.drawStr(5, 60, "SEL:Detener BACK:Salir");
  } else {
    u8g2.drawStr(11, 24, "Estado: INACTIVO");

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(12, 38, "Terminal por WiFi");

    u8g2.drawStr(1, 59, "SEL:Iniciar BACK:Salir");
  }

  u8g2.sendBuffer();
}
