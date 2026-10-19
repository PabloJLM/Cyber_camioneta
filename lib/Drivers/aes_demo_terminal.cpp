#include "aes_demo_terminal.h"
#include "aes_demo_core.h"
#include "aes_demo_steps.h"
#include <string.h>

static void hexLine(Print &c, const uint8_t *data, int n) {
  for (int i = 0; i < n; i++) {
    if (data[i] < 16) c.print('0');
    c.print(data[i], HEX);
    if (i < n - 1) c.print(' ');
  }
  c.println();
}

// Igual que hexLine pero devuelve el texto "legible" de un bloque:
// los bytes imprimibles se muestran tal cual, el resto como '.'
// (sirve para mostrar que un descifrado con clave incorrecta da basura).
static void legibleLine(Print &c, const uint8_t *data, int n) {
  char t[17];
  int m = n > 16 ? 16 : n;
  for (int i = 0; i < m; i++) {
    uint8_t b = data[i];
    t[i] = (b >= 32 && b <= 126) ? (char)b : '.';
  }
  t[m] = 0;
  c.print('"'); c.print(t); c.print('"');
}

// Imprime el estado de 16 bytes como grid 4x4 (orden columna-mayor, igual
// que la especificacion: fila = indice%4, columna = indice/4).
static void printGrid(Print &c, const uint8_t state[16]) {
  for (int r = 0; r < 4; r++) {
    c.print(F("     "));
    for (int col = 0; col < 4; col++) {
      uint8_t b = state[col * 4 + r];
      if (b < 16) c.print('0');
      c.print(b, HEX);
      if (col < 3) c.print(' ');
    }
    c.println();
  }
}

static void printHelp(Print &c) {
  c.println();
  c.println(F("   == comandos AES (Advanced Encryption Standard) =="));
  c.println(F("   aes msg <texto>     mensaje a cifrar (hasta 16 caracteres)"));
  c.println(F("   aes key <0|1|hex>   clave: preset 0/1, o 32 hex (16 bytes)"));
  c.println(F("   aes show            mensaje y clave actuales"));
  c.println(F("   aes run             cifra y descifra el mensaje actual"));
  c.println(F("   aes steps           cifrado paso a paso"));
  c.println(F("   aes keysteps        expansion de la clave paso a paso"));
  c.println(F("   aes attempts        intentos con claves casi correctas"));
  c.println(F("   aes avalanche       mismo mensaje, 1 bit distinto"));
  c.println(F("   aes help            ayuda"));
}

static void printShow(Print &c) {
  uint8_t key[AES_BLOCK_LEN];
  aesDemoGetKey(key);
  c.println();
  c.print(F("   mensaje: \"")); c.print(aesDemoGetMessageText()); c.println('"');
  c.print(F("   clave:   ")); hexLine(c, key, AES_BLOCK_LEN);
}

static void printRun(Print &c) {
  uint8_t plain[AES_BLOCK_LEN], key[AES_BLOCK_LEN], cipher[AES_BLOCK_LEN], back[AES_BLOCK_LEN];
  aesDemoGetMessageBlock(plain);
  aesDemoGetKey(key);

  c.println();
  c.println(F("   --- AES-128 ECB usando el core C6 ---"));
  c.print(F("   1. mensaje (16 bytes):  \"")); c.print(aesDemoGetMessageText()); c.println('"');
  c.print(F("   2. clave (128 bits):    ")); hexLine(c, key, AES_BLOCK_LEN);

  aesDemoEncrypt(key, plain, cipher);           // <- AES real
  c.print(F("   3. cifrado:             ")); hexLine(c, cipher, AES_BLOCK_LEN);

  aesDemoDecrypt(key, cipher, back);            // <- AES real
  char t[AES_BLOCK_LEN + 1];
  memcpy(t, back, AES_BLOCK_LEN);
  t[AES_BLOCK_LEN] = 0;
  c.print(F("   4. descifrado:          \"")); c.print(t); c.println('"');
}

