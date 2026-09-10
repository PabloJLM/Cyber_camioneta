#include "Ajustes/screen_sdbrowser.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
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


static const char* AP_SSID = "Camioneta1";
static const char* AP_PASS = "12345678";

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

static WebServer webServer(80);
static bool browserRunning = false;
static File uploadFile;


static bool sdReady() {
  return SD.begin(PIN_CD);
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


static String jstr(const String& v) {
  String o = "\"";
  for (size_t i = 0; i < v.length(); i++) {
    char c = v[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  o += "\"";
  return o;
}

static bool sanitizePath(String& path) {
  if (path.length() == 0) return false;
  if (path.indexOf("..") >= 0) return false;
  if (path[0] != '/') path = "/" + path;
  return true;
}

static String joinPath(const String& dir, const String& name) {
  if (dir == "/") return "/" + name;
  return dir + "/" + name;
}


static const char PAGE_BROWSER_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SD Browser</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<div class="wrap">
  <h1>SD Browser</h1>
  <div class="crumbs" id="crumbs"></div>
  <table>
    <thead><tr><th>Nombre</th><th>Tamano</th><th></th></tr></thead>
    <tbody id="rows"></tbody>
  </table>
  <div class="upload">
    <input type="file" id="file">
    <button onclick="doUpload()">Subir aca</button>
  </div>
  <div class="custom">
    <h2>Personalizar esta pagina (LittleFS)</h2>
    <p class="hint">Esto reemplaza el HTML/CSS de este browser -- va directo a
    LittleFS, sin tocar la SD. Se aplica al recargar la pagina.</p>
    <div class="customRow">
      <span>index.html</span>
      <input type="file" id="fileHtml" accept=".html,text/html">
      <button onclick="doCustomUpload('fileHtml','/custom_html')">Subir</button>
    </div>
    <div class="customRow">
      <span>style.css</span>
      <input type="file" id="fileCss" accept=".css,text/css">
      <button onclick="doCustomUpload('fileCss','/custom_css')">Subir</button>
    </div>
    <button class="reset" onclick="doReset()">Restaurar version por defecto</button>
  </div>
  <div id="status"></div>
</div>
<script>
var cur = "/";

function fmtSize(n){
  if(n<1024) return n+" B";
  if(n<1024*1024) return (n/1024).toFixed(1)+" KB";
  return (n/1024/1024).toFixed(1)+" MB";
}
function setStatus(msg,isErr){
  var s=document.getElementById('status');
  s.textContent=msg||''; s.style.color=isErr?'#ff6b6b':'#8b95a5';
}
function renderCrumbs(){
  var parts=cur.split('/').filter(Boolean);
  var html='<a onclick="go(\'/\')">SD:/</a>';
  var acc='';
  parts.forEach(function(p){ acc+='/'+p; html+=' / <a onclick="go(\''+acc+'\')">'+p+'</a>'; });
  document.getElementById('crumbs').innerHTML=html;
}
function go(dir){ cur=dir; load(); }
async function load(){
  renderCrumbs();
  setStatus('cargando...');
  try{
    var r=await fetch('/api/list?dir='+encodeURIComponent(cur));
    if(!r.ok){ setStatus('error: HTTP '+r.status,true); return; }
    var items=await r.json();
    items.sort(function(a,b){ if(a.dir!==b.dir) return b.dir-a.dir; return a.name.localeCompare(b.name); });
    var tb=document.getElementById('rows');
    if(!items.length){ tb.innerHTML='<tr><td colspan="3" class="empty">carpeta vacia</td></tr>'; setStatus(''); return; }
    tb.innerHTML=items.map(function(it){
      var path=(cur==='/'?'':cur)+'/'+it.name;
      if(it.dir){
        return '<tr><td colspan="2"><span class="nm dir" onclick="go(\''+path+'\')">'+it.name+'</span></td>'+
               '<td class="acts"><button class="del" onclick="del(\''+path+'\')">Borrar</button></td></tr>';
      }
      return '<tr><td><span class="nm file">'+it.name+'</span></td><td class="sz">'+fmtSize(it.size)+'</td>'+
             '<td class="acts"><a href="/dl?path='+encodeURIComponent(path)+'">Descargar</a>'+
             '<button class="del" onclick="del(\''+path+'\')">Borrar</button></td></tr>';
    }).join('');
    setStatus('');
  }catch(e){ setStatus('error: '+e,true); }
}
async function del(path){
  if(!confirm('Borrar '+path+'?')) return;
  setStatus('borrando...');
  try{
    var r=await fetch('/del',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'path='+encodeURIComponent(path)});
    var t=await r.text();
    if(r.ok){ setStatus('borrado'); load(); } else { setStatus('error: '+t,true); }
  }catch(e){ setStatus('error: '+e,true); }
}
async function doUpload(){
  var inp=document.getElementById('file');
  if(!inp.files.length){ setStatus('elegi un archivo primero',true); return; }
  var fd=new FormData(); fd.append('file',inp.files[0]);
  setStatus('subiendo...');
  try{
    var r=await fetch('/upload?dir='+encodeURIComponent(cur),{method:'POST',body:fd});
    if(r.ok){ setStatus('subido'); inp.value=''; load(); } else { setStatus('error: HTTP '+r.status,true); }
  }catch(e){ setStatus('error: '+e,true); }
}
async function doCustomUpload(inputId,url){
  var inp=document.getElementById(inputId);
  if(!inp.files.length){ setStatus('elegi un archivo primero',true); return; }
  var fd=new FormData(); fd.append('file',inp.files[0]);
  setStatus('subiendo a LittleFS...');
  try{
    var r=await fetch(url,{method:'POST',body:fd});
    if(r.ok){ setStatus('listo, recarga la pagina para verlo'); inp.value=''; } else { setStatus('error: HTTP '+r.status,true); }
  }catch(e){ setStatus('error: '+e,true); }
}
async function doReset(){
  if(!confirm('Volver a la version por defecto (borra el html/css personalizado de SD y LittleFS)?')) return;
  setStatus('restaurando...');
  try{
    var r=await fetch('/custom_reset',{method:'POST'});
    var t=await r.text();
    if(r.ok){ setStatus(t+' -- recarga la pagina'); } else { setStatus('error: '+t,true); }
  }catch(e){ setStatus('error: '+e,true); }
}
load();
</script>
</body>
</html>)HTML";

static const char PAGE_BROWSER_CSS[] PROGMEM = R"CSS(
:root{--bg:#12161c;--panel:#1a2029;--edge:#2a3341;--fg:#e6e9ef;--dim:#8b95a5;--acc:#4da3ff;--err:#ff6b6b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,Arial,sans-serif;font-size:14px;padding:16px}
.wrap{max-width:680px;margin:0 auto}
h1{font-size:18px;margin:0 0 4px}
.crumbs{color:var(--dim);font-size:13px;margin-bottom:12px;word-break:break-all}
.crumbs a{color:var(--acc);text-decoration:none;cursor:pointer}
table{width:100%;border-collapse:collapse;background:var(--panel);border:1px solid var(--edge);border-radius:8px;overflow:hidden}
th,td{text-align:left;padding:8px 10px;border-bottom:1px solid var(--edge);font-size:13px}
th{color:var(--dim);font-weight:600;font-size:12px}
tr:last-child td{border-bottom:0}
.nm{cursor:pointer;color:var(--fg)}
.nm.dir{color:var(--acc)}
.nm.dir::before{content:"\1F4C1 ";}
.nm.file::before{content:"\1F4C4 ";}
.sz{color:var(--dim);white-space:nowrap}
.acts{white-space:nowrap;text-align:right}
.acts a,.acts button{font-size:12px;padding:4px 8px;border-radius:5px;border:1px solid var(--edge);
  background:transparent;color:var(--fg);text-decoration:none;cursor:pointer;margin-left:4px}
.acts button.del{color:var(--err);border-color:var(--err)}
.empty{padding:16px;color:var(--dim);text-align:center}
.upload{margin-top:14px;background:var(--panel);border:1px solid var(--edge);border-radius:8px;padding:12px;
  display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.upload input[type=file]{flex:1;color:var(--dim);font-size:12px}
.upload button{background:var(--acc);color:#04121f;border:0;border-radius:6px;padding:8px 14px;font-weight:600;cursor:pointer}
.custom{margin-top:14px;background:var(--panel);border:1px solid var(--edge);border-radius:8px;padding:12px}
.custom h2{font-size:14px;margin:0 0 4px}
.custom .hint{color:var(--dim);font-size:12px;margin:0 0 10px}
.customRow{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:8px}
.customRow span{width:72px;font-size:12px;color:var(--dim)}
.customRow input[type=file]{flex:1;color:var(--dim);font-size:12px}
.customRow button{background:var(--acc);color:#04121f;border:0;border-radius:6px;padding:6px 12px;font-weight:600;cursor:pointer;font-size:12px}
.custom .reset{margin-top:4px;background:transparent;color:var(--err);border:1px solid var(--err);border-radius:6px;padding:6px 12px;cursor:pointer;font-size:12px}
#status{margin-top:10px;font-size:12px;color:var(--dim);min-height:16px}
)CSS";

// ---------- handlers HTTP ----------

static const char* SDB_INDEX = "/sdbrowser/index.html";
static const char* SDB_CSS   = "/sdbrowser/style.css";

static bool serveFromSD(const char* path, const char* contentType) {
  if (!sdReady() || !SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  webServer.streamFile(f, contentType);
  f.close();
  return true;
}

static bool serveFromFS(const char* path, const char* contentType) {
  if (!fsReady() || !LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  webServer.streamFile(f, contentType);
  f.close();
  return true;
}


static bool serveCustom(const char* path, const char* contentType) {
  return serveFromSD(path, contentType) || serveFromFS(path, contentType);
}

static void handleRoot() {
  if (!serveCustom(SDB_INDEX, "text/html")) {
    webServer.send_P(200, "text/html", PAGE_BROWSER_HTML);
  }
}

static void handleStyle() {
  if (!serveCustom(SDB_CSS, "text/css")) {
    webServer.send_P(200, "text/css", PAGE_BROWSER_CSS);
  }
}

static void handleApiList() {
  String dir = webServer.hasArg("dir") ? webServer.arg("dir") : "/";
  if (!sanitizePath(dir)) { webServer.send(400, "text/plain", "ruta invalida"); return; }
  if (!sdReady()) { webServer.send(503, "text/plain", "SD no disponible"); return; }

  File root = SD.open(dir);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    webServer.send(404, "text/plain", "no existe");
    return;
  }

  String json = "[";
  bool first = true;
  File entry = root.openNextFile();
  while (entry) {
    String name = entry.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.length()) {
      if (!first) json += ",";
      first = false;
      json += "{\"name\":" + jstr(name) + ",\"size\":" + String(entry.size()) +
              ",\"dir\":" + (entry.isDirectory() ? "true" : "false") + "}";
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
  json += "]";
  webServer.send(200, "application/json", json);
}

static void handleDownload() {
  if (!webServer.hasArg("path")) { webServer.send(400, "text/plain", "falta path"); return; }
  String path = webServer.arg("path");
  if (!sanitizePath(path)) { webServer.send(400, "text/plain", "ruta invalida"); return; }
  if (!sdReady() || !SD.exists(path)) { webServer.send(404, "text/plain", "no existe"); return; }

  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    webServer.send(400, "text/plain", "no es un archivo");
    return;
  }

  String name = path;
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);

  webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  webServer.streamFile(f, "application/octet-stream");
  f.close();
}

static void handleDelete() {
  if (!webServer.hasArg("path")) { webServer.send(400, "text/plain", "falta path"); return; }
  String path = webServer.arg("path");
  if (!sanitizePath(path) || path == "/") { webServer.send(400, "text/plain", "ruta invalida"); return; }
  if (!sdReady()) { webServer.send(503, "text/plain", "SD no disponible"); return; }

  File f = SD.open(path);
  bool isDir = f && f.isDirectory();
  if (f) f.close();

  bool ok = isDir ? SD.rmdir(path) : SD.remove(path);
  webServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "no se pudo borrar (carpeta no vacia?)");
}

static void handleUploadData() {
  HTTPUpload& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String dir = webServer.hasArg("dir") ? webServer.arg("dir") : "/";
    if (!sanitizePath(dir)) dir = "/";
    String path = joinPath(dir, upload.filename);
    if (sdReady()) {
      if (SD.exists(path)) SD.remove(path);
      uploadFile = SD.open(path, FILE_WRITE);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
  }
}

static void handleUploadDone() {
  webServer.send(200, "text/plain", "OK");
}

static File customUploadFile;

static void handleCustomUpload(const char* fsPath) {
  HTTPUpload& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (fsReady()) {
      LittleFS.mkdir("/sdbrowser");
      if (LittleFS.exists(fsPath)) LittleFS.remove(fsPath);
      customUploadFile = LittleFS.open(fsPath, "w");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (customUploadFile) customUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (customUploadFile) customUploadFile.close();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (customUploadFile) customUploadFile.close();
  }
}

static void handleCustomUploadHTML() { handleCustomUpload(SDB_INDEX); }
static void handleCustomUploadCSS()  { handleCustomUpload(SDB_CSS); }


static void handleCustomReset() {
  bool didSomething = false;

  if (sdReady()) {
    if (SD.exists(SDB_INDEX)) { SD.remove(SDB_INDEX); didSomething = true; }
    if (SD.exists(SDB_CSS))   { SD.remove(SDB_CSS);   didSomething = true; }
  }
  if (fsReady()) {
    if (LittleFS.exists(SDB_INDEX)) { LittleFS.remove(SDB_INDEX); didSomething = true; }
    if (LittleFS.exists(SDB_CSS))   { LittleFS.remove(SDB_CSS);   didSomething = true; }
  }

  webServer.send(200, "text/plain", didSomething ? "OK, restaurado" : "nada que borrar");
}



static void startBrowser() {
  if (browserRunning) return;

  Serial.println(F("\nSD Browser: iniciando"));

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);

  Serial.print(F("IP:   ")); Serial.println(WiFi.softAPIP());
  Serial.print(F("SSID: ")); Serial.println(AP_SSID);

  webServer.on("/", handleRoot);
  webServer.on("/style.css", handleStyle);
  webServer.on("/api/list", handleApiList);
  webServer.on("/dl", handleDownload);
  webServer.on("/del", HTTP_POST, handleDelete);
  webServer.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
  webServer.on("/custom_html", HTTP_POST, handleUploadDone, handleCustomUploadHTML);
  webServer.on("/custom_css", HTTP_POST, handleUploadDone, handleCustomUploadCSS);
  webServer.on("/custom_reset", HTTP_POST, handleCustomReset);
  webServer.begin();

  browserRunning = true;
  Serial.println(F("SD Browser: activo\n"));
}

static void stopBrowser() {
  if (!browserRunning) return;

  webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  browserRunning = false;
  Serial.println(F("SD Browser: detenido"));
}



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

void screenSDBrowserLoop() {
  if (browserRunning) {
    webServer.handleClient();
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (browserRunning) stopBrowser();
    currentScreen = SCREEN_AJUSTES;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (browserRunning) stopBrowser();
    else                startBrowser();
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(34, 11, "SD Browser");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  if (browserRunning) {
    u8g2.drawStr(10, 24, "Estado: ACTIVO");
    u8g2.drawStr(10, 36, "SSID:");
    u8g2.drawStr(40, 36, AP_SSID);

    IPAddress ip = WiFi.softAPIP();
    char ipBuf[24];
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    u8g2.drawStr(10, 48, ipBuf);

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(5, 60, "SEL:Detener BACK:Salir");
  } else {
    u8g2.drawStr(11, 19, "Estado: INACTIVO");

    // Descripcion en 3 lineas cortas con mas aire entre ellas, en vez
    // de 2 lineas largas apretadas (la segunda no cabia bien).
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(12, 30, "Ver, subir y borrar");
    u8g2.drawStr(12, 39, "archivos de la SD");
    u8g2.drawStr(12, 48, "por WiFi");

    u8g2.drawStr(1, 59, "SEL:Iniciar BACK:Salir");
  }

  u8g2.sendBuffer();
}
