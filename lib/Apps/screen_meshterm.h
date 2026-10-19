#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// App "Terminal Chat": envio/recepcion libre de mensajes ESP-NOW por
// serial (USB). Comandos: send <texto>, sendto <id_hex> <texto>,
// nodes/ls, clear/cls, exit. BACK en el dispositivo tambien sale.
void screenMeshTermLoop();
