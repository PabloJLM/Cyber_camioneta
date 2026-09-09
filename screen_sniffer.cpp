#include "Apps/screen_sniffer.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include "esp_wifi.h"
#include "esp_timer.h"

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

// ============================================================
//  Sniffer WiFi -> PCAP
//
//  Pone el ESP32 en modo promiscuo (802.11) y guarda cada
//  paquete crudo en un archivo .pcap en la SD. El analisis
//  serio siempre se hace despues en la PC con Wireshark; el
//  ESP32 no relee ni interpreta el archivo.
//
//  En pantalla solo se muestran estadisticas basicas EN VIVO,
//  calculadas al vuelo mientras se captura (sin tocar el
//  archivo), pensadas para la parte educativa:
//    - Cuantos beacons / probes / datos se ven.
//    - Intensidad de señal del ultimo paquete (RSSI).
//    - El ultimo SSID que un celular busco por probe request
//      (la fuga de privacidad clasica: el telefono delata
//      redes a las que se conecto antes).
//    - Aviso de que el trafico de datos esta cifrado y no se
//      puede leer sin la contraseña + handshake.
//
//  Controles:
//    SEL     -> iniciar / detener captura
//    UP/DOWN -> canal (1-13)
//    BACK    -> salir
// ============================================================

// 512 bytes cubre beacons con varios tags (WPS, WMM, HT/VHT, vendor IEs),
// que son muy comunes en routers modernos y con 256 se cortaban a la
// mitad, causando "Malformed Packet" en Wireshark.
#define SNAP_LEN   512   // bytes maximos guardados por paquete
#define QUEUE_LEN  24    // paquetes en cola entre el callback y el loop

struct PktRec {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint16_t capLen;   // bytes realmente guardados (<= SNAP_LEN)
  uint16_t origLen;  // tamaño real de la trama en el aire (antes de recortar)
  int8_t   rssi;
  uint8_t  data[SNAP_LEN];
};

static QueueHandle_t pktQueue  = NULL;
static File          pcapFile;
static volatile bool capturing = false;
static uint8_t        channel  = 1;
static char           pcapName[24] = "";

// Nombre base configurable desde la terminal de Ajustes (comando
// "snifname <nombre>"). Por defecto "cap" -> cap1.pcap, cap2.pcap...
// Si en una misma sesion se hacen 2 capturas, la segunda sigue el
// numero siguiente automaticamente (nextPcapName ya revisa la SD).
#define SNIFFER_MAX_BASENAME 12
static char pcapBaseName[SNIFFER_MAX_BASENAME + 1] = "cap";

void snifferSetBaseName(const char* name) {
  if (!name || !name[0]) return;
  int j = 0;
  for (int i = 0; name[i] != '\0' && j < SNIFFER_MAX_BASENAME; i++) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (ok) pcapBaseName[j++] = c;
  }
  pcapBaseName[j] = '\0';
  if (j == 0) strcpy(pcapBaseName, "cap");   // no dejar el nombre vacio
}

const char* snifferGetBaseName() {
  return pcapBaseName;
}

// ---------- Estadisticas en vivo (solo en RAM) ----------
static uint32_t pktCount    = 0;
static uint32_t cntBeacon   = 0;
static uint32_t cntProbe    = 0;
static uint32_t cntData     = 0;
static uint32_t cntOtros    = 0;
static bool     dataSeen    = false;
static int8_t   lastRssi    = -100;
static char     lastProbeSsid[23] = "";
static bool     hasProbeSsid = false;

// ---------- Escritura PCAP ----------

static void writePcapGlobalHeader(File& f) {
  uint32_t magic    = 0xa1b2c3d4;
  uint16_t vmajor   = 2;
  uint16_t vminor   = 4;
  int32_t  thiszone = 0;
  uint32_t sigfigs  = 0;
  uint32_t snaplen  = SNAP_LEN;
  uint32_t network  = 105;         // LINKTYPE_IEEE802_11

  f.write((uint8_t*)&magic,    4);
  f.write((uint8_t*)&vmajor,   2);
  f.write((uint8_t*)&vminor,   2);
  f.write((uint8_t*)&thiszone, 4);
  f.write((uint8_t*)&sigfigs,  4);
  f.write((uint8_t*)&snaplen,  4);
  f.write((uint8_t*)&network,  4);
}

