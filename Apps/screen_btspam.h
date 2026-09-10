#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void screenBtSpamLoop();

// API para la pantalla de ajustes de BT Spam (checkbox por fabricante).
// El orden de los indices es fijo: 0 Apple, 1 Microsoft, 2 Samsung,
// 3 Google Fast Pair, 4 Flipper Zero -- igual al enum SpamType interno.
int btSpamGetTypeCount();
const char* btSpamGetTypeName(int i);
bool btSpamIsTypeUnstable(int i);   // true = dio crash en hardware real, mostrar aviso
bool btSpamIsTypeEnabled(int i);
void btSpamSetTypeEnabled(int i, bool enabled);
