#include "Apps/screen_blescan.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// BLE Scanner: reconocimiento pasivo de dispositivos BLE cercanos
// (nombre, MAC, RSSI). Solo escucha advertising -- no transmite nada
// propio, a diferencia de Bluetooth Spam. SELECT prende/apaga el
// escaneo (mismo patron que BT Spam: WiFi.mode(WIFI_OFF) antes de
// BLEDevice::init(), radio 2.4GHz compartido), UP/DOWN recorre la
// lista mientras esta prendido, BACK apaga y sale.

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

static void runScan() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  BLEScanResults* results = pBLEScan->start(BLESCAN_DURATION_S, false);

  deviceCount = 0;
  int n = results->getCount();
  for (int i = 0; i < n && deviceCount < BLESCAN_MAX_DEVICES; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    BleDeviceInfo &e = devices[deviceCount];

    String nm = d.haveName() ? d.getName() : String("(sin nombre)");
    strncpy(e.name, nm.c_str(), sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';

    String mac = d.getAddress().toString();
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

  // Mismo criterio que BT Spam: el radio 2.4GHz es compartido entre
  // WiFi y BLE, asi que se apaga el WiFi para no pelear con BLE.
  WiFi.mode(WIFI_OFF);

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(90);
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

  u8g2.drawStr(28, 10, "BLE Scanner");
  u8g2.drawLine(0, 12, 127, 12);

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
