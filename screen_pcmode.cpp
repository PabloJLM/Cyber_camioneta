#include "Apps/screen_pcmode.h"
#include "Drivers/buzzer.h"
#include "Drivers/neopixel.h"
#include "Apps/screen_apflood.h"
#include <SD.h>
#include <SPI.h>
#include "config.h"

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
static bool pianoMode = false;
static bool isButtonJustPressed(int pin);
static void processSerialCommand();
static void printPrompt();
static void listSD();
static void catFile(String path);
static void processPiano(String in);
static void printPianoHelp();
static int  noteFreq(String n);
static void processFloodCommand(const String& cmd);

static void printSection(const char* title) {
  Serial.println();
  Serial.print(F("== "));
  Serial.println(title);
}

static void printRow(const char* key, const String& value) {
  Serial.print(F("  "));
  Serial.print(key);
  int pad = 14 - strlen(key);
  for (int i = 0; i < pad; i++) Serial.print(' ');
  Serial.println(value);
}

// ---------- Captive log ----------

void showCaptiveLog() {
  printSection("captive log (ultimos 5)");

  if (!SD.begin(PIN_CD)) {
    Serial.println(F("  SD no disponible"));
    return;
  }
  if (!SD.exists("/captive_log.txt")) {
    Serial.println(F("  sin datos: el portal no ha capturado nada"));
    return;
  }

  File logFile = SD.open("/captive_log.txt", FILE_READ);
  if (!logFile) {
    Serial.println(F("  no se pudo abrir el archivo"));
    return;
  }

  logFile.readStringUntil('\n');
  logFile.readStringUntil('\n'); // ignora el header xd

  Serial.println(F("  time   correo                  ip"));

  int count = 0;
  while (logFile.available() && count < 5) {
    String line = logFile.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int comma1 = line.indexOf(',');
      int comma2 = line.indexOf(',', comma1 + 1);
      int comma3 = line.indexOf(',', comma2 + 1);

      if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
        String timestamp = line.substring(0, comma1);
        String correo = line.substring(comma1 + 1, comma2);
        String ip = line.substring(comma3 + 1);

        unsigned long segundos = timestamp.toInt();
        int mins = segundos / 60;
        int secs = segundos % 60;

        Serial.printf("  %02d:%02d  %-22s  %s\n", mins, secs, correo.c_str(), ip.c_str());
        count++;
      }
    }
  }

  if (count == 0) Serial.println(F("  sin registros"));
  logFile.close();
}

void showFullCaptiveLog() {
  printSection("captive log (completo)");

  if (!SD.begin(PIN_CD) || !SD.exists("/captive_log.txt")) {
    Serial.println(F("  sin datos"));
    return;
  }

  File logFile = SD.open("/captive_log.txt", FILE_READ);
  if (logFile) {
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        Serial.print(F("  "));
        Serial.println(line);
      }
    }
    logFile.close();
  }
}

void clearCaptiveLog() {
  printSection("borrar captive log");

  if (!SD.begin(PIN_CD)) {
    Serial.println(F("  SD no disponible"));
    return;
  }

  if (SD.exists("/captive_log.txt")) {
    if (SD.exists("/captive_log_old.txt")) {
      SD.remove("/captive_log_old.txt");
    }
    SD.rename("/captive_log.txt", "/captive_log_old.txt");
    Serial.println(F("  backup: captive_log_old.txt"));
  }

  File logFile = SD.open("/captive_log.txt", FILE_WRITE);
  if (logFile) {
    logFile.println("=== CAPTIVE PORTAL LOG ===");
    logFile.println("Timestamp,Correo,Telefono,IP");
    logFile.close();
    Serial.println(F("  ok: log borrado y reiniciado"));
  } else {
    Serial.println(F("  error: no se pudo crear el log"));
  }
}

void showCaptiveStats() {
  printSection("estadisticas captive");

  if (!SD.begin(PIN_CD) || !SD.exists("/captive_log.txt")) {
    Serial.println(F("  sin datos"));
    return;
  }

  File logFile = SD.open("/captive_log.txt", FILE_READ);
  if (logFile) {
    logFile.readStringUntil('\n');
    logFile.readStringUntil('\n');

    int total = 0;
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) total++;
    }
    logFile.close();

    printRow("total", String(total));

    logFile = SD.open("/captive_log.txt", FILE_READ);
    logFile.readStringUntil('\n');
    logFile.readStringUntil('\n');

    String lastLine;
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) lastLine = line;
    }
    logFile.close();

    if (lastLine.length() > 0) {
      int comma1 = lastLine.indexOf(',');
      int comma2 = lastLine.indexOf(',', comma1 + 1);
      int comma3 = lastLine.indexOf(',', comma2 + 1);

      if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
        String correo = lastLine.substring(comma1 + 1, comma2);
        String telefono = lastLine.substring(comma2 + 1, comma3);
        printRow("ult. correo", correo.substring(0, 24));
        printRow("ult. tel", telefono);
      }
    }
  }
}