static void printSteps(Print &c) {
  uint8_t plain[AES_BLOCK_LEN], key[AES_BLOCK_LEN];
  aesDemoGetMessageBlock(plain);
  aesDemoGetKey(key);

  uint8_t words[44][4];
  aesStepsKeyExpansion(key, words);

  c.println();
  c.println(F("   === AES-128 paso a paso ==="));
  c.print(F("   mensaje: \"")); c.print(aesDemoGetMessageText()); c.println('"');
  c.print(F("   clave:   ")); hexLine(c, key, AES_BLOCK_LEN);
  c.println();
  c.println(F("   estado inicial (grid 4x4, columna-mayor):"));
  uint8_t state[AES_BLOCK_LEN];
  memcpy(state, plain, AES_BLOCK_LEN);
  printGrid(c, state);

  c.println(F("   ronda 0: AddRoundKey (round key 0 = la clave tal cual)"));
  aesStepsAddRoundKey(state, &words[0][0]);
  printGrid(c, state);

  for (int r = 1; r <= 9; r++) {
    c.println();
    c.print(F("   --- ronda ")); c.print(r); c.println(F(" ---"));

    c.println(F("   SubBytes (sustituye cada byte usando la S-box):"));
    aesStepsSubBytes(state);
    printGrid(c, state);

    c.println(F("   ShiftRows (corre cada fila a la izquierda: fila r, r posiciones):"));
    aesStepsShiftRows(state);
    printGrid(c, state);

    c.println(F("   MixColumns (mezcla cada columna via GF(2^8)):"));
    aesStepsMixColumns(state);
    printGrid(c, state);

    c.print(F("   AddRoundKey (round key ")); c.print(r); c.println(F("):"));
    aesStepsAddRoundKey(state, &words[4 * r][0]);
    printGrid(c, state);
  }

  c.println();
  c.println(F("   --- ronda 10 (final, sin MixColumns) ---"));
  c.println(F("   SubBytes:"));
  aesStepsSubBytes(state);
  printGrid(c, state);
  c.println(F("   ShiftRows:"));
  aesStepsShiftRows(state);
  printGrid(c, state);
  c.println(F("   AddRoundKey (round key 10):"));
  aesStepsAddRoundKey(state, &words[40][0]);
  printGrid(c, state);

  c.println();
  c.print(F("   cifrado final: ")); hexLine(c, state, AES_BLOCK_LEN);

  uint8_t realCipher[AES_BLOCK_LEN];
  aesDemoEncrypt(key, plain, realCipher);          // <- AES real (hardware), para verificar
  bool coincide = memcmp(state, realCipher, AES_BLOCK_LEN) == 0;
  c.print(F("   ¿coincide con el cifrado real del hardware C6? "));
  c.println(coincide ? F("si") : F("no (revisa el fw)"));
}

