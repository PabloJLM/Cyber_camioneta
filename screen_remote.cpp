#include "Ajustes/screen_remote.h"
#include "Drivers/buzzer.h"
#include "Drivers/settings.h"
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
static const char* AP_SSID = "Camioneta-Remote";
static const char* AP_PASS = "123456789";

static WiFiServer termServer(REMOTE_TERM_PORT);
static WiFiClient termClient;
static String cmdBuffer;
static bool remoteRunning = false;
static bool staConnected = false;

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

static void printPrompt(WiFiClient& c) {
  c.print("camioneta-remote:~$ ");
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
      c.printf("  [dir]  %s\n", entry.name());
    } else {
      c.printf("  %8lu  %s\n", (unsigned long)entry.size(), entry.name());
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

static void printHelp(WiFiClient& c) {
  printSection(c, "comandos (terminal remota)");
  c.println("  help       esta ayuda");
  c.println("  status     estado del sistema");
  c.println("  info       informacion del device");
  c.println("  ls         listar archivos de la SD");
  c.println("  cat <arch> ver un archivo de la SD");
  c.println("  uptime     tiempo activo desde el ultimo reinicio");
  c.println("  free       memoria heap disponible");
  c.println("  whoami     usuario actual");
  c.println("  uname      info del sistema/mcu");
  c.println("  echo <txt> repite el texto");
  c.println("  reboot     reinicia el equipo");
  c.println("  clear      limpiar pantalla");
  c.println("  exit       cierra esta conexion");
  c.println();
  c.println("  archivos: usa FTP (mismo modo de red) para subir/bajar de la SD.");
}

static void handleCommand(WiFiClient& c, String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  c.println();

  if (cmd == "help") {
    printHelp(c);
  }
  else if (cmd == "status") {
    printSection(c, "estado del sistema");
    unsigned long s = millis() / 1000;
    char up[24];
    snprintf(up, sizeof(up), "%luh %02lum %02lus", s / 3600, (s % 3600) / 60, s % 60);
    printRow(c, "uptime", String(up));
    printRow(c, "ip", (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
  }
  else if (cmd == "info") {
    printSection(c, "device info");
    printRow(c, "proyecto", "Camioneta");
    printRow(c, "version", "1.0");
    printRow(c, "autor", "Pablo Lopez");
    printRow(c, "lab", "Tesla Lab");
    printRow(c, "display", "SH1106 128x64");
    printRow(c, "mcu", "ESP32");
  }
  else if (cmd == "ls" || cmd == "dir") {
    listSD(c);
  }
  else if (cmd.startsWith("cat ")) {
    catFile(c, cmd.substring(4));
  }
  else if (cmd == "uptime") {
    showUptime(c);
  }
  else if (cmd == "free") {
    showFree(c);
  }
  else if (cmd == "whoami") {
    showWhoami(c);
  }
  else if (cmd == "uname") {
    showUname(c);
  }
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
    c.println("  cerrando conexion...");
    c.flush();
    c.stop();
    return;
  }
  else {
    c.print("  command not found: ");
    c.println(cmd);
    c.println("  escribe 'help' para ver los comandos");
  }

  c.println();
  printPrompt(c);
}

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
        termClient.println();
        termClient.println("Camioneta - Terminal remota (mismos comandos que PC-Mode)");
        termClient.println("Escribe 'help' para ver los comandos.");
        printPrompt(termClient);
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
        handleCommand(termClient, cmd);
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
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(100);
    Serial.print(F("IP:   ")); Serial.println(WiFi.softAPIP());
    Serial.print(F("SSID: ")); Serial.println(AP_SSID);
  }

  termServer.begin();
  remoteRunning = true;
  Serial.println(F("Remoto: activo"));
}

static void stopRemote() {
  if (!remoteRunning) return;

  if (termClient) termClient.stop();
  termServer.end();

  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  remoteRunning = false;
  staConnected = false;
  Serial.println(F("Remoto: detenido"));
}

void screenRemoteLoop() {
  if (remoteRunning) serviceTerminal();

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
  u8g2.drawStr(20, 11, "Conexion Remota");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  uint8_t mode = settingsGetRemoteMode();

  if (remoteRunning) {
    u8g2.drawStr(10, 24, "Estado: ACTIVO");
    u8g2.drawStr(10, 36, mode == 1 ? "Modo: STA" : "Modo: AP");

    IPAddress ip = (mode == 1) ? WiFi.localIP() : WiFi.softAPIP();
    char ipBuf[32];
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d:%u", ip[0], ip[1], ip[2], ip[3], REMOTE_TERM_PORT);
    u8g2.drawStr(10, 48, ipBuf);

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(5, 60, "SEL:Detener BACK:Salir");
  } else {
    u8g2.drawStr(11, 19, "Estado: INACTIVO");

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(12, 30, "Terminal TCP por WiFi");
    u8g2.drawStr(12, 39, mode == 1 ? "Modo: STA (tu WiFi)" : "Modo: AP propio");
    u8g2.drawStr(12, 48, "Config: 'wifi' en term.");

    u8g2.drawStr(1, 59, "SEL:Iniciar BACK:Salir");
  }

  u8g2.sendBuffer();
}
