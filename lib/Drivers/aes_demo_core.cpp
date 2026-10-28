#include "aes_demo_core.h"
#include <Arduino.h>
#include <mbedtls/aes.h>
#include <string.h>

const char *const AES_DEMO_MESSAGES[] = {
  "HOLA TESLA LAB",
  "CAMIONETA ESP32",
  "ZENITH CORE 2026",
  "NO ME DESCIFRES",
};
const int AES_DEMO_MESSAGE_COUNT = sizeof(AES_DEMO_MESSAGES) / sizeof(AES_DEMO_MESSAGES[0]);

const uint8_t AES_DEMO_KEYS[][AES_BLOCK_LEN] = {
  {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C},
  {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F},
};
const int AES_DEMO_KEY_COUNT = sizeof(AES_DEMO_KEYS) / AES_BLOCK_LEN;

void aesDemoPackMessage(const char *msg, uint8_t out[AES_BLOCK_LEN]) {
  memset(out, ' ', AES_BLOCK_LEN);
  size_t n = strlen(msg);
  if (n > AES_BLOCK_LEN) n = AES_BLOCK_LEN;
  memcpy(out, msg, n);
}

void aesDemoEncrypt(const uint8_t key[AES_BLOCK_LEN], const uint8_t plain[AES_BLOCK_LEN], uint8_t cipher[AES_BLOCK_LEN]) {
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, key, 128);
  mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, plain, cipher);
  mbedtls_aes_free(&ctx);
}

void aesDemoDecrypt(const uint8_t key[AES_BLOCK_LEN], const uint8_t cipher[AES_BLOCK_LEN], uint8_t plain[AES_BLOCK_LEN]) {
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_dec(&ctx, key, 128);
  mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, cipher, plain);
  mbedtls_aes_free(&ctx);
}

void aesDemoFlipKeyBits(const uint8_t key[AES_BLOCK_LEN], int bitCount, uint8_t out[AES_BLOCK_LEN]) {
  memcpy(out, key, AES_BLOCK_LEN);
  for (int i = 0; i < bitCount; i++) {
    int bit = random(0, AES_BLOCK_LEN * 8);
    out[bit / 8] ^= (uint8_t)(1 << (bit % 8));
  }
}

void aesDemoRandomKey(uint8_t out[AES_BLOCK_LEN]) {
  for (int i = 0; i < AES_BLOCK_LEN; i++) out[i] = (uint8_t)random(0, 256);
}

void aesDemoFlipMessageBit(const uint8_t msg[AES_BLOCK_LEN], int bytePos, uint8_t out[AES_BLOCK_LEN]) {
  memcpy(out, msg, AES_BLOCK_LEN);
  if (bytePos >= 0 && bytePos < AES_BLOCK_LEN) out[bytePos] ^= 0x01;
}

int aesDemoHammingDistance(const uint8_t a[AES_BLOCK_LEN], const uint8_t b[AES_BLOCK_LEN]) {
  int d = 0;
  for (int i = 0; i < AES_BLOCK_LEN; i++) {
    uint8_t x = a[i] ^ b[i];
    while (x) { d += (x & 1); x >>= 1; }
  }
  return d;
}

bool aesDemoLooksLikeText(const uint8_t block[AES_BLOCK_LEN]) {
  for (int i = 0; i < AES_BLOCK_LEN; i++) {
    uint8_t c = block[i];
    if (c < 32 || c > 126) return false;
  }
  return true;
}

static char    currentMsgText[AES_BLOCK_LEN + 1] = "HOLA TESLA LAB";
static uint8_t currentKey[AES_BLOCK_LEN] = {
  0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C
};

void aesDemoSetMessageText(const char *msg) {
  size_t n = strlen(msg);
  if (n > AES_BLOCK_LEN) n = AES_BLOCK_LEN;
  memcpy(currentMsgText, msg, n);
  currentMsgText[n] = 0;
}
const char *aesDemoGetMessageText() { return currentMsgText; }
void aesDemoGetMessageBlock(uint8_t out[AES_BLOCK_LEN]) { aesDemoPackMessage(currentMsgText, out); }

void aesDemoSetKeyPreset(int idx) {
  if (idx < 0 || idx >= AES_DEMO_KEY_COUNT) return;
  memcpy(currentKey, AES_DEMO_KEYS[idx], AES_BLOCK_LEN);
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool aesDemoSetKeyHex(const char *hex) {
  if (strlen(hex) != AES_BLOCK_LEN * 2) return false;
  uint8_t tmp[AES_BLOCK_LEN];
  for (int i = 0; i < AES_BLOCK_LEN; i++) {
    int hi = hexNibble(hex[i * 2]);
    int lo = hexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    tmp[i] = (uint8_t)((hi << 4) | lo);
  }
  memcpy(currentKey, tmp, AES_BLOCK_LEN);
  return true;
}

void aesDemoGetKey(uint8_t out[AES_BLOCK_LEN]) { memcpy(out, currentKey, AES_BLOCK_LEN); }
