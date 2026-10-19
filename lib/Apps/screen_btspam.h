#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void screenBtSpamLoop();

int btSpamGetTypeCount();
const char* btSpamGetTypeName(int i);
bool btSpamIsTypeUnstable(int i);
bool btSpamIsTypeEnabled(int i);
void btSpamSetTypeEnabled(int i, bool enabled);
