#include "Drivers/ftp_server.h"
#include "config.h"
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>

static const uint16_t FTP_CTRL_PORT = 21;
static const uint16_t FTP_DATA_PORT = 50009;
static const char* FTP_USER = "camioneta";
static const char* FTP_PASS = "camioneta";

enum FtpState { FTP_WAIT_CLIENT, FTP_WAIT_USER, FTP_WAIT_PASS, FTP_READY };

static WiFiServer ctrlServer(FTP_CTRL_PORT);
static WiFiServer dataServer(FTP_DATA_PORT);
static WiFiClient ctrlClient;
static WiFiClient dataClient;
static FtpState state = FTP_WAIT_CLIENT;
static String cmdLine;
static String cwd = "/";
static File transferFile;
static int transferMode = 0;

static bool validPath(const String& p) {
  return p.indexOf("..") < 0;
}

static String resolvePath(String arg) {
  arg.trim();
  if (arg.length() == 0) return cwd;
  if (arg[0] == '/') return arg;
  if (cwd == "/") return "/" + arg;
  return cwd + "/" + arg;
}

static bool openDataConnection() {
  unsigned long t0 = millis();
  while (!dataServer.hasClient() && millis() - t0 < 5000) delay(10);
  if (!dataServer.hasClient()) return false;
  dataClient = dataServer.available();
  return true;
}

static void doCwd(String arg) {
  arg.trim();
  if (arg == "..") {
    int slash = cwd.lastIndexOf('/');
    cwd = (slash <= 0) ? "/" : cwd.substring(0, slash);
    ctrlClient.println("250 OK.");
    return;
  }
  String target = resolvePath(arg);
  if (!validPath(target)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD)) { ctrlClient.println("550 SD no disponible."); return; }
  if (target != "/" && !SD.exists(target)) { ctrlClient.println("550 No existe."); return; }
  File f = SD.open(target);
  bool isDir = f && f.isDirectory();
  if (f) f.close();
  if (target != "/" && !isDir) { ctrlClient.println("550 No es un directorio."); return; }
  cwd = target;
  ctrlClient.println("250 OK.");
}

static void doPasv() {
  if (dataClient) dataClient.stop();
  IPAddress ip = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP() : WiFi.localIP();
  ctrlClient.println("227 Entering Passive Mode (" +
    String(ip[0]) + "," + String(ip[1]) + "," + String(ip[2]) + "," + String(ip[3]) + "," +
    String(FTP_DATA_PORT >> 8) + "," + String(FTP_DATA_PORT & 255) + ").");
}

static void doList(String arg) {
  String target = resolvePath(arg);
  if (!validPath(target)) { ctrlClient.println("550 Ruta invalida."); return; }
  ctrlClient.println("150 Listando.");
  if (!openDataConnection()) { ctrlClient.println("425 No se pudo abrir conexion de datos."); return; }
  if (!SD.begin(PIN_CD)) {
    dataClient.stop();
    ctrlClient.println("550 SD no disponible.");
    return;
  }
  File dir = SD.open(target);
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry) {
      String name = entry.name();
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      char sizeBuf[16];
      snprintf(sizeBuf, sizeof(sizeBuf), "%10lu", (unsigned long)entry.size());
      if (entry.isDirectory()) {
        dataClient.println("drwxr-xr-x 1 camioneta camioneta " + String(sizeBuf) + " Jan 01 00:00 " + name);
      } else {
        dataClient.println("-rw-r--r-- 1 camioneta camioneta " + String(sizeBuf) + " Jan 01 00:00 " + name);
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  if (dir) dir.close();
  dataClient.stop();
  ctrlClient.println("226 Listo.");
}

static void doRetr(String arg) {
  String path = resolvePath(arg);
  if (!validPath(path)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD)) { ctrlClient.println("550 SD no disponible."); return; }
  if (!SD.exists(path)) { ctrlClient.println("550 No existe."); return; }

  transferFile = SD.open(path, FILE_READ);
  if (!transferFile || transferFile.isDirectory()) {
    if (transferFile) transferFile.close();
    ctrlClient.println("550 No es un archivo.");
    return;
  }

  ctrlClient.println("150 Enviando " + String(transferFile.size()) + " bytes.");
  if (!openDataConnection()) {
    transferFile.close();
    ctrlClient.println("425 No se pudo abrir conexion de datos.");
    return;
  }
  transferMode = 1;
}

