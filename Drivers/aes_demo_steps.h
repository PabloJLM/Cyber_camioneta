#pragma once
#include <stdint.h>

// Implementacion "de libro" (software, sin el acelerador de hardware) de
// AES-128, expuesta paso a paso solo con fines didacticos para la terminal
// ("aes steps" / "aes keysteps"). El cifrado/descifrado real que usa el
// acelerador de hardware del C6 sigue en Drivers/aes_demo_core.h.
//
// Convencion de indices del "estado" de 16 bytes: state[columna*4 + fila],
// igual que el grid de la especificacion (orden columna-mayor):
//   [ b0 | b4 | b8  | b12 ]
//   [ b1 | b5 | b9  | b13 ]
//   [ b2 | b6 | b10 | b14 ]
//   [ b3 | b7 | b11 | b15 ]

extern const uint8_t AES_STEPS_SBOX[256];
extern const uint8_t AES_STEPS_RCON[11];   // RCON[0] no se usa, RCON[1..10] si

// Expande la clave de 16 bytes en 44 "words" de 4 bytes (= 11 round keys
// de 16 bytes cada una, contiguas: words[4*r .. 4*r+3] = round key r).
void aesStepsKeyExpansion(const uint8_t key[16], uint8_t words[44][4]);

// Las 4 transformaciones de una ronda de cifrado, todas sobre el estado
// de 16 bytes (se modifica in-place).
void aesStepsSubBytes(uint8_t state[16]);
void aesStepsShiftRows(uint8_t state[16]);
void aesStepsMixColumns(uint8_t state[16]);
void aesStepsAddRoundKey(uint8_t state[16], const uint8_t roundKey[16]);

// Utilidades del key schedule, usadas tanto internamente como por la
// traza de "aes keysteps" para mostrar el calculo real paso a paso.
void aesStepsRotWord(uint8_t w[4]);
void aesStepsSubWord(uint8_t w[4]);

// Multiplicacion en GF(2^8), el cuerpo finito que usa MixColumns.
uint8_t aesStepsGMul(uint8_t a, uint8_t b);
