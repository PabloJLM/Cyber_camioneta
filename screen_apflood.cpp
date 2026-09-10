#include "Apps/screen_apflood.h"
#include "Drivers/buzzer.h"
#include "Drivers/settings.h"
#include <WiFi.h>
#include "esp_wifi.h"

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

// ============================================================
//  AP Flood (beacon flooding)
//
//  Inyecta tramas beacon 802.11 con SSID falsos, de modo que
//  aparezcan como redes WiFi en los dispositivos cercanos.
//  Los mensajes se pueden cambiar en caliente desde PC-Mode con
//  el comando flood{mensaje1,mensaje2,...} (hasta 6, 32 bytes
//  cada uno, limite real de un SSID 802.11). Si no se configura
//  nada, usa la lista de fabrica de abajo.
//
//  Uso responsable: es una demo. Saturar el espectro 2.4GHz
//  con SSID falsos puede molestar a redes vecinas; usalo solo
//  en tu propio entorno de pruebas.
//
//  Controles:
//    SEL  -> iniciar / detener
//    BACK -> salir

// +1 para el terminador nulo.
static char ssidList[APFLOOD_MAX_MSGS][APFLOOD_MAX_SSIDLEN + 1] = {
  "That girl is corrupt",
  "Could you raise her to love me",
  "maybe?",
  "She sure fucked me up",
  "And yes I'm talking 'bout your",
  "baby"
};
static int SSID_COUNT = 6;   // cuantas entradas de ssidList estan en uso

// Reemplaza los mensajes activos. Se llama desde PC-Mode.
int apFloodSetMessages(const char* const* msgs, int count) {
  if (count > APFLOOD_MAX_MSGS) count = APFLOOD_MAX_MSGS;
  if (count < 1) return 0;

  for (int i = 0; i < count; i++) {
    strncpy(ssidList[i], msgs[i], APFLOOD_MAX_SSIDLEN);
    ssidList[i][APFLOOD_MAX_SSIDLEN] = '\0';
  }
  SSID_COUNT = count;
  return SSID_COUNT;
}

int apFloodGetMessageCount() {
  return SSID_COUNT;
}

const char* apFloodGetMessage(int i) {
  if (i < 0 || i >= SSID_COUNT) return "";
  return ssidList[i];
}

static bool     flooding    = false;
static uint32_t beaconsSent = 0;
static uint8_t  channel     = 1;

// Cada cuanto se manda una tanda completa de beacons, en ms. 0 = lo
// mas rapido posible (comportamiento de siempre). Se lee de NVS la
// primera vez que hace falta y se persiste cuando se cambia.
static uint16_t floodIntervalMs   = 0;
static bool     floodIntervalRead = false;
static unsigned long lastBeaconBatch = 0;

static void loadIntervalIfNeeded() {
  if (floodIntervalRead) return;
  floodIntervalMs = settingsGetApFloodInterval();
  floodIntervalRead = true;
}

void apFloodSetInterval(uint16_t ms) {
  floodIntervalMs = ms;
  floodIntervalRead = true;
  settingsSetApFloodInterval(ms);
}

uint16_t apFloodGetInterval() {
  loadIntervalIfNeeded();
  return floodIntervalMs;
}

// Plantilla de trama beacon. El SSID se inserta en el offset 38.
static uint8_t beaconTemplate[128] = {
  0x80, 0x00,                                     // Frame Control: beacon
  0x00, 0x00,                                     // Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,             // Destino: broadcast
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // Origen (BSSID) - aleatorio
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // BSSID - aleatorio
  0x00, 0x00,                                     // Seq
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  0xe8, 0x03,                                     // Beacon interval
  0x31, 0x04,                                     // Capabilities
  0x00, 0x00                                      // Tag SSID: id=0, len (se rellena)
};

// Nota: en IDF 5.x (core 3.x) esp_wifi_80211_tx() ya permite enviar
// beacons con cualquier MAC de origen, asi que NO hace falta sobreescribir
// ieee80211_raw_frame_sanity_check (hacerlo da "multiple definition").

// Envia un beacon para el SSID indicado.
static void sendBeacon(const char* ssid) {
  uint8_t packet[128];
  memcpy(packet, beaconTemplate, sizeof(beaconTemplate));

  // BSSID/origen aleatorio para que cada red parezca distinta.
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = random(256);
  mac[0] = (mac[0] & 0xFE) | 0x02; // MAC localmente administrada
  memcpy(&packet[10], mac, 6);
  memcpy(&packet[16], mac, 6);

  // Inserta el SSID.
  int ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;
  packet[37] = ssidLen;                 // longitud del tag SSID
  memcpy(&packet[38], ssid, ssidLen);

  int pos = 38 + ssidLen;

  // Tag: supported rates
  const uint8_t rates[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c };
  memcpy(&packet[pos], rates, sizeof(rates));
  pos += sizeof(rates);

  // Tag: DS parameter set (canal actual)
  packet[pos++] = 0x03;
  packet[pos++] = 0x01;
  packet[pos++] = channel;

  esp_wifi_80211_tx(WIFI_IF_AP, packet, pos, false);
  beaconsSent++;
}

static void startFlood() {
  if (flooding) return;

  WiFi.mode(WIFI_AP);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  flooding    = true;
  beaconsSent = 0;
  Serial.println(F("AP Flood: iniciado"));
}

static void stopFlood() {
  if (!flooding) return;

  flooding = false;
  WiFi.mode(WIFI_OFF);
  Serial.print(F("AP Flood: detenido. Beacons: "));
  Serial.println(beaconsSent);
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

void screenApFloodLoop() {
  // Mientras flooding: manda un beacon de cada SSID y salta de canal.
  // El intervalo entre tandas es ajustable (settings/terminal); 0 sigue
  // siendo "lo mas rapido posible", igual que antes de tener el ajuste.
  loadIntervalIfNeeded();
  if (flooding && (millis() - lastBeaconBatch >= floodIntervalMs)) {
    lastBeaconBatch = millis();
    for (int i = 0; i < SSID_COUNT; i++) {
      sendBeacon(ssidList[i]);
    }
    // Rota entre canales 1, 6 y 11 (los que no se solapan).
    static const uint8_t hop[] = {1, 6, 11};
    static uint8_t hopIdx = 0;
    hopIdx = (hopIdx + 1) % 3;
    channel = hop[hopIdx];
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (flooding) stopFlood();
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (flooding) stopFlood();
    else          startFlood();
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(40, 11, "AP Flood");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  char line[28];

  if (flooding) {
    u8g2.drawStr(6, 24, "Estado: ACTIVO");
  } else {
    u8g2.drawStr(6, 24, "Estado: IDLE");
  }

  snprintf(line, sizeof(line), "SSIDs: %d  Ch: %d", SSID_COUNT, channel);
  u8g2.drawStr(6, 34, line);

  if (floodIntervalMs > 0) {
    snprintf(line, sizeof(line), "Intervalo: %ums", floodIntervalMs);
  } else {
    snprintf(line, sizeof(line), "Intervalo: max");
  }
  u8g2.drawStr(6, 44, line);

  snprintf(line, sizeof(line), "Beacons: %lu", (unsigned long)beaconsSent);
  u8g2.drawStr(6, 54, line);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(6, 62, "SEL:On/Off  BACK:Salir");

  u8g2.sendBuffer();
}
