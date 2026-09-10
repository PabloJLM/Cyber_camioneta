#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void screenSnifferLoop();

// Nombre base de los archivos .pcap que genera el sniffer. Por defecto
// es "cap" (cap1.pcap, cap2.pcap, ...); se puede cambiar desde la
// terminal de Ajustes. Maximo 12 caracteres, solo letras/numeros/'_'.
void snifferSetBaseName(const char* name);
const char* snifferGetBaseName();

// Canal WiFi donde el sniffer empieza a escuchar al entrar a la app
// (1-13). Se guarda en NVS via Drivers/settings.h, configurable con
// "sniffer channel <n>" desde la terminal de Ajustes. Dentro de la
// app UP/DOWN lo siguen cambiando en vivo, igual que antes.
void snifferSetChannel(uint8_t ch);
uint8_t snifferGetChannel();
