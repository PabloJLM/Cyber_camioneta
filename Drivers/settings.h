#pragma once
#include <Arduino.h>

// Configuracion global persistente en NVS (flash no volatil del ESP32,
// via la libreria Preferences que ya trae el core -- no hace falta
// instalar nada). Un solo lugar para leer/escribir todo lo que se
// guarda desde la pantalla "Configuracion" en Ajustes: sobrevive a
// reinicios y a reflashear el firmware (mientras no se borre la flash
// entera).

void settingsInit();  // no hace falta llamarla, existe por si a futuro hace falta migrar datos

uint8_t settingsGetSplashIndex();
void settingsSetSplashIndex(uint8_t index);

uint8_t settingsGetBreathColorIndex();
void settingsSetBreathColorIndex(uint8_t index);

// Brillo global de los neopixeles, guardado como el valor crudo 0-255
// que despues se pasa directo a strip.setBrightness().
uint8_t settingsGetBrightness();
void settingsSetBrightness(uint8_t value);

void settingsFactoryReset();  // borra toda la configuracion guardada