// ---------- SD por serial ----------

// Lista el contenido completo
static void listSD() {
  printSection("sd: contenido de /");

  if (!SD.begin(PIN_CD)) {
    Serial.println(F("  SD no disponible"));
    return;
  }

  File root = SD.open("/");
  if (!root) {
    Serial.println(F("  no se pudo abrir /"));
    return;
  }

  int n = 0;
  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      Serial.printf("  [dir]  %s\n", entry.name());
    } else {
      Serial.printf("  %8lu  %s\n", (unsigned long)entry.size(), entry.name());
    }
    entry.close();
    entry = root.openNextFile();
    n++;
  }
  root.close();

  if (n == 0) Serial.println(F("  (vacio)"));
}

// Vuelca un archivo de la SD por serial
static void catFile(String path) {
  path.trim();
  if (path.length() == 0) {
    Serial.println(F("  uso: cat <archivo>"));
    return;
  }
  if (!path.startsWith("/")) path = "/" + path;

  printSection("sd: cat");
  Serial.print(F("  archivo: "));
  Serial.println(path);

  if (!SD.begin(PIN_CD)) {
    Serial.println(F("  SD no disponible"));
    return;
  }
  if (!SD.exists(path)) {
    Serial.println(F("  no existe"));
    return;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.println(F("  no se pudo abrir"));
    return;
  }

  Serial.println(F("  ---- inicio ----"));
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println();
  Serial.println(F("  ---- fin ----"));
}

// ---------- Modo piano ----------

static int noteFreq(String n) {
  n.trim();
  n.toUpperCase();
  if (n == "DO")  return 262;
  if (n == "RE")  return 294;
  if (n == "MI")  return 330;
  if (n == "FA")  return 349;
  if (n == "SOL") return 392;
  if (n == "LA")  return 440;
  if (n == "SI")  return 494;
  if (n == "DO2" || n == "DO+") return 523; // do de la octava superior
  if (n == "-" || n == "_")     return 0;   // mudaaaa
  return -1;
}

static void printPianoHelp() {
  printSection("modo piano");
  Serial.println(F("  toca escribiendo notas separadas por espacio."));
  Serial.println(F("  ej:  do re mi fa sol la si do2"));
  Serial.println(F("  notas: DO RE MI FA SOL LA SI DO2 (do agudo)"));
  Serial.println(F("  '-' = silencio   |   exit = salir del piano"));
}

static void processPiano(String in) {
  in.trim();

  if (in == "exit" || in == "salir") {
    pianoMode = false;
    Serial.println(F("  saliendo del modo piano"));
    return;
  }
  if (in == "help" || in == "?") {
    printPianoHelp();
    return;
  }

  int start = 0;
  bool played = false;
  while (start < (int)in.length()) {
    int sp = in.indexOf(' ', start);
    if (sp < 0) sp = in.length();
    String tok = in.substring(start, sp);
    tok.trim();
    if (tok.length() > 0) {
      int f = noteFreq(tok);
      if (f < 0) {
        Serial.print(F("  nota desconocida: "));
        Serial.println(tok);
      } else {
        buzzerNote(f, 300);
        played = true;
      }
    }
    start = sp + 1;
  }

  if (!played) Serial.println(F("  (nada que tocar) escribe 'help'"));
}

// ---------- AP Flood parametrico ----------

