#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// Terminal serial exclusiva de Ajustes: reemplaza a PC-Mode en este
// menu (PC-Mode ahora vive en Apps, es un juguete, no una pantalla de
// configuracion). Esta terminal SOLO toca configuracion: mensajes del
// AP Flood, nombre base de los archivos del sniffer, y de donde sale
// el html/css del captive portal. Nada de piano, nada de ASCII art.
void screenAjustesTermLoop();
