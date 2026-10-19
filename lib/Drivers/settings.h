#pragma once
#include <Arduino.h>

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

uint8_t settingsGetRemoteMode();
void settingsSetRemoteMode(uint8_t mode);

void settingsGetWifiSSID(char* out, size_t outLen);
void settingsSetWifiSSID(const char* ssid);

void settingsGetWifiPass(char* out, size_t outLen);
void settingsSetWifiPass(const char* pass);

void settingsGetRemoteApSSID(char* out, size_t outLen);
void settingsSetRemoteApSSID(const char* ssid);

void settingsGetRemoteApPass(char* out, size_t outLen);
void settingsSetRemoteApPass(const char* pass);

void settingsFactoryReset();
