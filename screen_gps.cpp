#include "Apps/screen_gps.h"
#include "Drivers/buzzer.h"
#include <TinyGPSPlus.h>

// Modulo real (ver esquematico): ATGM336H-6N-74, GNSS GPS+BeiDou.
//
// Historial del lio de pines (para que quede documentado):
// 1) Se asumio que el GPS estaba en TXD0/RXD0 (UART0 "de fabrica" del
//    chip) por como se leia el esquematico general. Se probo con
//    HardwareSerial(0) directo -> rompia el Monitor Serie (esa consola
//    SI es UART0 en esta placa).
// 2) Se paso a UART1 (periferico de hardware separado) pero routeado
//    via GPIO matrix a los pines de las macros RX/TX del core (GPIO17 /
//    GPIO16) -- pensando que esos eran los pines fisicos del GPS.
//    Resultado: cero bytes, nunca llego nada.
// 3) Con el pinout real de la placa se confirmo que el GPS en realidad
//    esta soldado a otro par completamente distinto: neto "RX1" -> IO4
//    y neto "TX1" -> IO5. Nada que ver con TXD0/RXD0. Con eso arreglado
//    deberia empezar a llegar dato.
//
// Asi que: UART1 de hardware (separado de la consola), pero apuntado a
// los pines reales IO4 (RX del ESP32, recibe el TX del modulo) e IO5
// (TX del ESP32, hacia el RX del modulo).
static const int GPS_RX_PIN = 4;   // IO4 = "RX1" en el esquematico
static const int GPS_TX_PIN = 5;   // IO5 = "TX1" en el esquematico
//
// Con los pines ya correctos empezo a llegar dato, pero repetia siempre
// el mismo patron de basura -- la firma clasica de un baudrate mal
// puesto (la trama real se repite ~1 vez por segundo y al decodificarla
// mal siempre sale la misma pinta). En vez de recompilar a mano
// probando velocidades, las probamos en rotacion automatica hasta que
// una trama pase el checksum de NMEA.
static const unsigned long BAUD_CANDIDATES[] = {9600, 4800, 19200, 38400, 57600, 115200};
static const int NUM_BAUDS = sizeof(BAUD_CANDIDATES) / sizeof(BAUD_CANDIDATES[0]);
static const unsigned long BAUD_TRY_MS = 2500;  // cuanto se le da a cada velocidad antes de probar la siguiente

static int baudIndex = 0;
static unsigned long lastBaudSwitch = 0;
static bool baudLocked = false;  // true en cuanto una trama pasa el checksum -- deja de rotar

static HardwareSerial GPSSerial(1);  // UART1: hardware separado del UART0/consola
static TinyGPSPlus gps;
static bool gpsStarted = false;

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

// Ademas de alimentar al parser, hace un eco crudo de cada byte que
// llega del modulo hacia el Monitor Serie (Serial normal, USB) -- asi
// se puede ver en vivo, desde la PC, si esta llegando algo y que pinta
// tiene: si no aparece nada = no llega dato (cableado/GPS_ON); si
// aparece basura ilegible = el baudrate no es el que pusimos; si
// aparecen lineas tipo "$GPGGA,..." o "$GNRMC,..." = el UART esta bien
// y solo falta el fix (antena/cielo abierto).
static void gpsFeed() {
  while (GPSSerial.available()) {
    char c = GPSSerial.read();
    Serial.write(c);
    gps.encode(c);
  }
}

// Traza de diagnostico: se imprime UNA sola vez por completo (la primera
// vuelta que logra terminar entera) para ver, si vuelve a trabarse, en
// que paso exacto se quedo -- el ultimo "paso N" que se alcanzo a
// imprimir es la linea siguiente a la que esta colgando.
static bool tracedOnce = false;
#define GPS_TRACE(msg) do { if (!tracedOnce) { Serial.print(F("[GPS-trace] ")); Serial.println(F(msg)); } } while (0)

