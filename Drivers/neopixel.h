#pragma once
#include <Adafruit_NeoPixel.h>
#include "config.h"

extern Adafruit_NeoPixel strip;

struct PixelColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

const PixelColor pixelColors[NUM_PIXELS] = {
  {128, 0, 255},     //1
  {128, 0, 255},   //2
  {128, 0, 255},   //3
  {128, 0, 255},     //4
  {128, 0, 255},   //5
  {128, 0, 255},   //6
  {255, 255, 255},   //7
  {255, 255, 255},    //8
  {255, 255, 255}    //9
};

// Presets de color para el efecto "breath" (respiracion) de despues del
// splash. Elegibles desde Ajustes > Configuracion > Color Breath, y
// guardados en NVS. Para agregar uno nuevo alcanza con sumar una fila
// aca -- BREATH_PRESET_COUNT se calcula solo.
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
void neopixelSetPixelCustom(int index);
void neopixelShow();
void neopixelSplashSequence(int &currentPixel, unsigned long &lastTime);
void neopixelBreathe(uint8_t &brightness, int8_t &direction, unsigned long &lastTime,
                      uint8_t r, uint8_t g, uint8_t b);
void neopixelFadeOut();
void neopixelSetBrightness(uint8_t brightness);  // 0-255, se aplica de una (strip.show())