// Parsea  flood{mensaje1,mensaje2,...}  y actualiza los SSID que
// transmite el AP Flood. Hasta APFLOOD_MAX_MSGS mensajes, cada uno
// recortado a APFLOOD_MAX_SSIDLEN bytes (limite real de un SSID).
static void processFloodCommand(const String& cmd) {
  int open  = cmd.indexOf('{');
  int close = cmd.lastIndexOf('}');
  if (open < 0 || close < 0 || close <= open) {
    Serial.println(F("  uso: flood{mensaje1,mensaje2,...}"));
    return;
  }

  String inner = cmd.substring(open + 1, close);

  String msgs[APFLOOD_MAX_MSGS];
  int  count    = 0;
  bool overflow = false;
  int  start    = 0;

  while (start <= (int)inner.length()) {
    int comma = inner.indexOf(',', start);
    if (comma < 0) comma = inner.length();

    String tok = inner.substring(start, comma);
    tok.trim();

    if (tok.length() > 0) {
      if (count < APFLOOD_MAX_MSGS) {
        bool trimmed = tok.length() > APFLOOD_MAX_SSIDLEN;
        if (trimmed) tok = tok.substring(0, APFLOOD_MAX_SSIDLEN);
        msgs[count++] = tok;
        if (trimmed) {
          Serial.print(F("  aviso: recortado a 32 bytes -> "));
          Serial.println(tok);
        }
      } else {
        overflow = true;
      }
    }
    start = comma + 1;
  }

  if (count == 0) {
    Serial.println(F("  no se encontraron mensajes validos"));
    return;
  }

  const char* ptrs[APFLOOD_MAX_MSGS];
  for (int i = 0; i < count; i++) ptrs[i] = msgs[i].c_str();

  int applied = apFloodSetMessages(ptrs, count);

  Serial.print(F("  ok: AP Flood actualizado con "));
  Serial.print(applied);
  Serial.println(F(" mensajes"));

  if (overflow) {
    Serial.print(F("  aviso: se ignoraron mensajes extra (max "));
    Serial.print(APFLOOD_MAX_MSGS);
    Serial.println(F(")"));
  }
}

// ---------- Procesamiento de comandos ----------

void processSerialCommand() {
  static String commandBuffer = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        commandBuffer.trim();
        Serial.println();

        if (pianoMode) {
          processPiano(commandBuffer);
        }
        else if (commandBuffer == "help") {
          printSection("comandos");
          Serial.println(F("  help       esta ayuda"));
          Serial.println(F("  status     estado del sistema"));
          Serial.println(F("  info       informacion del device"));
          Serial.println(F("  rgb/R/G/B  control de NeoPixels"));
          Serial.println(F("  buzzer     prueba del buzzer"));
          Serial.println(F("  piano      mini piano por serial"));
          Serial.println(F("  ls         listar archivos de la SD"));
          Serial.println(F("  cat <arch> ver un archivo de la SD"));
          Serial.println(F("  flood{...} SSIDs del AP Flood (max 6)"));
          Serial.println(F("  logcap     ultimos 5 logs del portal"));
          Serial.println(F("  logfull    todos los logs"));
          Serial.println(F("  logclear   borrar log"));
          Serial.println(F("  logstats   estadisticas"));
          Serial.println(F("  clear      limpiar pantalla"));
          Serial.println(F("  exit       salir del PC-Mode"));
        }
        else if (commandBuffer == "status") {
          printSection("estado del sistema");
          char rgb[16];
          snprintf(rgb, sizeof(rgb), "%d, %d, %d", neoR, neoG, neoB);
          printRow("rgb", String(rgb));
          unsigned long s = millis() / 1000;
          char up[24];
          snprintf(up, sizeof(up), "%luh %02lum %02lus", s/3600, (s%3600)/60, s%60);
          printRow("uptime", String(up));
          printRow("neopixels", String(NUM_PIXELS));
        }
        else if (commandBuffer == "info") {
          printSection("device info");
          printRow("proyecto", "Camioneta");
          printRow("version", "1.0");
          printRow("autor", "Pablo Lopez");
          printRow("lab", "Tesla Lab");
          printRow("neopixels", String(NUM_PIXELS));
          printRow("display", "SH1106 128x64");
          printRow("mcu", "ESP32");
        }
        else if (commandBuffer.startsWith("rgb/")) {
          int r, g, b;
          if (sscanf(commandBuffer.c_str(), "rgb/%d/%d/%d", &r, &g, &b) == 3) {
            r = constrain(r, 0, 255);
            g = constrain(g, 0, 255);
            b = constrain(b, 0, 255);
            neoR = r; neoG = g; neoB = b;
            for (int i = 0; i < NUM_PIXELS; i++) neopixelSetPixel(i, r, g, b);
            neopixelShow();
            Serial.printf("  ok: rgb -> %d,%d,%d\n", r, g, b);
          } else {
            Serial.println(F("  error: usa rgb/R/G/B"));
          }
        }
        else if (commandBuffer == "buzzer") {
          Serial.println(F("  probando buzzer..."));
          buzzerBeep();
          delay(750);
          buzzerBeep();
          Serial.println(F("  ok: test completado"));
        }
        else if (commandBuffer == "piano") {
          pianoMode = true;
          printPianoHelp();
        }
        else if (commandBuffer == "ls" || commandBuffer == "dir") {
          listSD();
        }
        else if (commandBuffer.startsWith("cat ")) {
          catFile(commandBuffer.substring(4));
        }
        else if (commandBuffer.startsWith("flood{") && commandBuffer.endsWith("}")) {
          processFloodCommand(commandBuffer);
        }
        else if (commandBuffer == "logcap")   { showCaptiveLog(); }
        else if (commandBuffer == "logfull")  { showFullCaptiveLog(); }
        else if (commandBuffer == "logclear") { clearCaptiveLog(); }
        else if (commandBuffer == "logstats") { showCaptiveStats(); }
        else if (commandBuffer == "clear" || commandBuffer == "cls") {
          Serial.print(F("\033[2J\033[H"));
        }
        else if (commandBuffer == "exit") {
          Serial.println(F("  saliendo de PC-Mode..."));
          pcModeInitialized = false;
          pianoMode = false;
          currentScreen = SCREEN_APPS;
          return;
        }
        else if (commandBuffer == "Tesla") {
          Serial.println();
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
        else if (commandBuffer == "Goth") {
          Serial.println();
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
        else {
          Serial.print(F("  command not found: "));
          Serial.println(commandBuffer);
          Serial.println(F("  escribe 'help' para ver los comandos"));
        }

        commandBuffer = "";
        Serial.println();
        printPrompt();
      }
    }
    else if (c == 8 || c == 127) {
      if (commandBuffer.length() > 0) {
        commandBuffer.remove(commandBuffer.length() - 1);
        Serial.write(8);
        Serial.write(' ');
        Serial.write(8);
      }
    }
    else if (c >= 32 && c <= 126) {
      commandBuffer += c;
      Serial.write(c);
    }
  }
}


