#include "Drivers/buzzer.h"

void buzzerWelcome() {
  tone(PIN_BUZZER, 800, 100);
  delay(120);
  tone(PIN_BUZZER, 1200, 100);
  delay(120);
  tone(PIN_BUZZER, 1600, 150);
}

void buzzerClick() {
  tone(PIN_BUZZER, 2000, 30);
}

void buzzerBeep() {
  tone(PIN_BUZZER, 1000, 100);
}