void screenGPSLoop() {
  if (!gpsStarted) {
    GPS_TRACE("paso 1: entrando");

    // OJO: por ahora NO forzamos PIN_GPSON. No sabemos si es un enable
    // activo en alto, activo en bajo, o un pin de solo lectura de
    // estado del modulo -- forzarlo mal puede dejar el modulo apagado
    // sin que se note. Lo dejamos como entrada (tal como estaria si
    // este codigo no existiera; R4 lo mantiene en HIGH por el pull-up)
    // y solo lo leemos, para no arriesgar apagarlo por error.
    pinMode(PIN_GPSON, INPUT);
    Serial.print(F("[GPS] PIN_GPSON leido (sin tocar) = "));
    Serial.println(digitalRead(PIN_GPSON) ? "HIGH" : "LOW");

    Serial.print(F("[GPS] Pines UART1 -> RX="));
    Serial.print(GPS_RX_PIN);
    Serial.print(F(" TX="));
    Serial.println(GPS_TX_PIN);

    GPS_TRACE("paso 2: llamando GPSSerial.begin()");
    GPSSerial.begin(BAUD_CANDIDATES[baudIndex], SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // IO4/IO5 = "RX1"/"TX1" reales
    GPS_TRACE("paso 3: begin() volvio sin colgarse");
    gpsStarted = true;
    lastBaudSwitch = millis();
    Serial.println();
    Serial.print(F("[GPS] Probando baudrate: "));
    Serial.println(BAUD_CANDIDATES[baudIndex]);
    Serial.println(F("[GPS] Tramas NMEA crudas abajo (si no aparece nada, revisa cableado):"));
  }

  // Si todavia no paso ninguna trama con checksum valido, cada
  // BAUD_TRY_MS probamos la siguiente velocidad de la lista.
  if (!baudLocked) {
    if (gps.passedChecksum() > 0) {
      baudLocked = true;
      Serial.print(F("[GPS] Baudrate correcto encontrado: "));
      Serial.println(BAUD_CANDIDATES[baudIndex]);
    } else if (millis() - lastBaudSwitch > BAUD_TRY_MS) {
      baudIndex = (baudIndex + 1) % NUM_BAUDS;
      GPSSerial.end();
      GPSSerial.begin(BAUD_CANDIDATES[baudIndex], SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
      lastBaudSwitch = millis();
      Serial.print(F("[GPS] Probando baudrate: "));
      Serial.println(BAUD_CANDIDATES[baudIndex]);
    }
  }

  GPS_TRACE("paso 4: chequeando boton BACK");
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    currentScreen = SCREEN_APPS;
    return;
  }

  GPS_TRACE("paso 5: entrando a gpsFeed()");
  gpsFeed();
  GPS_TRACE("paso 6: gpsFeed() volvio sin colgarse");

  u8g2.clearBuffer();
  GPS_TRACE("paso 7: clearBuffer OK");
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

    snprintf(line, sizeof(line), "Satelites: %d",
              gps.satellites.isValid() ? gps.satellites.value() : 0);
    u8g2.drawStr(2, 34, line);

    snprintf(line, sizeof(line), "Bytes:%lu OK:%lu",
              gps.charsProcessed(), gps.passedChecksum());
    u8g2.drawStr(2, 44, line);

    if (gps.charsProcessed() < 10) {
      u8g2.drawStr(2, 54, "Sin datos del modulo");
    } else if (!baudLocked) {
      snprintf(line, sizeof(line), "Probando baud:%lu", BAUD_CANDIDATES[baudIndex]);
      u8g2.drawStr(2, 54, line);
    } else {
      u8g2.drawStr(2, 54, "Esperando fix...");
    }
  }

  u8g2.drawStr(2, 62, "BACK:Volver");

  GPS_TRACE("paso 8: antes de sendBuffer()");
  u8g2.sendBuffer();
  GPS_TRACE("paso 9: sendBuffer() volvio, vuelta completa OK");
  tracedOnce = true;
}
