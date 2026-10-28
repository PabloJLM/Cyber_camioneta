#pragma once
#include <stdint.h>

#define AES_BLOCK_LEN 16

extern const char *const AES_DEMO_MESSAGES[];
extern const int         AES_DEMO_MESSAGE_COUNT;

extern const uint8_t AES_DEMO_KEYS[][AES_BLOCK_LEN];
extern const int     AES_DEMO_KEY_COUNT;

void aesDemoPackMessage(const char *msg, uint8_t out[AES_BLOCK_LEN]);

void aesDemoEncrypt(const uint8_t key[AES_BLOCK_LEN], const uint8_t plain[AES_BLOCK_LEN], uint8_t cipher[AES_BLOCK_LEN]);
void aesDemoDecrypt(const uint8_t key[AES_BLOCK_LEN], const uint8_t cipher[AES_BLOCK_LEN], uint8_t plain[AES_BLOCK_LEN]);

void aesDemoFlipKeyBits(const uint8_t key[AES_BLOCK_LEN], int bitCount, uint8_t out[AES_BLOCK_LEN]);
void aesDemoRandomKey(uint8_t out[AES_BLOCK_LEN]);

void aesDemoFlipMessageBit(const uint8_t msg[AES_BLOCK_LEN], int bytePos, uint8_t out[AES_BLOCK_LEN]);

int aesDemoHammingDistance(const uint8_t a[AES_BLOCK_LEN], const uint8_t b[AES_BLOCK_LEN]);

bool aesDemoLooksLikeText(const uint8_t block[AES_BLOCK_LEN]);

void        aesDemoSetMessageText(const char *msg);
const char *aesDemoGetMessageText();
void        aesDemoGetMessageBlock(uint8_t out[AES_BLOCK_LEN]);

void aesDemoSetKeyPreset(int idx);
bool aesDemoSetKeyHex(const char *hex32);
void aesDemoGetKey(uint8_t out[AES_BLOCK_LEN]);
