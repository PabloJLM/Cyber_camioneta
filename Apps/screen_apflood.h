#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// Limites de los SSID falsos del AP Flood.
#define APFLOOD_MAX_MSGS    6
#define APFLOOD_MAX_SSIDLEN 32   // limite real de un SSID 802.11

void screenApFloodLoop();

// Reemplaza los SSID que transmite el AP Flood (desde PC-Mode, comando
// flood{...}). Cada mensaje se recorta a APFLOOD_MAX_SSIDLEN bytes y
// se aceptan como maximo APFLOOD_MAX_MSGS. Devuelve cuantos quedaron.
int apFloodSetMessages(const char* const* msgs, int count);

// Cuantos mensajes hay cargados actualmente.
int apFloodGetMessageCount();

// Mensaje actual en la posicion i (0-based). Devuelve "" si i esta
// fuera de rango. Sirve para mostrar la lista desde la terminal de
// Ajustes sin duplicar el arreglo interno.
const char* apFloodGetMessage(int i);

// Cada cuanto se manda una tanda completa de beacons falsos, en ms.
// 0 = lo mas rapido posible (comportamiento de siempre). Se guarda en
// NVS via Drivers/settings.h, configurable con "apflood interval <ms>"
// desde la terminal de Ajustes.
void apFloodSetInterval(uint16_t ms);
uint16_t apFloodGetInterval();
