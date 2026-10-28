#pragma once
#include <stdint.h>

extern const uint8_t AES_STEPS_SBOX[256];
extern const uint8_t AES_STEPS_RCON[11];

void aesStepsKeyExpansion(const uint8_t key[16], uint8_t words[44][4]);

void aesStepsSubBytes(uint8_t state[16]);
void aesStepsShiftRows(uint8_t state[16]);
void aesStepsMixColumns(uint8_t state[16]);
void aesStepsAddRoundKey(uint8_t state[16], const uint8_t roundKey[16]);

void aesStepsRotWord(uint8_t w[4]);
void aesStepsSubWord(uint8_t w[4]);

uint8_t aesStepsGMul(uint8_t a, uint8_t b);
