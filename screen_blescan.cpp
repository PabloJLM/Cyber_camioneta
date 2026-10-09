#include "Apps/screen_blescan.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

#define BLESCAN_MAX_DEVICES 24
#define BLESCAN_DURATION_S  2
static const unsigned long RESCAN_INTERVAL_MS = 3000;

struct BleDeviceInfo {
  char name[22];
  char mac[18];
  int rssi;
};

static BleDeviceInfo devices[BLESCAN_MAX_DEVICES];
static int deviceCount = 0;
static int selIndex = 0;
static bool scanning = false;
static bool bleReady = false;
static unsigned long lastScanAt = 0;

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

static const char* companyName(uint16_t id) { //https://gist.github.com/kongmunist/033a50237cdeb67af5666aa4435afd19
  switch (id) {
    case 0x004C: return "Apple";
    case 0x0006: return "Microsoft";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    case 0x038F: return "Xiaomi";
    case 0x0157: return "Amazfit/Huami";
    case 0x0171: return "Amazon";
    case 0x0059: return "Nordic Semi";
    case 0x000F: return "Broadcom";
    case 0x0002: return "Intel";
    case 0x0078: return "Nike";
    default: return nullptr;
  }
}

static String fallbackLabel(BLEAdvertisedDevice &d) {
  if (d.haveManufacturerData()) {
    String md = d.getManufacturerData();
    if (md.length() >= 2) {
      uint16_t companyId = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
      const char* nm = companyName(companyId);
      if (nm) return String(nm);
      char buf[16];
      snprintf(buf, sizeof(buf), "MFR:%04X", companyId);
      return String(buf);
    }
  }
  if (d.haveServiceUUID()) {
    String uuid = String(d.getServiceUUID().toString().c_str());
    if (uuid.length() > 8) uuid = uuid.substring(0, 8);
    return "UUID:" + uuid;
  }
  return String("(sin nombre)");
}

static void runScan() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  BLEScanResults* results = pBLEScan->start(BLESCAN_DURATION_S, false);

  deviceCount = 0;
  int n = results->getCount();
  for (int i = 0; i < n && deviceCount < BLESCAN_MAX_DEVICES; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    BleDeviceInfo &e = devices[deviceCount];

    String nm = d.haveName() ? String(d.getName().c_str()) : fallbackLabel(d);
    strncpy(e.name, nm.c_str(), sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';

    String mac = String(d.getAddress().toString().c_str());
    strncpy(e.mac, mac.c_str(), sizeof(e.mac) - 1);
    e.mac[sizeof(e.mac) - 1] = '\0';

    e.rssi = d.haveRSSI() ? d.getRSSI() : 0;
    deviceCount++;
  }
  pBLEScan->clearResults();

  if (selIndex >= deviceCount) selIndex = (deviceCount > 0) ? deviceCount - 1 : 0;
  lastScanAt = millis();
}

static void startScanning() {
  if (scanning) return;

  WiFi.mode(WIFI_OFF);

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  pBLEScan->setInterval(100);
  pBLEScan->setWindow(100);
  bleReady = true;

  scanning = true;
  deviceCount = 0;
  selIndex = 0;
  runScan();
}

static void stopScanning() {
  if (!scanning) return;
  if (bleReady) {
    BLEDevice::getScan()->stop();
    BLEDevice::deinit(true);
    bleReady = false;
  }
  scanning = false;
}

void screenBleScanLoop() {
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    if (scanning) stopScanning();
    currentScreen = SCREEN_APPS;
    return;
  }

  if (isButtonJustPressed(PIN_SELECT)) {
    buzzerBeep();
    if (scanning) stopScanning();
    else          startScanning();
  }

  if (scanning) {
    if (isButtonJustPressed(PIN_UP)) {
      buzzerClick();
      if (deviceCount > 0) selIndex = (selIndex - 1 + deviceCount) % deviceCount;
    }
    if (isButtonJustPressed(PIN_DOWN)) {
      buzzerClick();
      if (deviceCount > 0) selIndex = (selIndex + 1) % deviceCount;
    }
    if (millis() - lastScanAt >= RESCAN_INTERVAL_MS) {
      runScan();
    }
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(31, 11, "BLE Scanner");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  if (!scanning) {
    u8g2.drawCircle(10, 21, 3);
    u8g2.drawStr(16, 25, "OFF");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(6, 40, "SEL para escanear");
    u8g2.drawStr(6, 52, "BACK: Salir");
  } else {
    u8g2.drawDisc(10, 21, 3);
    u8g2.drawStr(16, 25, "ON");
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d disp.", deviceCount);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(80, 25, cnt);

    if (deviceCount == 0) {
      u8g2.drawStr(6, 38, "buscando...");
    } else {
      const int visible = 3;
      int start = selIndex - 1;
      if (start > deviceCount - visible) start = deviceCount - visible;
      if (start < 0) start = 0;

      for (int row = 0; row < visible && (start + row) < deviceCount; row++) {
        int idx = start + row;
        int yy = 34 + row * 9;
        if (idx == selIndex) u8g2.drawStr(0, yy, ">");
        char line[26];
        snprintf(line, sizeof(line), "%s %ddBm", devices[idx].name, devices[idx].rssi);
        u8g2.drawStr(8, yy, line);
      }
      u8g2.drawStr(6, 63, devices[selIndex].mac);
    }
  }

  u8g2.sendBuffer();
}
