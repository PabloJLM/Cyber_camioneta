#include "Drivers/aes_demo_terminal.h"
#include "Drivers/aes_demo_core.h"
#include <string.h>

static void hexLine(Print &c, const uint8_t *data, int n) {
  for (int i = 0; i < n; i++) {
    if (data[i] < 16) c.print('0');
    c.print(data[i], HEX);
    if (i < n - 1) c.print(' ');
  }
  c.println();
}

static void printHelp(Print &c) {
  c.println();
  c.println(F("   == comandos AES (Advanced Encryption Standard) =="));
  c.println(F("   aes msg <texto>     mensaje a cifrar (hasta 16 caracteres)"));
  c.println(F("   aes key <0|1|hex>   clave: preset 0/1, o 32 hex (16 bytes)"));
  c.println(F("   aes show            mensaje y clave actuales"));
  c.println(F("   aes run             cifra y descifra el mensaje actual, paso a paso"));
  c.println(F("   aes attempts        intentos con claves casi correctas (todas fallan)"));
  c.println(F("   aes avalanche       mismo mensaje, 1 bit distinto -> cifrado totalmente distinto"));
  c.println(F("   aes help            esta ayuda"));
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
  c.println(F("   --- AES-128 ECB, acelerador de hardware del C6 ---"));
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
    c.print(F("     ¿texto?     "));
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
  } else if (rest == "attempts") {
    printAttempts(c);
  } else if (rest == "avalanche") {
    printAvalanche(c);
  } else {
    c.println(F("   uso: aes msg|key|show|run|attempts|avalanche   (aes help para detalle)"));
  }
  return true;
}
