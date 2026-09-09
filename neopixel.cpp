#include "Drivers/neopixel.h"

//Funciones para neos simplificadas xd 
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

void neopixelShow() {
  strip.show();
}

// Encender neopixeles con el color elegido 
void neopixelSplashSequence(int &currentPixel, unsigned long &lastTime,
                             uint8_t r, uint8_t g, uint8_t b) {
  unsigned long currentTime = millis();

  // Encender un pixel cada 200ms
  if (currentTime - lastTime > 200 && currentPixel < NUM_PIXELS) {
    neopixelSetPixel(currentPixel, r, g, b);
    strip.show();
    currentPixel++;
    lastTime = currentTime;
  }
}

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


void neopixelFadeOut(uint8_t r, uint8_t g, uint8_t b) {
  for (int brightness = 255; brightness >= 0; brightness -= 10) {
    uint8_t rr = (uint16_t)r * brightness / 255;
    uint8_t gg = (uint16_t)g * brightness / 255;
    uint8_t bb = (uint16_t)b * brightness / 255;
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixelColor(i, strip.Color(rr, gg, bb));
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
