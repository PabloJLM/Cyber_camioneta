#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void screenCaptiveLoop();
void startCaptivePortal();
void stopCaptivePortal();
bool isCaptiveRunning();

// De donde sale el html/css del portal (/portal/index.html, /portal/style.css).
// AUTO es el comportamiento de siempre: usa la SD si el archivo esta ahi,
// si no cae al html embebido en el firmware. Con EMBEDDED/SD/FS se fuerza
// una fuente puntual, configurable desde la terminal de Ajustes (comando
// "portalsrc auto|embebido|sd|fs").
enum CaptivePortalSource {
  PORTAL_SRC_AUTO = 0,
  PORTAL_SRC_EMBEDDED,
  PORTAL_SRC_SD,
  PORTAL_SRC_FS,       // LittleFS: propio filesystem interno del ESP32
};

void captiveSetPortalSource(CaptivePortalSource src);
CaptivePortalSource captiveGetPortalSource();
const char* captivePortalSourceName();   // texto corto para mostrar en pantalla/terminal

// Nombre de la red (SSID) que levanta el portal cautivo. Se guarda en
// NVS (Drivers/settings.h) asi el nombre sobrevive un reinicio.
void captiveSetSSID(const char* ssid);
const char* captiveGetSSID();
