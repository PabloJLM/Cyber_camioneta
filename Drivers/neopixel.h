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


void neopixelInit();
void neopixelClear();
void neopixelSetPixel(int index, uint8_t r, uint8_t g, uint8_t b);
void neopixelSetPixelCustom(int index);
void neopixelShow();
void neopixelSplashSequence(int &currentPixel, unsigned long &lastTime);
void neopixelBreathe(uint8_t &brightness, int8_t &direction, unsigned long &lastTime);
void neopixelFadeOut();
