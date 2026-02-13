#include "screen_pcmode.h"
#include "Drivers/buzzer.h"
#include "Drivers/neopixel.h"


static const unsigned char image_terminal_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0xfe,0xff,0xff,0x7f,0x02,0x00,0x00,0x40,0x02,0x00,0x00,0x40,
  0x02,0x00,0x00,0x40,0x02,0x00,0x00,0x40,0x72,0x00,0x00,0x40,0x8a,0x00,0x00,0x40,
  0x8a,0x00,0x00,0x40,0x8a,0x00,0x00,0x40,0x72,0x00,0x00,0x40,0x02,0x00,0x00,0x40,
  0x02,0x00,0x00,0x40,0x02,0x00,0x00,0x40,0xfe,0xff,0xff,0x7f,0x00,0x00,0x00,0x00
};

static const unsigned char image_Layer_9_bits[] PROGMEM = {
  0x7e,0x7e,0x7e,0x7e,0x99,0x99,0x99,0x99,
  0x67,0xe6,0x67,0xe6,0x18,0x18,0x18,0x18,
  0x67,0xe6,0x67,0xe6,0x99,0x99,0x99,0x99,
  0x7e,0x7e,0x7e,0x7e
};

static bool pcModeInitialized = false;
static bool isButtonJustPressed(int pin);
static void processSerialCommand();
static void printPrompt();

void screenPCModeLoop() {
  // Inicializar modo PC una sola vez
  if (!pcModeInitialized) {
    Serial.println();
    Serial.println(F("╔════════════════════════════════════╗"));
    Serial.println(F("║     CAMIONETA PC-MODE v1.0         ║"));
    Serial.println(F("║     Terminal de Control            ║"));
    Serial.println(F("╚════════════════════════════════════╝"));
    Serial.println();
    Serial.println(F("Hola! Bienvenido al modo PC"));
    Serial.println(F("Escribe 'help' para ver comandos disponibles"));
    Serial.println();
    printPrompt();
    pcModeInitialized = true;
  }
  
  // Procesar comandos seriales
  processSerialCommand();
  
  // Permitir salir con el botón BACK
  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    pcModeInitialized = false;
    Serial.println();
    Serial.println(F(">> Saliendo de PC-Mode..."));
    currentScreen = SCREEN_AJUSTES;
    return;
  }
  
  // Dibujar interfaz en pantalla
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  
  // Header
  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(35, 11, "PC-MODE");
  u8g2.setDrawColor(1);
  
  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);
  
  // Terminal icon
  u8g2.drawXBM(48, 24, 32, 16, image_terminal_bits);
  
  // Status text
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(25, 48, "Terminal Activo");
  u8g2.drawStr(15, 58, "Ver Monitor Serial");
  
  u8g2.sendBuffer();
}

static void printPrompt() {
  Serial.print(F("camioneta> "));
}

