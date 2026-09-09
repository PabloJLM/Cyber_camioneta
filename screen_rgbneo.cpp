#include "Ajustes/screen_rgbneo.h"
#include "Drivers/buzzer.h"
#include "Drivers/neopixel.h"

static const unsigned char image_ButtonCenter_bits[] PROGMEM = {
  0x1c,0x22,0x5d,0x5d,0x5d,0x22,0x1c
};

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static int selectedSlider = 0; // 0=R, 1=G, 2=B
static unsigned long lastUpdateTime = 0;

static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t lastReading[4]     = {HIGH, HIGH, HIGH, HIGH};
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};

  const int pins[4] = {PIN_SELECT, PIN_UP, PIN_DOWN, PIN_BACK};
  int index = -1;

  for (int i = 0; i < 4; i++) {
    if (pins[i] == pin) {
      index = i;
      break;
    }
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

// Mapear valor 0-255 a posición X en pantalla (16 a 115 píxeles)
int mapValueToX(uint8_t value) {
  return 16 + (value * 99 / 255);
}

// Aplicar el color actual a los NeoPixels
void updateNeoPixels() {
  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(neoR, neoG, neoB));
  }
  strip.show();
}

void screenRGBNeoLoop() {
  unsigned long currentTime = millis();
  bool valueChanged = false;

  // Navegar entre sliders con UP/DOWN
  if (isButtonJustPressed(PIN_UP)) {
    selectedSlider--;
    if (selectedSlider < 0) selectedSlider = 2;
    buzzerClick();
  }

  if (isButtonJustPressed(PIN_DOWN)) {
    selectedSlider++;
    if (selectedSlider > 2) selectedSlider = 0;
    buzzerClick();
  }

  // Ajustar valores con SELECT (incrementar de 5 en 5)
  if (isButtonJustPressed(PIN_SELECT)) {
    switch(selectedSlider) {
      case 0: // R
        neoR += 5;
        if (neoR < 5) neoR = 255; // Overflow wrap
        break;
      case 1: // G
        neoG += 5;
        if (neoG < 5) neoG = 255;
        break;
      case 2: // B
        neoB += 5;
        if (neoB < 5) neoB = 255;
        break;
    }
    valueChanged = true;
    buzzerClick();
  }

  // Mantener presionado para ajuste continuo
  if (!digitalRead(PIN_SELECT)) {
    if (currentTime - lastUpdateTime > 100) {
      switch(selectedSlider) {
        case 0:
          neoR = (neoR + 5) % 256;
          break;
        case 1:
          neoG = (neoG + 5) % 256;
          break;
        case 2:
          neoB = (neoB + 5) % 256;
          break;
      }
      valueChanged = true;
      lastUpdateTime = currentTime;
    }
  }

  if (isButtonJustPressed(PIN_BACK)) {
    currentScreen = SCREEN_AJUSTES;
    buzzerClick();
    return;
  }

  // Actualizar NeoPixels si cambió algún valor
  if (valueChanged) {
    updateNeoPixels();
  }

  // Dibujar pantalla
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  // Título
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(31, 11, "Neo Control");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  // Labels
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(6, 34, "R");
  u8g2.drawStr(6, 45, "G");
  u8g2.drawStr(6, 57, "B");

  // Líneas de los sliders
  u8g2.drawLine(17, 29, 120, 29);
  u8g2.drawLine(17, 40, 120, 40);
  u8g2.drawLine(17, 52, 120, 52);

  // Dibujar los controles en sus posiciones
  int xPosR = mapValueToX(neoR);
  int xPosG = mapValueToX(neoG);
  int xPosB = mapValueToX(neoB);

  u8g2.drawXBM(xPosR, 26, 7, 7, image_ButtonCenter_bits);
  u8g2.drawXBM(xPosG, 37, 7, 7, image_ButtonCenter_bits);
  u8g2.drawXBM(xPosB, 49, 7, 7, image_ButtonCenter_bits);

  // Indicador de slider seleccionado (marco más grueso)
  int yIndicator[] = {26, 37, 49};
  int xPos[] = {xPosR, xPosG, xPosB};

  // Dibujar marco alrededor del slider seleccionado
  u8g2.drawFrame(xPos[selectedSlider] - 1, yIndicator[selectedSlider] - 1, 9, 9);

  u8g2.sendBuffer();
}
