#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

#define APFLOOD_MAX_MSGS    6
#define APFLOOD_MAX_SSIDLEN 32

void screenApFloodLoop();

int apFloodSetMessages(const char* const* msgs, int count);
int apFloodGetMessageCount();
const char* apFloodGetMessage(int i);

void apFloodSetInterval(uint16_t ms);
uint16_t apFloodGetInterval();
