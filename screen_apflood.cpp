#include "Apps/screen_apflood.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include "esp_wifi.h"

// ============================================================
//  AP Flood (beacon flooding)
//
//  Inyecta tramas beacon 802.11 con SSID falsos, de modo que
//  aparezcan como redes WiFi en los dispositivos cercanos.
//  Cada SSID es una linea de la cancion (max 32 bytes por SSID).
//
//  Uso responsable: es una demo. Saturar el espectro 2.4GHz
//  con SSID falsos puede molestar a redes vecinas; usalo solo
//  en tu propio entorno de pruebas.
//
//  Controles:
//    SEL  -> iniciar / detener
//    BACK -> salir

static const char* ssidList[] = {
  "That girl is corrupt",
  "Could you raise her to love me",
  "maybe?",
  "She sure fucked me up",
  "And yes I'm talking 'bout your",
  "baby"
};
static const int SSID_COUNT = sizeof(ssidList) / sizeof(ssidList[0]);

static bool     flooding    = false;
static uint32_t beaconsSent = 0;
static uint8_t  channel     = 1;

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
  if (flooding) {
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

  u8g2.drawStr(34, 10, "AP Flood");
  u8g2.drawLine(0, 12, 127, 12);

  char line[24];

  if (flooding) {
    u8g2.drawStr(6, 26, "Estado: ACTIVO");
  } else {
    u8g2.drawStr(6, 26, "Estado: IDLE");
  }

  snprintf(line, sizeof(line), "SSIDs: %d  Ch: %d", SSID_COUNT, channel);
  u8g2.drawStr(6, 38, line);

  snprintf(line, sizeof(line), "Beacons: %lu", (unsigned long)beaconsSent);
  u8g2.drawStr(6, 50, line);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(6, 60, "SEL:On/Off  BACK:Salir");

  u8g2.sendBuffer();
}
