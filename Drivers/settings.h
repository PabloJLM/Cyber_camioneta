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

uint8_t settingsGetBtSpamMask();
void settingsSetBtSpamMask(uint8_t mask);

uint16_t settingsGetApFloodInterval();
void settingsSetApFloodInterval(uint16_t ms);

uint8_t settingsGetSnifferChannel();
void settingsSetSnifferChannel(uint8_t ch);

void settingsGetPortalSSID(char* out, size_t outLen);
void settingsSetPortalSSID(const char* ssid);

void settingsFactoryReset();