static void printKeySteps(Print &c) {
  uint8_t key[AES_BLOCK_LEN];
  aesDemoGetKey(key);

  uint8_t words[44][4];
  aesStepsKeyExpansion(key, words);

  c.println();
  c.println(F("   === AES-128: expansion de la clave (key schedule) ==="));
  c.print(F("   clave original: ")); hexLine(c, key, AES_BLOCK_LEN);
  c.println(F("   como 4 words (W0..W3):"));
  for (int i = 0; i < 4; i++) {
    c.print(F("     W")); c.print(i); c.print(F(": "));
    hexLine(c, words[i], 4);
  }

  for (int r = 1; r <= 10; r++) {
    c.println();
    c.print(F("   --- round key ")); c.print(r); c.print(F(" (words W")); c.print(4 * r);
    c.print(F("..W")); c.print(4 * r + 3); c.println(F(") ---"));

    uint8_t temp[4];
    memcpy(temp, words[4 * r - 1], 4);
    c.print(F("     temp = W")); c.print(4 * r - 1); c.print(F(" = ")); hexLine(c, temp, 4);

    aesStepsRotWord(temp);
    c.print(F("     RotWord(temp)        = ")); hexLine(c, temp, 4);

    aesStepsSubWord(temp);
    c.print(F("     SubWord(temp)        = ")); hexLine(c, temp, 4);

    temp[0] ^= AES_STEPS_RCON[r];
    c.print(F("     XOR Rcon[")); c.print(r); c.print(F("]=")); 
    if (AES_STEPS_RCON[r] < 16) c.print('0');
    c.print(AES_STEPS_RCON[r], HEX);
    c.print(F("         = ")); hexLine(c, temp, 4);

    for (int k = 0; k < 4; k++) {
      c.print(F("     W")); c.print(4 * r + k); c.print(F(" = "));
      if (k == 0) {
        c.print(F("temp XOR W")); c.print(4 * (r - 1));
      } else {
        c.print(F("W")); c.print(4 * r + k - 1); c.print(F(" XOR W")); c.print(4 * (r - 1) + k);
      }
      c.print(F("  = ")); hexLine(c, words[4 * r + k], 4);
    }

    c.print(F("     round key ")); c.print(r); c.print(F(": "));
    hexLine(c, &words[4 * r][0], AES_BLOCK_LEN);
  }
}

static void printAttempts(Print &c) {
  uint8_t plain[AES_BLOCK_LEN], key[AES_BLOCK_LEN], cipher[AES_BLOCK_LEN];
  aesDemoGetMessageBlock(plain);
  aesDemoGetKey(key);
  aesDemoEncrypt(key, plain, cipher);           // <- AES real

  c.println();
  c.print(F("   mensaje cifrado con la clave correcta: \"")); c.print(aesDemoGetMessageText()); c.println('"');
  c.print(F("   clave correcta: ")); hexLine(c, key, AES_BLOCK_LEN);
  c.print(F("   cifrado:        ")); hexLine(c, cipher, AES_BLOCK_LEN);
  c.println();
  c.println(F("   intentos de descifrar ese mismo cifrado con claves casi correctas:"));

  const int bits[3] = {1, 4, 16};
  for (int i = 0; i < 3; i++) {
    uint8_t wrongKey[AES_BLOCK_LEN], out[AES_BLOCK_LEN];
    aesDemoFlipKeyBits(key, bits[i], wrongKey);
    aesDemoDecrypt(wrongKey, cipher, out);      // <- AES real, con clave incorrecta

    c.print(F("   [")); c.print(bits[i]); c.println(F(" bit(s) distinto(s)]"));
    c.print(F("     clave:      ")); hexLine(c, wrongKey, AES_BLOCK_LEN);
    c.print(F("     resultado:  ")); hexLine(c, out, AES_BLOCK_LEN);
    c.print(F("     como texto: ")); legibleLine(c, out, AES_BLOCK_LEN); c.println();
    c.print(F("     ¿coincide con el mensaje original? "));
    c.println(aesDemoLooksLikeText(out) ? F("si (raro, probá de nuevo)") : F("no"));
  }

  uint8_t recovered[AES_BLOCK_LEN];
  aesDemoDecrypt(key, cipher, recovered);       // <- AES real, con la clave correcta
  char t[AES_BLOCK_LEN + 1];
  memcpy(t, recovered, AES_BLOCK_LEN);
  t[AES_BLOCK_LEN] = 0;
  c.println();
  c.print(F("   [clave correcta] -> \"")); c.print(t); c.println('"');
}

static void printVariant(Print &c, const char *label, const uint8_t cipher[AES_BLOCK_LEN],
                          const uint8_t baseCipher[AES_BLOCK_LEN]) {
  c.print(F("   [")); c.print(label); c.println(F("]"));
  c.print(F("     cifrado:        ")); hexLine(c, cipher, AES_BLOCK_LEN);
  int h = aesDemoHammingDistance(baseCipher, cipher);
  c.print(F("     bits distintos: ")); c.print(h); c.print(F("/128 (")); c.print(h * 100 / 128); c.println(F("%)"));
}

