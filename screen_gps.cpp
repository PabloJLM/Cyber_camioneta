#include "Apps/screen_gps.h"
#include "Drivers/buzzer.h"
#include <TinyGPSPlus.h>
#include <string.h>

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static const int GPS_RX_PIN = 4;        // IO4 = "RX1"
static const int GPS_TX_PIN = 5;        // IO5 = "TX1"
static const unsigned long GPS_BAUD = 115200;

static HardwareSerial GPSSerial(1);  // UART1: hardware separado del UART0/consola
static TinyGPSPlus gps;
static bool gpsStarted = false;

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
