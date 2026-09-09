#include "Drivers/settings.h"
#include <Preferences.h>

static Preferences prefs;
static const char* NS = "camioneta";

void settingsInit() {

}

uint8_t settingsGetSplashIndex() {
  prefs.begin(NS, true);
  uint8_t v = prefs.getUChar("splash", 0);
  prefs.end();
  return v;
}

void settingsSetSplashIndex(uint8_t index) {
  prefs.begin(NS, false);
  prefs.putUChar("splash", index);
  prefs.end();
}

uint8_t settingsGetBreathColorIndex() {
  prefs.begin(NS, true);
  uint8_t v = prefs.getUChar("breath", 0);
  prefs.end();
  return v;
}

void settingsSetBreathColorIndex(uint8_t index) {
  prefs.begin(NS, false);
  prefs.putUChar("breath", index);
  prefs.end();
}

uint8_t settingsGetBrightness() {
  prefs.begin(NS, true);
  uint8_t v = prefs.getUChar("bright", 150);  
  prefs.end();
  return v;
}

void settingsSetBrightness(uint8_t value) {
  prefs.begin(NS, false);
  prefs.putUChar("bright", value);
  prefs.end();
}

void settingsFactoryReset() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}
