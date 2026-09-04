#include "Apps/screen_gps.h"
#include "Drivers/buzzer.h"
#include <TinyGPSPlus.h>
#include <string.h>

// Modulo real (ver esquematico): ATGM336H-6N-74, GNSS GPS+BeiDou.
//
// Datos confirmados en banco de pruebas (ver historial en el chat si
// hace falta reabrir esto):
// - El modulo NO esta en TXD0/RXD0 (esos son otra cosa en esta placa).
//   Esta soldado a los netos "RX1"/"TX1" = IO4/IO5.
// - Corre a 115200 baudios (no el 9600 "de fabrica" que trae la hoja de
//   datos generica -- este modulo en particular viene configurado a
//   otra velocidad).
// - El propio modulo reporta "$GPTXT,...,ANTENNA OPEN" pero en esta
//   placa ese aviso NO es confiable: entre el conector de antena y el
//   pin RF_IN del modulo hay un LNA propio en la placa (U5, AT2659S)
//   con su capacitor de acoplamiento -- eso corta la continuidad de DC
//   que el chip usa para "sentir" si hay antena, asi que el aviso sale
//   "OPEN" este conectada o no la antena. No hace falta antena activa:
//   el LNA ya esta puesto en la placa (VCC_RF alimenta a U5, no a la
//   antena), asi que sirve una antena pasiva normal. Se muestra el
//   aviso igual en pantalla como dato informativo del modulo, pero no
//   como diagnostico definitivo -- la prueba real es salir a la calle.
static const int GPS_RX_PIN = 4;        // IO4 = "RX1"
static const int GPS_TX_PIN = 5;        // IO5 = "TX1"
static const unsigned long GPS_BAUD = 115200;

static HardwareSerial GPSSerial(1);  // UART1: hardware separado del UART0/consola
static TinyGPSPlus gps;
static bool gpsStarted = false;

// El estado de la antena NO hace falta salir a la calle para verlo: el
// propio modulo lo manda como texto plano en una sentencia $GPTXT
// ("ANTENNA OPEN" = no conectada/cortada, "ANTENNA OK"/"ANTENNA ON" =
// conectada bien, "ANTENNA SHORT" = cortocircuito). TinyGPSPlus ignora
// esas lineas (no son GGA/RMC/etc), asi que las buscamos a mano
// juntando los caracteres en un buffer chico hasta el salto de linea.
// OJO: en esta placa este aviso del modulo no es diagnostico definitivo
// (ver comentario arriba, hay un LNA propio -- U5 -- en el medio). Se
// muestra como dato informativo nomas, con el prefijo "Modulo dice:".
static char antennaStatus[24] = "Modulo: detectando...";
static char lineBuf[96];
static uint8_t lineLen = 0;

static void checkAntennaLine(const char* line) {
  if (strstr(line, "ANTENNA OPEN")) {
    strcpy(antennaStatus, "Modulo dice: OPEN");
  } else if (strstr(line, "ANTENNA SHORT")) {
    strcpy(antennaStatus, "Modulo dice: SHORT");
  } else if (strstr(line, "ANTENNA OK") || strstr(line, "ANTENNA ON")) {
    strcpy(antennaStatus, "Modulo dice: OK");
  }
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

static void gpsFeed() {
  while (GPSSerial.available()) {
    char c = GPSSerial.read();
    gps.encode(c);

    if (c == '\n' || lineLen >= sizeof(lineBuf) - 1) {
      lineBuf[lineLen] = '\0';
      checkAntennaLine(lineBuf);
      lineLen = 0;
    } else if (c != '\r') {
      lineBuf[lineLen++] = c;
    }
  }
}

void screenGPSLoop() {
  if (!gpsStarted) {
    // PIN_GPSON se deja como entrada (sin forzar) -- R4 ya lo mantiene
    // en HIGH por el pull-up, que es como estaba antes de este codigo.
    pinMode(PIN_GPSON, INPUT);

    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    gpsStarted = true;

    Serial.println();
    Serial.println(F("[GPS] UART1 @ 115200 (IO4/IO5). Tramas NMEA:"));
  }

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    currentScreen = SCREEN_APPS;
    return;
  }

  gpsFeed();

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(28, 10, "GPS Position");
  u8g2.drawLine(0, 12, 127, 12);

  u8g2.setFont(u8g2_font_5x7_tr);
  char line[24];

  if (gps.location.isValid()) {
    snprintf(line, sizeof(line), "Lat: %.6f", gps.location.lat());
    u8g2.drawStr(2, 24, line);

    snprintf(line, sizeof(line), "Lon: %.6f", gps.location.lng());
    u8g2.drawStr(2, 34, line);

    snprintf(line, sizeof(line), "Alt:%.0fm  Vel:%.0fkm/h",
              gps.altitude.isValid() ? gps.altitude.meters() : 0.0,
              gps.speed.isValid() ? gps.speed.kmph() : 0.0);
    u8g2.drawStr(2, 44, line);

    if (gps.time.isValid()) {
      snprintf(line, sizeof(line), "UTC %02d:%02d:%02d  Sat:%d",
                gps.time.hour(), gps.time.minute(), gps.time.second(),
                gps.satellites.isValid() ? gps.satellites.value() : 0);
    } else {
      snprintf(line, sizeof(line), "Sat:%d",
                gps.satellites.isValid() ? gps.satellites.value() : 0);
    }
    u8g2.drawStr(2, 54, line);
  } else {
    u8g2.drawStr(2, 24, "Buscando senal GPS...");

    u8g2.drawStr(2, 34, antennaStatus);

    snprintf(line, sizeof(line), "Satelites: %d",
              gps.satellites.isValid() ? gps.satellites.value() : 0);
    u8g2.drawStr(2, 44, line);

    if (gps.charsProcessed() < 10) {
      u8g2.drawStr(2, 54, "Sin datos del modulo");
    } else {
      snprintf(line, sizeof(line), "Bytes:%lu OK:%lu",
                gps.charsProcessed(), gps.passedChecksum());
      u8g2.drawStr(2, 54, line);
    }
  }

  u8g2.drawStr(2, 62, "BACK:Volver");

  u8g2.sendBuffer();
}
