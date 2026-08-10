#include "Apps/screen_sniffer.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include "esp_wifi.h"
#include "esp_timer.h"

// ============================================================
//  Sniffer WiFi -> PCAP
//
//  Pone el ESP32 en modo promiscuo (802.11) y guarda cada
//  paquete en un archivo .pcap en la SD. Ese archivo se abre
//  directamente en Wireshark (Link-layer: IEEE 802.11).
//
//  Controles:
//    SEL   -> iniciar / detener captura
//    UP    -> canal +1
//    DOWN  -> canal -1
//    BACK  -> salir (detiene y cierra el archivo)
// ============================================================

// Bytes maximos que guardamos por paquete (recorta los muy grandes).
#define SNAP_LEN   256
// Cuantos paquetes caben en la cola callback -> loop.
#define QUEUE_LEN  24

// Un paquete capturado listo para escribir.
struct PktRec {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint16_t len;
  uint8_t  data[SNAP_LEN];
};

static QueueHandle_t pktQueue = NULL;
static File          pcapFile;
static bool          capturing = false;
static uint8_t       channel   = 1;
static uint32_t      pktCount  = 0;
static char          pcapName[24] = "";

// ---------- Escritura PCAP ----------

// Cabecera global del formato .pcap (24 bytes).
static void writePcapGlobalHeader(File& f) {
  uint32_t magic   = 0xa1b2c3d4;
  uint16_t vmajor  = 2;
  uint16_t vminor  = 4;
  int32_t  thiszone = 0;
  uint32_t sigfigs = 0;
  uint32_t snaplen = SNAP_LEN;
  uint32_t network = 105;         // LINKTYPE_IEEE802_11

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
  uint32_t inclLen = rec.len;
  uint32_t origLen = rec.len;
  f.write((uint8_t*)&inclLen, 4);
  f.write((uint8_t*)&origLen, 4);
  f.write(rec.data, rec.len);
}

// ---------- Callback de modo promiscuo ----------

static void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!capturing || pktQueue == NULL) return;

  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint16_t len = pkt->rx_ctrl.sig_len;
  if (len > SNAP_LEN) len = SNAP_LEN;

  PktRec rec;
  int64_t us = esp_timer_get_time();
  rec.ts_sec  = us / 1000000;
  rec.ts_usec = us % 1000000;
  rec.len     = len;
  memcpy(rec.data, pkt->payload, len);

  // Si la cola esta llena, se descarta el paquete (sin bloquear).
  xQueueSend(pktQueue, &rec, 0);
}

// ---------- Control de captura ----------

// Busca el primer nombre libre /capNNNN.pcap
static void nextPcapName() {
  for (int i = 1; i < 10000; i++) {
    snprintf(pcapName, sizeof(pcapName), "/cap%04d.pcap", i);
    if (!SD.exists(pcapName)) return;
  }
  strcpy(pcapName, "/cap0000.pcap");
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

  pktCount = 0;

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

  // Vacia lo que quede en la cola.
  if (pktQueue) {
    PktRec rec;
    while (xQueueReceive(pktQueue, &rec, 0) == pdTRUE) {
      writePcapRecord(pcapFile, rec);
      pktCount++;
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

// ---------- Pantalla ----------

void screenSnifferLoop() {
  // Escribe a la SD los paquetes que haya en la cola.
  if (capturing && pktQueue) {
    PktRec rec;
    int written = 0;
    while (written < 32 && xQueueReceive(pktQueue, &rec, 0) == pdTRUE) {
      writePcapRecord(pcapFile, rec);
      pktCount++;
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

  // Cambiar canal (afecta en vivo si esta capturando).
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

  u8g2.drawStr(35, 10, "Sniffer");
  u8g2.drawLine(0, 12, 127, 12);

  char line[24];

  if (capturing) {
    u8g2.drawStr(6, 26, "Estado: CAPTURANDO");
  } else {
    u8g2.drawStr(6, 26, "Estado: IDLE");
  }

  snprintf(line, sizeof(line), "Canal: %d", channel);
  u8g2.drawStr(6, 38, line);

  snprintf(line, sizeof(line), "Paquetes: %lu", (unsigned long)pktCount);
  u8g2.drawStr(6, 50, line);

  u8g2.setFont(u8g2_font_5x7_tr);
  if (capturing && pcapName[0]) {
    u8g2.drawStr(6, 60, pcapName);
  } else {
    u8g2.drawStr(6, 60, "SEL:Iniciar UP/DN:Canal");
  }

  u8g2.sendBuffer();
}