static void printBanner() {
  //no sirve xd Serial.print(F("\033[2J\033[H")); // limpia pantalla
  Serial.println();
  Serial.println(F("                      ***************++"));
  Serial.println(F("                  ***+=::...... ....::-++**"));
  Serial.println(F("               **+-:......................-+**"));
  Serial.println(F("             **-......:......................-**"));
  Serial.println(F("          +*+-.::::::::::::-+++**=-:...........-+*+"));
  Serial.println(F("         *+=:::::::=****###%%%%%%%%#*=-:::.......-*+"));
  Serial.println(F("        *+-::::::=###++*#@@@%%%%%%%%@%%#=::::::::::+*"));
  Serial.println(F("      =*+:::::::=*##+++*#%@@@@@@%%%%%%%%#=::::::::::+*"));
  Serial.println(F("      *+--------=*%%+===*#%@@@@@@@@@@@@%%#+-:::::::::+*"));
  Serial.println(F("     ++-------::+%%@*=++=-..:----*%#==++++*+---::::::-++"));
  Serial.println(F("    =+=-------:-+##%*=-=+*=:::::.-##-..:-++*=------:--=+="));
  Serial.println(F("    ++--------::-*=+*=:---=#%%#+++*#%%##*=+**----------++"));
  Serial.println(F("    +=-------:.:=#@#+=++**#%%%#**==-+**%%**%*----------=+"));
  Serial.println(F("   =+=-=----=-.:=#%#==++*#@@@@%%=:....:%@%%%#=---------=+"));
  Serial.println(F("   =+=-======-:-*%#=:::==++*#%@#*#####@%@@@@#+---------=+-"));
  Serial.println(F("   -+=========-:+*++=-::=+**#%%###*=-:=#@@@%**====-----=+"));
  Serial.println(F("    ++=======-.::-=******%%#*#@@*=*#*+=-*@@@+==========++"));
  Serial.println(F("    =+========-:..-**##***#%%%@%#####%%#+%%@%+=========+="));
  Serial.println(F("    :++=++++++:...:-=+#*+=*#####@@@@@@@@@@%%%%+=======++:"));
  Serial.println(F("     =++++++++=:.:.:=-+++=*%@%@@@@@@@@@@@@@@@%#+======+="));
  Serial.println(F("     -=+=+++++++=-..:-:-+=-#%#%%%@@%@@@%#%@@@@@%=====+=:"));
  Serial.println(F("      :=+=+++++====..:::.:-++*@%%#@@@@@@@#*@@@@@+===+=:"));
  Serial.println(F("        =+=========-:......--*#@@%%@%@@%%%%%%@@%+==+="));
  Serial.println(F("         -==--::::--:.... ..:-=+==+=+*%+##*###%#+++-"));
  Serial.println(F("          :==:.......  .. .  ..::-:::::-=-=-::-++=-"));
  Serial.println(F("            :-=-.           ..          .....-+-."));
  Serial.println(F("              :-==:...     .    ...     ..-==-:"));
  Serial.println(F("                 :-===-:....     ....:-===-:"));
  Serial.println(F("                     :---===========---:"));
  Serial.println();
  Serial.println(F(" ____    ______  __     ______   __       ____    _____"));
  Serial.println(F("/\\  _`\\ /\\  _  \\/\\ \\   /\\__  _\\ /\\ \\     /\\  _`\\ /\\  __`\\"));
  Serial.println(F("\\ \\ \\L\\_\\ \\ \\L\\ \\ \\ \\  \\/_/\\ \\/ \\ \\ \\    \\ \\ \\L\\_\\ \\ \\/\\ \\"));
  Serial.println(F(" \\ \\ \\L_L\\ \\  __ \\ \\ \\  __\\ \\ \\  \\ \\ \\  __\\ \\  _\\L\\ \\ \\ \\ \\"));
  Serial.println(F("  \\ \\ \\/, \\ \\ \\/\\ \\ \\ \\L\\ \\\\_\\ \\__\\ \\ \\L\\ \\\\ \\ \\L\\ \\ \\ \\_\\ \\"));
  Serial.println(F("   \\ \\____/\\ \\_\\ \\_\\ \\____//\\_____\\\\ \\____/ \\ \\____/\\ \\_____\\"));
  Serial.println(F("    \\/___/  \\/_/\\/_/\\/___/ \\/_____/ \\/___/   \\/___/  \\/_____/"));
  Serial.println();
  Serial.println(F(" _________  _______   ________  ___       ________"));
  Serial.println(F("|\\___   ___\\\\  ___ \\ |\\   ____\\|\\  \\     |\\   __  \\"));
  Serial.println(F("\\|___ \\  \\_\\ \\   __/|\\ \\  \\___|\\ \\  \\    \\ \\  \\|\\  \\"));
  Serial.println(F("     \\ \\  \\ \\ \\  \\_|/_\\ \\_____  \\ \\  \\    \\ \\   __  \\"));
  Serial.println(F("      \\ \\  \\ \\ \\  \\_|\\ \\|____|\\  \\ \\  \\____\\ \\  \\ \\  \\"));
  Serial.println(F("       \\ \\__\\ \\ \\_______\\____\\_\\  \\ \\_______\\ \\__\\ \\__\\"));
  Serial.println(F("        \\|__|  \\|_______|\\_________\\|_______|\\|__|\\|__|"));
  Serial.println(F("                        \\|_________|"));
  Serial.println(F(" ___       ________  ________"));
  Serial.println(F("|\\  \\     |\\   __  \\|\\   __  \\"));
  Serial.println(F("\\ \\  \\    \\ \\  \\|\\  \\ \\  \\|\\ /_"));
  Serial.println(F(" \\ \\  \\    \\ \\   __  \\ \\   __  \\"));
  Serial.println(F("  \\ \\  \\____\\ \\  \\ \\  \\ \\  \\|\\  \\"));
  Serial.println(F("   \\ \\_______\\ \\__\\ \\__\\ \\_______\\"));
  Serial.println(F("    \\|_______|\\|__|\\|__|\\|_______|"));
  Serial.println();
  Serial.println(F("Version 1.0  ::  PC-MODE / Serial Mode"));
  Serial.println();
}

void screenPCModeLoop() {
  if (!pcModeInitialized) {
    printBanner();
    printPrompt();
    pcModeInitialized = true;
  }

  processSerialCommand();

  if (isButtonJustPressed(PIN_BACK)) {
    buzzerClick();
    pcModeInitialized = false;
    pianoMode = false;
    Serial.println();
    Serial.println(F("  saliendo de PC-Mode..."));
    currentScreen = SCREEN_APPS;
    return;
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setDrawColor(1);
  u8g2.drawBox(16, 1, 96, 14);
  u8g2.setDrawColor(2);
  u8g2.drawStr(35, 11, "PC-MODE");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(0, 1, 16, 14, image_Layer_9_bits);
  u8g2.drawXBM(112, 1, 16, 14, image_Layer_9_bits);

  u8g2.drawXBM(48, 24, 32, 16, image_terminal_bits);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(25, 48, "Terminal Activa");
  u8g2.drawStr(18, 58, "Ver Monitor Serial");

  u8g2.sendBuffer();
}

static void printPrompt() {
  if (pianoMode) {
    Serial.print(F("piano> "));
  } else {
    Serial.print(F("camioneta:~$ "));
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
