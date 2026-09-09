#pragma once
#include <Adafruit_NeoPixel.h>
#include "config.h"

extern Adafruit_NeoPixel strip;

// Presets de color para el splash 
struct BreathColor {
  const char* name;
  uint8_t r, g, b;
};

static const BreathColor BREATH_PRESETS[] = {
  { "Morado", 128, 0,   255 },
  { "Azul",   0,   90,  255 },
  { "Rojo",   255, 0,   0   },
  { "Verde",  0,   255, 60  },
  { "Blanco", 255, 255, 255 },
  { "Cian",   0,   255, 255 },
  { "Ambar",  255, 140, 0   },
};
static const uint8_t BREATH_PRESET_COUNT = sizeof(BREATH_PRESETS) / sizeof(BREATH_PRESETS[0]);

void neopixelInit();
void neopixelClear();
void neopixelSetPixel(int index, uint8_t r, uint8_t g, uint8_t b);
void neopixelShow();
void neopixelSplashSequence(int &currentPixel, unsigned long &lastTime,
                             uint8_t r, uint8_t g, uint8_t b);
void neopixelBreathe(uint8_t &brightness, int8_t &direction, unsigned long &lastTime,
                      uint8_t r, uint8_t g, uint8_t b);
void neopixelFadeOut(uint8_t r, uint8_t g, uint8_t b);
void neopixelSetBrightness(uint8_t brightness);  