static void printAvalanche(Print &c) {
  uint8_t plain[AES_BLOCK_LEN], key[AES_BLOCK_LEN], baseCipher[AES_BLOCK_LEN];
  aesDemoGetMessageBlock(plain);
  aesDemoGetKey(key);
  aesDemoEncrypt(key, plain, baseCipher);       // <- AES real

  c.println();
  c.print(F("   mensaje base: \"")); c.print(aesDemoGetMessageText()); c.println('"');
  c.print(F("   clave:        ")); hexLine(c, key, AES_BLOCK_LEN);
  c.print(F("   cifrado base: ")); hexLine(c, baseCipher, AES_BLOCK_LEN);
  c.println();

  uint8_t tmpMsg[AES_BLOCK_LEN], tmpKey[AES_BLOCK_LEN], cipher[AES_BLOCK_LEN];

  aesDemoFlipMessageBit(plain, 0, tmpMsg);
  aesDemoEncrypt(key, tmpMsg, cipher);          // <- AES real
  printVariant(c, "mensaje: 1 bit distinto", cipher, baseCipher);

  aesDemoFlipKeyBits(key, 1, tmpKey);
  aesDemoEncrypt(tmpKey, plain, cipher);        // <- AES real
  printVariant(c, "clave: 1 bit distinto", cipher, baseCipher);

  aesDemoRandomKey(tmpKey);
  aesDemoEncrypt(tmpKey, plain, cipher);        // <- AES real
  printVariant(c, "clave: al azar", cipher, baseCipher);

  c.println();
  c.println(F("   un bit de diferencia cambia ~ la mitad del cifrado."));
}

bool aesTerminalHandle(Print &c, const String &cmd) {
  if (cmd == "aes" || cmd == "aes help") { printHelp(c); return true; }
  if (!cmd.startsWith("aes ")) return false;

  String rest = cmd.substring(4);
  rest.trim();

  if (rest.startsWith("msg ")) {
    String msg = rest.substring(4);
    msg.trim();
    if (msg.length() == 0) {
      c.println(F("   uso: aes msg <texto>   (hasta 16 caracteres)"));
    } else {
      aesDemoSetMessageText(msg.c_str());
      c.print(F("   ok: mensaje -> \"")); c.print(aesDemoGetMessageText()); c.println('"');
    }
  } else if (rest.startsWith("key ")) {
    String arg = rest.substring(4);
    arg.trim();
    if (arg.length() == (int)(AES_BLOCK_LEN * 2)) {
      if (aesDemoSetKeyHex(arg.c_str())) {
        uint8_t key[AES_BLOCK_LEN];
        aesDemoGetKey(key);
        c.print(F("   ok: clave -> ")); hexLine(c, key, AES_BLOCK_LEN);
      } else {
        c.println(F("   error: hex invalido (deben ser 32 caracteres 0-9/a-f)"));
      }
    } else if (arg.length() == 1 && arg[0] >= '0' && arg[0] < '0' + AES_DEMO_KEY_COUNT) {
      aesDemoSetKeyPreset(arg[0] - '0');
      uint8_t key[AES_BLOCK_LEN];
      aesDemoGetKey(key);
      c.print(F("   ok: clave -> ")); hexLine(c, key, AES_BLOCK_LEN);
    } else {
      c.println(F("   uso: aes key 0|1   o   aes key <32 caracteres hex>"));
    }
  } else if (rest == "show") {
    printShow(c);
  } else if (rest == "run") {
    printRun(c);
  } else if (rest == "steps") {
    printSteps(c);
  } else if (rest == "keysteps") {
    printKeySteps(c);
  } else if (rest == "attempts") {
    printAttempts(c);
  } else if (rest == "avalanche") {
    printAvalanche(c);
  } else {
    c.println(F("   uso: aes msg|key|show|run|steps|keysteps|attempts|avalanche   (aes help para detalle)"));
  }
  return true;
}