static void writePcapRecord(File& f, const PktRec& rec) {
  f.write((uint8_t*)&rec.ts_sec,  4);
  f.write((uint8_t*)&rec.ts_usec, 4);
  // incl_len = lo que de verdad guardamos; orig_len = tamaño real en el
  // aire. Si difieren, Wireshark lo marca como "recortado durante la
  // captura" (correcto) en vez de "malformado" (confuso).
  uint32_t inclLen = rec.capLen;
  uint32_t origLen = rec.origLen;
  f.write((uint8_t*)&inclLen, 4);
  f.write((uint8_t*)&origLen, 4);
  f.write(rec.data, rec.capLen);
}

// ---------- Estadisticas ----------

// Clasifica una trama 802.11 cruda y actualiza los contadores.
// Si es un probe request con SSID (no vacio), lo guarda como
// "ultima red buscada" para la leccion de privacidad.
static void classifyFrame(const uint8_t* d, uint16_t len) {
  if (len < 1) { cntOtros++; return; }

  uint8_t frameType    = (d[0] >> 2) & 0x03;
  uint8_t frameSubtype = (d[0] >> 4) & 0x0F;

  if (frameType == 0) {                 // gestion
    if (frameSubtype == 8) {                          // beacon
      cntBeacon++;
    } else if (frameSubtype == 4) {                   // probe request
      cntProbe++;
      // Sin parametros fijos: el tag SSID empieza justo tras
      // el header de 24 bytes (id en 24, longitud en 25).
      if (len >= 26) {
        uint8_t ssidLen = d[25];
        if (ssidLen > 0 && 26u + ssidLen <= len) {
          if (ssidLen > 22) ssidLen = 22;
          memcpy(lastProbeSsid, &d[26], ssidLen);
          lastProbeSsid[ssidLen] = '\0';
          hasProbeSsid = true;
        }
      }
    } else if (frameSubtype == 5) {                   // probe response
      cntProbe++;
    } else {
      cntOtros++;
    }
  } else if (frameType == 2) {          // datos
    cntData++;
    dataSeen = true;
  } else {                              // control u otro
    cntOtros++;
  }
}

static void processRecord(const PktRec& rec) {
  writePcapRecord(pcapFile, rec);
  pktCount++;
  lastRssi = rec.rssi;
  classifyFrame(rec.data, rec.capLen);
}

static void resetStats() {
  pktCount = cntBeacon = cntProbe = cntData = cntOtros = 0;
  dataSeen = false;
  hasProbeSsid = false;
  lastProbeSsid[0] = '\0';
  lastRssi = -100;
}

// ---------- Callback de modo promiscuo ----------

static void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!capturing || pktQueue == NULL) return;

  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint16_t origLen = pkt->rx_ctrl.sig_len;   // tamaño real en el aire
  uint16_t capLen  = origLen;
  if (capLen > SNAP_LEN) capLen = SNAP_LEN;  // recorte, si aplica

  PktRec rec;
  int64_t us = esp_timer_get_time();
  rec.ts_sec  = us / 1000000;
  rec.ts_usec = us % 1000000;
  rec.capLen  = capLen;
  rec.origLen = origLen;
  rec.rssi    = pkt->rx_ctrl.rssi;
  memcpy(rec.data, pkt->payload, capLen);

  // Si la cola esta llena, se descarta el paquete (sin bloquear).
  xQueueSend(pktQueue, &rec, 0);
}

// ---------- Control de captura ----------

static void nextPcapName() {
  // "nombre1.pcap", "nombre2.pcap", ... -- sin ceros a la izquierda,
  // como pidio Pablo. Si ya se hizo una captura en esta sesion (o en
  // una anterior, porque revisa la SD) el numero sigue subiendo solo.
  for (int i = 1; i < 10000; i++) {
    snprintf(pcapName, sizeof(pcapName), "/%s%d.pcap", pcapBaseName, i);
    if (!SD.exists(pcapName)) return;
  }
  snprintf(pcapName, sizeof(pcapName), "/%s0.pcap", pcapBaseName);
}

static void startCapture() {
  if (capturing) return;

  if (!SD.begin(PIN_CD)) {
    Serial.println(F("Sniffer: SD no disponible"));
    return;
  }

  nextPcapName();
  pcapFile = SD.open(pcapName, FILE_WRITE);
  if (!pcapFile) {
    Serial.println(F("Sniffer: no se pudo crear el archivo"));
    return;
  }
  writePcapGlobalHeader(pcapFile);
  pcapFile.flush();

  if (pktQueue == NULL) {
    pktQueue = xQueueCreate(QUEUE_LEN, sizeof(PktRec));
  }

  resetStats();

  // WiFi en modo estacion pero sin conectarse, solo escuchando.
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);

  capturing = true;
  Serial.print(F("Sniffer: capturando en "));
  Serial.print(pcapName);
  Serial.print(F(" canal "));
  Serial.println(channel);
}

