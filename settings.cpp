#include "Drivers/settings.h"
#include <Preferences.h>

static Preferences prefs;
static const char* NS = "camioneta";

static const uint8_t BTSPAM_MASK_DEFAULT = 0x07;
static const char*   PORTAL_SSID_DEFAULT = "WiFi_Gratis1";
static const char*   REMOTE_AP_SSID_DEFAULT = "Camioneta-Remote";
static const char*   REMOTE_AP_PASS_DEFAULT = "12345678";

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

uint8_t settingsGetBtSpamMask() {
  prefs.begin(NS, true);
  uint8_t v = prefs.getUChar("btmask", BTSPAM_MASK_DEFAULT);
  prefs.end();
  return v;
}

void settingsSetBtSpamMask(uint8_t mask) {
  prefs.begin(NS, false);
  prefs.putUChar("btmask", mask);
  prefs.end();
}

uint16_t settingsGetApFloodInterval() {
  prefs.begin(NS, true);
  uint16_t v = prefs.getUShort("floodint", 0);
  prefs.end();
  return v;
}

void settingsSetApFloodInterval(uint16_t ms) {
  prefs.begin(NS, false);
  prefs.putUShort("floodint", ms);
  prefs.end();
}

uint8_t settingsGetSnifferChannel() {
  prefs.begin(NS, true);
  uint8_t v = prefs.getUChar("snifch", 1);
  prefs.end();
  return v;
}

void settingsSetSnifferChannel(uint8_t ch) {
  prefs.begin(NS, false);
  prefs.putUChar("snifch", ch);
  prefs.end();
}

void settingsGetPortalSSID(char* out, size_t outLen) {
  prefs.begin(NS, true);
  String v = prefs.getString("portalssid", PORTAL_SSID_DEFAULT);
  prefs.end();
  strncpy(out, v.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
}

void settingsSetPortalSSID(const char* ssid) {
  prefs.begin(NS, false);
  prefs.putString("portalssid", ssid);
  prefs.end();
}

uint8_t settingsGetRemoteMode() {
  prefs.begin(NS, true);
  uint8_t v = prefs.getUChar("remmode", 0);
  prefs.end();
  return v;
}

void settingsSetRemoteMode(uint8_t mode) {
  prefs.begin(NS, false);
  prefs.putUChar("remmode", mode);
  prefs.end();
}

void settingsGetWifiSSID(char* out, size_t outLen) {
  prefs.begin(NS, true);
  String v = prefs.getString("wifissid", "");
  prefs.end();
  strncpy(out, v.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
}

void settingsSetWifiSSID(const char* ssid) {
  prefs.begin(NS, false);
  prefs.putString("wifissid", ssid);
  prefs.end();
}

void settingsGetWifiPass(char* out, size_t outLen) {
  prefs.begin(NS, true);
  String v = prefs.getString("wifipass", "");
  prefs.end();
  strncpy(out, v.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
}

void settingsSetWifiPass(const char* pass) {
  prefs.begin(NS, false);
  prefs.putString("wifipass", pass);
  prefs.end();
}

void settingsGetRemoteApSSID(char* out, size_t outLen) {
  prefs.begin(NS, true);
  String v = prefs.getString("remapssid", REMOTE_AP_SSID_DEFAULT);
  prefs.end();
  strncpy(out, v.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
}

void settingsSetRemoteApSSID(const char* ssid) {
  prefs.begin(NS, false);
  prefs.putString("remapssid", ssid);
  prefs.end();
}

void settingsGetRemoteApPass(char* out, size_t outLen) {
  prefs.begin(NS, true);
  String v = prefs.getString("remappass", REMOTE_AP_PASS_DEFAULT);
  prefs.end();
  strncpy(out, v.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
}

void settingsSetRemoteApPass(const char* pass) {
  prefs.begin(NS, false);
  prefs.putString("remappass", pass);
  prefs.end();
}

void settingsFactoryReset() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}
