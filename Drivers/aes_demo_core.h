#pragma once
#include <stdint.h>

// Nucleo de la demo de AES: AES-128 ECB real con el acelerador por hardware
// del C6 (via mbedtls). Un solo bloque de 16 bytes por mensaje/clave, para
// mantener la demo simple (sin modos de cifrado de verdad ni relleno PKCS).

#define AES_BLOCK_LEN 16

extern const char *const AES_DEMO_MESSAGES[];
extern const int         AES_DEMO_MESSAGE_COUNT;

// Claves de ejemplo AES-128 (16 bytes). La [0] es la clave de prueba del
// estandar FIPS-197, la [1] es una clave "didactica" 00..0F.
extern const uint8_t AES_DEMO_KEYS[][AES_BLOCK_LEN];
extern const int     AES_DEMO_KEY_COUNT;

// Arma el bloque de 16 bytes de un mensaje (rellena con espacios, trunca si
// es mas largo de 16 caracteres).
void aesDemoPackMessage(const char *msg, uint8_t out[AES_BLOCK_LEN]);

// AES-128 ECB real (acelerador de hardware del C6, via mbedtls).
void aesDemoEncrypt(const uint8_t key[AES_BLOCK_LEN], const uint8_t plain[AES_BLOCK_LEN], uint8_t cipher[AES_BLOCK_LEN]);
void aesDemoDecrypt(const uint8_t key[AES_BLOCK_LEN], const uint8_t cipher[AES_BLOCK_LEN], uint8_t plain[AES_BLOCK_LEN]);

// Clave con 'bitCount' bits distintos a 'key', en posiciones al azar (para
// la lista de intentos fallidos: "casi" la clave correcta no alcanza).
void aesDemoFlipKeyBits(const uint8_t key[AES_BLOCK_LEN], int bitCount, uint8_t out[AES_BLOCK_LEN]);
void aesDemoRandomKey(uint8_t out[AES_BLOCK_LEN]);

// Mensaje con 1 bit cambiado en la posicion dada (para el efecto avalancha).
void aesDemoFlipMessageBit(const uint8_t msg[AES_BLOCK_LEN], int bytePos, uint8_t out[AES_BLOCK_LEN]);

// Cantidad de bits distintos entre dos bloques de 16 bytes (de 128 posibles).
int aesDemoHammingDistance(const uint8_t a[AES_BLOCK_LEN], const uint8_t b[AES_BLOCK_LEN]);

// Heuristica: ¿el bloque se ve como texto (todo imprimible)? Para distinguir
// "mensaje recuperado" de "basura" en los intentos fallidos.
bool aesDemoLooksLikeText(const uint8_t block[AES_BLOCK_LEN]);

// Estado activo (mensaje y clave), compartido entre las terminales
// (serie y TCP). La pantalla OLED es solo simbolica y no lo usa.
void        aesDemoSetMessageText(const char *msg);                 // hasta 16 caracteres
const char *aesDemoGetMessageText();
void        aesDemoGetMessageBlock(uint8_t out[AES_BLOCK_LEN]);     // version de 16 bytes, rellenada

void aesDemoSetKeyPreset(int idx);                 // 0..AES_DEMO_KEY_COUNT-1
bool aesDemoSetKeyHex(const char *hex32);          // 32 caracteres hex -> 16 bytes; false si invalido
void aesDemoGetKey(uint8_t out[AES_BLOCK_LEN]);
