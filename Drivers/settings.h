#pragma once
#include <Arduino.h>

// Configuracion global persistente en NVS (flash no volatil del ESP32)

void settingsInit();  

uint8_t settingsGetSplashIndex();
void settingsSetSplashIndex(uint8_t index);

uint8_t settingsGetBreathColorIndex();
void settingsSetBreathColorIndex(uint8_t index);

uint8_t settingsGetBrightness();
void settingsSetBrightness(uint8_t value);

void settingsFactoryReset(); 
