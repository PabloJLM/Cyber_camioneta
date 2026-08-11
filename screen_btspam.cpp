#include "Apps/screen_btspam.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>

//mensajes U
/*static const char* const AD_MESSAGES[] = {
  "cuando sale silksong dlc",
  "ingenieria UG inscribete!!",
  "magneto was right",
  "ven a universidad galileo!",
  "El Gran Viaje no espera",
  "escribe al tel. 2423 8000",
  "carreras->",
  "ing en mecatronica",
  "ing en electronica",
  "ing en telecomunicaciones",
  "ing en sistemas",
};*/

//mensajes random
static const char* const AD_MESSAGES[] = {
  "cuando sale silksong dlc",
  "magneto was right",
  "El Gran Viaje no espera",
};
static const int AD_COUNT = sizeof(AD_MESSAGES) / sizeof(AD_MESSAGES[0]);

static const unsigned long ROTATE_INTERVAL_MS = 3000; // 3s por mensaje

static BLEAdvertising* pAdvertising = nullptr;
static bool           spamming      = false;
static int             msgIndex     = 0;
static unsigned long   lastSwitchMs = 0;

// Arma el paquete de advertising con el texto indicado, recortado
// de forma segura al limite real de BLE clasico (26 bytes de nombre).
static void setAdMessage(const char* msg) {
  char safe[27];
  size_t n = strlen(msg);
  if (n > 26) n = 26;
  memcpy(safe, msg, n);
  safe[n] = '\0';

  BLEAdvertisementData advData;
  advData.setFlags(0x06); // LE General Discoverable, BR/EDR no soportado
  advData.setName(safe);
  pAdvertising->setAdvertisementData(advData);
}

static void startSpam() {
  if (spamming) return;

  // Por las dudas: el radio 2.4GHz es compartido entre WiFi y BLE en
  // este chip. Si quedo algo de WiFi encendido de otra pantalla, se
  // apaga aqui para no pelear con el controlador de BLE.
  WiFi.mode(WIFI_OFF);

  BLEDevice::init("Camioneta");
  pAdvertising = BLEDevice::getAdvertising();
  // Fuerza tipo de anuncio conectable/escaneable estandar y activa
  // la respuesta a scan activo (asi la mayoria de escaners/telefonos
  // lo detectan, no solo los que hacen scan pasivo).
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  msgIndex = 0;
  setAdMessage(AD_MESSAGES[msgIndex]);
  pAdvertising->start();

  spamming     = true;
  lastSwitchMs = millis();
  Serial.println(F("BT Ads: iniciado"));
}

static void stopSpam() {
  if (!spamming) return;

  if (pAdvertising) pAdvertising->stop();
  BLEDevice::deinit(true);

  spamming = false;
  Serial.println(F("BT Ads: detenido"));
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
  // Rota al siguiente mensaje cada ROTATE_INTERVAL_MS sin bloquear.
  if (spamming && millis() - lastSwitchMs >= ROTATE_INTERVAL_MS) {
    lastSwitchMs = millis();
    msgIndex = (msgIndex + 1) % AD_COUNT;
    pAdvertising->stop();
    setAdMessage(AD_MESSAGES[msgIndex]);
    pAdvertising->start();
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

  u8g2.drawStr(38, 10, "BT Ads");
  u8g2.drawLine(0, 12, 127, 12);

  char line[24];

  if (spamming) {
    u8g2.drawDisc(10, 21, 3);
    u8g2.drawStr(16, 25, "ON");
  } else {
    u8g2.drawCircle(10, 21, 3);
    u8g2.drawStr(16, 25, "OFF");
  }
  snprintf(line, sizeof(line), "%d/%d", msgIndex + 1, AD_COUNT);
  u8g2.drawStr(96, 25, line);

  u8g2.setFont(u8g2_font_5x7_tr);
  if (spamming) {
    u8g2.drawStr(6, 40, AD_MESSAGES[msgIndex]);
  } else {
    u8g2.drawStr(6, 40, "Listo para anunciar");
  }

  u8g2.drawStr(6, 52, spamming ? "SEL:Detener" : "SEL:Iniciar");
  u8g2.drawStr(6, 62, "BACK:Salir");

  u8g2.sendBuffer();
}