static void serviceRetrieve() {
  if (!dataClient || !dataClient.connected()) {
    transferFile.close();
    transferMode = 0;
    ctrlClient.println("426 Conexion de datos perdida.");
    return;
  }

  uint8_t buf[512];
  int n = transferFile.available() ? transferFile.read(buf, sizeof(buf)) : 0;
  if (n > 0) dataClient.write(buf, n);

  if (!transferFile.available()) {
    transferFile.close();
    dataClient.stop();
    transferMode = 0;
    ctrlClient.println("226 Transferencia completa.");
  }
}

static void doStor(String arg) {
  String path = resolvePath(arg);
  if (!validPath(path)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD)) { ctrlClient.println("550 SD no disponible."); return; }

  if (SD.exists(path)) SD.remove(path);
  transferFile = SD.open(path, FILE_WRITE);
  if (!transferFile) { ctrlClient.println("550 No se pudo crear el archivo."); return; }

  ctrlClient.println("150 Listo para recibir.");
  if (!openDataConnection()) {
    transferFile.close();
    ctrlClient.println("425 No se pudo abrir conexion de datos.");
    return;
  }
  transferMode = 2;
}

static void serviceStore() {
  if (dataClient && dataClient.connected()) {
    while (dataClient.available()) {
      uint8_t buf[512];
      int n = dataClient.read(buf, sizeof(buf));
      if (n > 0) transferFile.write(buf, n);
    }
  }

  if (!dataClient.connected() && !dataClient.available()) {
    transferFile.close();
    dataClient.stop();
    transferMode = 0;
    ctrlClient.println("226 Transferencia completa.");
  }
}

static void doDele(String arg) {
  String path = resolvePath(arg);
  if (!validPath(path)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD)) { ctrlClient.println("550 SD no disponible."); return; }
  if (!SD.exists(path)) { ctrlClient.println("550 No existe."); return; }
  bool ok = SD.remove(path);
  ctrlClient.println(ok ? "250 Borrado." : "550 No se pudo borrar.");
}

static void doMkd(String arg) {
  String path = resolvePath(arg);
  if (!validPath(path)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD)) { ctrlClient.println("550 SD no disponible."); return; }
  bool ok = SD.mkdir(path);
  if (ok) ctrlClient.println("257 \"" + path + "\" creada.");
  else ctrlClient.println("550 No se pudo crear.");
}

static void doRmd(String arg) {
  String path = resolvePath(arg);
  if (!validPath(path)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD)) { ctrlClient.println("550 SD no disponible."); return; }
  bool ok = SD.rmdir(path);
  ctrlClient.println(ok ? "250 Borrado." : "550 No se pudo borrar.");
}

static void doSize(String arg) {
  String path = resolvePath(arg);
  if (!validPath(path)) { ctrlClient.println("550 Ruta invalida."); return; }
  if (!SD.begin(PIN_CD) || !SD.exists(path)) { ctrlClient.println("550 No existe."); return; }
  File f = SD.open(path, FILE_READ);
  if (!f) { ctrlClient.println("550 No se pudo abrir."); return; }
  ctrlClient.println("213 " + String(f.size()));
  f.close();
}

