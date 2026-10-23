#include "screen_gps.h"
#include "buzzer.h"
#include <TinyGPSPlus.h>
#include <string.h>
#include <stdlib.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static const int GPS_RX_PIN = 4;
static const int GPS_TX_PIN = 5;
static const unsigned long GPS_BAUD = 115200;

static HardwareSerial GPSSerial(1);
static TinyGPSPlus gps;
static bool gpsStarted = false;

// Satelites en vista (se llena aunque todavia no haya fix; confirma que la
// antena si esta captando algo, sin depender del "ANTENNA OPEN/SHORT" del
// modulo, que en esta placa no aplica (ver LNA externo AT2659 en el hardware).
// Se escuchan todos los talkers de GSV posibles porque el modulo puede
// mandar GPGSV, GNGSV (combinado), GLGSV, GAGSV o GBGSV segun que
// constelaciones tenga activas - no asumimos cual usa.
static TinyGPSCustom gpSatsInView(gps, "GPGSV", 3);
static TinyGPSCustom gnSatsInView(gps, "GNGSV", 3);
static TinyGPSCustom glSatsInView(gps, "GLGSV", 3);
static TinyGPSCustom gaSatsInView(gps, "GAGSV", 3);
static TinyGPSCustom gbSatsInView(gps, "GBGSV", 3);

static int getSatsInView() {
  int total = 0;
  total += atoi(gpSatsInView.value());
  total += atoi(gnSatsInView.value());
  total += atoi(glSatsInView.value());
  total += atoi(gaSatsInView.value());
  total += atoi(gbSatsInView.value());
  return total;
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
    gps.encode(GPSSerial.read());
  }
}

// Mismos comandos PMTK que usa Minino (gps_hw_configure_advanced) para el
// ATGM336H: fuerza multi-constelacion y que sentencias NMEA manda, por si el
// modulo por default no estaba mandando GSV (de ahi que "Sats en vista" se
// quedara en 0 aunque el UART ya estuviera recibiendo datos validos).
static void gpsConfigure() {
  // PMTK353: habilita GPS,GLONASS,Galileo,BeiDou,QZSS
  GPSSerial.print("$PMTK353,1,1,1,1,1,0,0,0,0*2A\r\n");
  delay(100);
  // PMTK314: habilita GLL,RMC,VTG,GGA,GSA,GSV en cada fix (incluye GSV)
  GPSSerial.print("$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n");
  delay(100);
}

void screenGPSLoop() {
  if (!gpsStarted) {

    pinMode(PIN_GPSON, OUTPUT);
    digitalWrite(PIN_GPSON, HIGH);  // enciende el modulo GPS (antes quedaba como INPUT, nunca se energizaba)
    delay(300);                     // da tiempo a que arranque antes de leer el UART

    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    gpsConfigure();
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
  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(28, 11, "GPS Position");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

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

    char svLine[24];
    snprintf(svLine, sizeof(svLine), "Sats en vista: %d", getSatsInView());
    u8g2.drawStr(2, 34, svLine);

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
