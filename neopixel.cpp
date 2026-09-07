#include "Drivers/neopixel.h"

void neopixelInit() {
  strip.begin();
  strip.show();
  strip.setBrightness(100);
}

void neopixelClear() {
  strip.clear();
  strip.show();
}

void neopixelSetPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index >= 0 && index < NUM_PIXELS) {
    strip.setPixelColor(index, strip.Color(r, g, b));
  }
}

void neopixelSetPixelCustom(int index) {
  if (index >= 0 && index < NUM_PIXELS) {
    strip.setPixelColor(index, strip.Color(
      pixelColors[index].r,
      pixelColors[index].g,
      pixelColors[index].b
    ));
  }
}

void neopixelShow() {
  strip.show();
}

// Encender neopixeles
void neopixelSplashSequence(int &currentPixel, unsigned long &lastTime) {
  unsigned long currentTime = millis();

  // Encender un pixel cada 200ms
  if (currentTime - lastTime > 200 && currentPixel < NUM_PIXELS) {
    neopixelSetPixelCustom(currentPixel);
    strip.show();
    currentPixel++;
    lastTime = currentTime;
  }
}

// Color configurable (ver BREATH_PRESETS en neopixel.h / pantalla
// Ajustes > Configuracion > Color Breath) en vez del patron fijo de
// pixelColors -- asi todo el tira "respira" con un solo color elegido.
void neopixelBreathe(uint8_t &brightness, int8_t &direction, unsigned long &lastTime,
                      uint8_t r, uint8_t g, uint8_t b) {
  unsigned long currentTime = millis();

  if (currentTime - lastTime > 20) {
    brightness += direction * 5;

    if (brightness >= 255) {
      brightness = 255;
      direction = -1;
    } else if (brightness <= 50) {
      brightness = 50;
      direction = 1;
    }


    for (int i = 0; i < NUM_PIXELS; i++) {
      uint8_t rr = (uint16_t)r * brightness / 255;
      uint8_t gg = (uint16_t)g * brightness / 255;
      uint8_t bb = (uint16_t)b * brightness / 255;
      strip.setPixelColor(i, strip.Color(rr, gg, bb));
    }
    strip.show();
    lastTime = currentTime;
  }
}


void neopixelFadeOut() {
  for (int brightness = 255; brightness >= 0; brightness -= 10) {
    for (int i = 0; i < NUM_PIXELS; i++) {
      uint8_t r = pixelColors[i].r * brightness / 255;
      uint8_t g = pixelColors[i].g * brightness / 255;
      uint8_t b = pixelColors[i].b * brightness / 255;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    delay(20);
  }
  strip.clear();
  strip.show();
}

void neopixelSetBrightness(uint8_t brightness) {
  strip.setBrightness(brightness);
  strip.show();
}