static void handleFtpCommand(const String& line) {
  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String arg = (sp < 0) ? "" : line.substring(sp + 1);
  cmd.toUpperCase();

  if (state == FTP_WAIT_USER) {
    if (cmd == "USER") {
      ctrlClient.println("331 Password requerido.");
      state = FTP_WAIT_PASS;
    } else {
      ctrlClient.println("530 Primero USER.");
    }
    return;
  }

  if (state == FTP_WAIT_PASS) {
    if (cmd == "PASS") {
      if (arg == FTP_PASS) {
        ctrlClient.println("230 Login OK.");
        state = FTP_READY;
      } else {
        ctrlClient.println("530 Login incorrecto.");
        state = FTP_WAIT_USER;
      }
    } else {
      ctrlClient.println("530 Primero PASS.");
    }
    return;
  }

  if (cmd == "USER" || cmd == "PASS") { ctrlClient.println("230 Ya identificado."); }
  else if (cmd == "SYST") { ctrlClient.println("215 UNIX Type: L8"); }
  else if (cmd == "FEAT") { ctrlClient.println("211 Sin features extra"); }
  else if (cmd == "TYPE") { ctrlClient.println("200 OK"); }
  else if (cmd == "NOOP") { ctrlClient.println("200 OK"); }
  else if (cmd == "PWD" || cmd == "XPWD") { ctrlClient.println("257 \"" + cwd + "\""); }
  else if (cmd == "CWD" || cmd == "XCWD") { doCwd(arg); }
  else if (cmd == "CDUP") { doCwd(".."); }
  else if (cmd == "PASV") { doPasv(); }
  else if (cmd == "LIST" || cmd == "NLST") { doList(arg); }
  else if (cmd == "RETR") { doRetr(arg); }
  else if (cmd == "STOR") { doStor(arg); }
  else if (cmd == "DELE") { doDele(arg); }
  else if (cmd == "MKD" || cmd == "XMKD") { doMkd(arg); }
  else if (cmd == "RMD" || cmd == "XRMD") { doRmd(arg); }
  else if (cmd == "SIZE") { doSize(arg); }
  else if (cmd == "QUIT") {
    ctrlClient.println("221 Bye.");
    ctrlClient.stop();
    state = FTP_WAIT_CLIENT;
  }
  else {
    ctrlClient.println("502 Comando no soportado.");
  }
}

void ftpServerBegin() {
  ctrlServer.begin();
  dataServer.begin();
  state = FTP_WAIT_CLIENT;
  cwd = "/";
  cmdLine = "";
  transferMode = 0;
}

void ftpServerEnd() {
  if (dataClient) dataClient.stop();
  if (ctrlClient) ctrlClient.stop();
  if (transferFile) transferFile.close();
  ctrlServer.end();
  dataServer.end();
  state = FTP_WAIT_CLIENT;
  transferMode = 0;
}

void ftpServerLoop() {
  if (state == FTP_WAIT_CLIENT) {
    if (ctrlServer.hasClient()) {
      if (ctrlClient && ctrlClient.connected()) {
        WiFiClient extra = ctrlServer.available();
        extra.println("421 Ya hay una sesion FTP activa.");
        extra.stop();
      } else {
        ctrlClient = ctrlServer.available();
        cwd = "/";
        cmdLine = "";
        ctrlClient.println("220 Camioneta FTP listo.");
        state = FTP_WAIT_USER;
      }
    }
    return;
  }

  if (!ctrlClient || !ctrlClient.connected()) {
    if (dataClient) dataClient.stop();
    if (transferFile) transferFile.close();
    transferMode = 0;
    state = FTP_WAIT_CLIENT;
    return;
  }

  if (transferMode == 1) serviceRetrieve();
  else if (transferMode == 2) serviceStore();

  while (ctrlClient.available() > 0) {
    char c = ctrlClient.read();
    if (c == '\n') {
      cmdLine.trim();
      if (cmdLine.length() > 0) handleFtpCommand(cmdLine);
      cmdLine = "";
    } else if (c != '\r') {
      cmdLine += c;
    }
  }
}
