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

// BT Spam: que fabricantes estan activos en la rotacion. Un bit por
// tipo (bit 0 = Apple, bit 1 = Microsoft, etc, en el mismo orden que
// el enum SpamType de screen_btspam.cpp). Por defecto solo estan
// prendidos Apple, Microsoft y Samsung (los que no dan crash).
uint8_t settingsGetBtSpamMask();
void settingsSetBtSpamMask(uint8_t mask);

// AP Flood: cada cuanto se manda una tanda de beacons falsos, en ms.
// 0 = lo mas rapido posible (como era antes de tener este ajuste).
uint16_t settingsGetApFloodInterval();
void settingsSetApFloodInterval(uint16_t ms);

// Sniffer: canal WiFi donde escucha por defecto al entrar a la app.
uint8_t settingsGetSnifferChannel();
void settingsSetSnifferChannel(uint8_t ch);

// Captive Portal: nombre de la red que levanta el AP.
void settingsGetPortalSSID(char* out, size_t outLen);
void settingsSetPortalSSID(const char* ssid);

void settingsFactoryReset();
