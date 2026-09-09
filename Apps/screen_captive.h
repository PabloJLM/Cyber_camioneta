#pragma once
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void screenCaptiveLoop();
void startCaptivePortal();
void stopCaptivePortal();
bool isCaptiveRunning();

// De donde sale el html/css del portal (/portal/index.html, /portal/style.css).
// hace los ifs si esta en sd, fs o embebido
enum CaptivePortalSource {
  PORTAL_SRC_AUTO = 0,
  PORTAL_SRC_EMBEDDED,
  PORTAL_SRC_SD,
  PORTAL_SRC_FS,       
};

void captiveSetPortalSource(CaptivePortalSource src);
CaptivePortalSource captiveGetPortalSource();
const char* captivePortalSourceName();   