static void processSerialCommand() {
  static String commandBuffer = "";
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        // Procesar comando
        commandBuffer.trim();
        Serial.println(); // Nueva línea después del comando
        
        if (commandBuffer == "help") {
          Serial.println(F("╔════════════════════════════════════╗"));
          Serial.println(F("║        COMANDOS DISPONIBLES        ║"));
          Serial.println(F("╠════════════════════════════════════╣"));
          Serial.println(F("║ help     - Muestra esta ayuda      ║"));
          Serial.println(F("║ status   - Estado del sistema      ║"));
          Serial.println(F("║ info     - Información del device  ║"));
          Serial.println(F("║ rgb/R/G/B- Control de NeoPixels    ║"));
          Serial.println(F("║ buzzer   - Prueba del buzzer       ║"));
          Serial.println(F("║ exit     - Salir del PC-Mode       ║"));
          Serial.println(F("╚════════════════════════════════════╝"));
        }
        else if (commandBuffer == "status") {
          Serial.println(F("╔════════════════════════════════════╗"));
          Serial.println(F("║       ESTADO DEL SISTEMA           ║"));
          Serial.println(F("╠════════════════════════════════════╣"));
          Serial.print(F("║ RGB R: ")); Serial.print(neoR); Serial.println(F("                         ║"));
          Serial.print(F("║ RGB G: ")); Serial.print(neoG); Serial.println(F("                         ║"));
          Serial.print(F("║ RGB B: ")); Serial.print(neoB); Serial.println(F("                         ║"));
          Serial.print(F("║ Uptime: ")); Serial.print(millis()/1000); Serial.println(F(" seg               ║"));
          Serial.println(F("╚════════════════════════════════════╝"));
        }
        else if (commandBuffer == "info") {
          Serial.println(F("╔════════════════════════════════════╗"));
          Serial.println(F("║      INFORMACIÓN DEL DEVICE        ║"));
          Serial.println(F("╠════════════════════════════════════╣"));
          Serial.println(F("║ Proyecto: Camioneta                ║"));
          Serial.println(F("║ Version: 1.0                       ║"));
          Serial.println(F("║ Autor: Pablo Lopez                 ║"));
          Serial.println(F("║ Lab: Tesla Lab                     ║"));
          Serial.println(F("║ NeoPixels: 9                       ║"));
          Serial.println(F("║ Display: SH1106 128x64             ║"));
          Serial.println(F("╚════════════════════════════════════╝"));
        }
        else if (commandBuffer.startsWith("rgb/")) {
          int r, g, b;
          // leer rgb/R/G/B
          if (sscanf(commandBuffer.c_str(), "rgb/%d/%d/%d", &r, &g, &b) == 3) {
            r = constrain(r, 0, 255);
            g = constrain(g, 0, 255);
            b = constrain(b, 0, 255);
            neoR = r;
            neoG = g;
            neoB = b;
            for (int i = 0; i < NUM_PIXELS; i++) {
              neopixelSetPixel(i, r, g, b);
            }
            neopixelShow();
            Serial.println(F("RGB actualizado"));
          } else {
            Serial.println(F("Formato invalido. Usa rgb/R/G/B"));
          }
        }

        else if (commandBuffer == "buzzer") {
          Serial.println(F(">> Probando buzzer..."));
          buzzerBeep();
          delay(750);
          buzzerBeep();
          Serial.println(F(">> Test completado"));
        }
        else if (commandBuffer == "exit") {
          Serial.println(F(">> Saliendo de PC-Mode..."));
          pcModeInitialized = false;
          currentScreen = SCREEN_AJUSTES;
          return;
        }
        else if(commandBuffer == "Tesla"){
          Serial.print(F("       ++++++++++++++++++++++++++++++++++++        \n"));
          Serial.print(F("    ++++++++++++++++++++++++++++++++++++++        \n"));
          Serial.print(F("   ++++++++++++++++++++++++++++++++++++++         \n"));
          Serial.print(F(" ++++++++++++++++++++++++++++ ++++++++++          \n"));
          Serial.print(F("+++++++++++++++++++++++++++  +++++++++++          \n"));
          Serial.print(F("        +++++++++++         ++++++++++=           \n"));
          Serial.print(F("       +++++++++++          ++++++++++            \n"));
          Serial.print(F("       ++++++++++          +++++++++++            \n"));
          Serial.print(F("      +++++++++++ ++++++++++++++++++++++++++++++++\n"));
          Serial.print(F("     +++++++++++ ++++++++++++++++++++++++++++++++ \n"));
          Serial.print(F("    =++++++++++=+++++++++++++++++++++++++++++++   \n"));
          Serial.print(F("    ++++++++++++++++++++++++++++++++++++++++++    \n"));
          Serial.print(F("   =++++++++++++++++++++++++++++++++++++++++      \n"));
        }
        else if(commandBuffer == "Goth"){
          Serial.print(F("##################+++++--------------..........                 \n"));
          Serial.print(F("#################++++++-------------........                    \n"));
          Serial.print(F("###############+++++++--------------........                    \n"));
          Serial.print(F("#############++++++++-------------##--......                    \n"));
          Serial.print(F("###############+++++-------------####-......                    \n"));
          Serial.print(F("##############+++++--------------#####-.....  #-                \n"));
          Serial.print(F("#############++++++------------.#######-....  -##.              \n"));
          Serial.print(F("#############++++++-----------.#########-.... .####             \n"));
          Serial.print(F("############++++++--+--------.-##########....  ######.          \n"));
          Serial.print(F("############+++++++-++------..############...  .#######+ #.     \n"));
          Serial.print(F("###########++++++++-+------..-#######+........  ###.      -#+   \n"));
          Serial.print(F("###########-++-++----------..###############-.  -######+++#####+\n"));
          Serial.print(F("###########----------------..###-###+ +#######   +###-.-  ++ .##\n"));
          Serial.print(F("###########-------.--------..##################   ##+##. -+.####\n"));
          Serial.print(F("###########-----.-.-------...###################+ -#############\n"));
          Serial.print(F("###########-----...-------...#####################-############+\n"));
          Serial.print(F("###########+---......-----.  .################################# \n"));
          Serial.print(F("++++++++++#+---.......---..   +##############################+  \n"));
          Serial.print(F("++++++++++++---.      .....   ##############################+ # \n"));
          Serial.print(F("----++----++....       ....   -#############################+#  \n"));
          Serial.print(F("------------.-.         ...    +##############################  \n"));
          Serial.print(F("--------------. .-        .     #############################-  \n"));
          Serial.print(F("-+++++++++++++- --.              -#########   ..    -+++####-   \n"));
          Serial.print(F("--------------....-.              -#######+.+..      ++++++-    \n"));
          Serial.print(F("....-..-....-.......              .############+--+++++++-+     \n"));
          Serial.print(F(" ....................              ###+###########+++++-+++     \n"));
          Serial.print(F(" .. .... .. ..........             -+++++#########+++-++++.     \n"));
          Serial.print(F(" .. .. . .. ..     .                ++++++++#####+++++++++      \n"));
          Serial.print(F("                                    ++++++++++++++++++++++      \n"));

        }
        else if (commandBuffer == "logcap") {
          Serial.println(F(">> Buscando log de captive portal"));
          pcModeInitialized = false;
          currentScreen = SCREEN_AJUSTES;
          return;
        }

        else {
          Serial.print(F(">> Comando desconocido: '"));
          Serial.print(commandBuffer);
          Serial.println(F("'"));
          Serial.println(F(">> Escribe 'help' para ver comandos disponibles"));
        }
        
        commandBuffer = "";
        Serial.println();
        printPrompt();
      }
    }
    else if (c == 8 || c == 127) { // Backspace
      if (commandBuffer.length() > 0) {
        commandBuffer.remove(commandBuffer.length() - 1);
        Serial.write(8);   // Backspace
        Serial.write(' '); // Espacio
        Serial.write(8);   // Backspace
      }
    }
    else if (c >= 32 && c <= 126) { // Caracteres imprimibles
      commandBuffer += c;
      Serial.write(c); // Echo
    }
  }
}

static bool isButtonJustPressed(int pin) {
  static uint8_t lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t lastReading[4]     = {HIGH, HIGH, HIGH, HIGH};
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};

  const int pins[4] = {PIN_SELECT, PIN_UP, PIN_DOWN, PIN_BACK};
  int index = -1;

  for (int i = 0; i < 4; i++) {
    if (pins[i] == pin) {
      index = i;
      break;
    }
  }
  if (index == -1) return false;

  uint8_t reading = digitalRead(pin);

  if (reading != lastReading[index]) {
    lastDebounceTime[index] = millis();
    lastReading[index] = reading;
  }

  if ((millis() - lastDebounceTime[index]) > 50) {
    if (lastStableState[index] == HIGH && reading == LOW) {
      lastStableState[index] = reading;
      return true;
    }
    lastStableState[index] = reading;
  }

  return false;
}