static void stopCapture() {
  if (!capturing) return;

  capturing = false;
  esp_wifi_set_promiscuous(false);

  if (pktQueue) {
    PktRec rec;
    while (xQueueReceive(pktQueue, &rec, 0) == pdTRUE) {
      processRecord(rec);
    }
  }

  if (pcapFile) {
    pcapFile.flush();
    pcapFile.close();
  }
  WiFi.mode(WIFI_OFF);

  Serial.print(F("Sniffer: detenido. Paquetes: "));
  Serial.println(pktCount);
}

// ---------- Botones ----------

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

// ---------- Iconos ----------

// Barras de señal tipo WiFi: 4 barras crecientes, solo se
// dibujan las que corresponden a la intensidad (rssi en dBm).
static void drawSignalIcon(int x, int baselineY, int8_t rssi) {
  int level;
  if      (rssi > -50) level = 4;
  else if (rssi > -60) level = 3;
  else if (rssi > -70) level = 2;
  else if (rssi > -80) level = 1;
  else                 level = 0;

  const uint8_t heights[4] = {3, 5, 7, 9};
  for (int i = 0; i < 4; i++) {
    if (i >= level) continue;
    int barX = x + i * 3;
    int barH = heights[i];
    int barY = baselineY - barH;
    u8g2.drawBox(barX, barY, 2, barH);
  }
}

// Candado pequeño: cuerpo + arco superior (mitad de un circulo).
static void drawLockIcon(int x, int y) {
  u8g2.drawCircle(x + 3, y + 2, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  u8g2.drawBox(x, y + 2, 7, 6);
}

// ---------- Pantalla ----------

static void drawDashboard() {
  char line[26];

  // Fila 1: estado (punto lleno = grabando) + canal + señal.
  u8g2.setFont(u8g2_font_6x10_tr);
  if (capturing) {
    u8g2.drawDisc(10, 21, 3);
    u8g2.drawStr(16, 25, "REC");
  } else {
    u8g2.drawCircle(10, 21, 3);
    u8g2.drawStr(16, 25, "IDLE");
  }
  snprintf(line, sizeof(line), "CH%02d", channel);
  u8g2.drawStr(62, 25, line);
  drawSignalIcon(106, 25, lastRssi);

  // Fila 2: contadores compactos + candado si hay datos cifrados.
  snprintf(line, sizeof(line), "B:%lu  P:%lu  D:%lu",
           (unsigned long)cntBeacon, (unsigned long)cntProbe, (unsigned long)cntData);
  u8g2.drawStr(6, 40, line);
  if (dataSeen) drawLockIcon(112, 32);

  // Fila 3: ultimo SSID buscado por un celular (probe request).
  u8g2.setFont(u8g2_font_5x7_tr);
  if (hasProbeSsid) {
    snprintf(line, sizeof(line), "Busca: %s", lastProbeSsid);
  } else {
    snprintf(line, sizeof(line), "Busca: --");
  }
  u8g2.drawStr(6, 52, line);

  // Fila 4: ayuda de controles, segun el estado.
  if (capturing) {
    u8g2.drawStr(6, 62, "SEL:Detener  BACK:Salir");
  } else {
    u8g2.drawStr(6, 62, "SEL:Iniciar  UP/DN:Canal");
  }
}

void screenSnifferLoop() {
  // Escribe a la SD los paquetes que haya en la cola (limite por
  // vuelta para no bloquear el loop principal mucho tiempo).
  if (capturing && pktQueue) {
    PktRec rec;
    int written = 0;
    while (written < 32 && xQueueReceive(pktQueue, &rec, 0) == pdTRUE) {
      processRecord(rec);
      written++;
    }
    if (written > 0) pcapFile.flush();
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (capturing) stopCapture();
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (capturing) stopCapture();
    else           startCapture();
  }

  if (isButtonJustPressed(PIN_UP)) {
    buzzerClick();
    if (channel < 13) channel++;
    if (capturing) esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  }
  if (isButtonJustPressed(PIN_DOWN)) {
    buzzerClick();
    if (channel > 1) channel--;
    if (capturing) esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(43, 11, "Sniffer");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  drawDashboard();

  u8g2.sendBuffer();
}
