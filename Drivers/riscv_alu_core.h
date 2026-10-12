#pragma once
#include <stdint.h>

// Estado y ejecucion de la ALU de 8 bits, compartidos entre la pantalla OLED
// (screen_riscvalu.cpp) y las terminales avanzadas (serie y TCP). La
// operacion real la ejecuta riscv_alu.S en RISC-V.

#define ALU_OP_ADD   0
#define ALU_OP_SUB   1
#define ALU_OP_AND   2
#define ALU_OP_OR    3
#define ALU_OP_XOR   4
#define ALU_OP_SLL   5
#define ALU_OP_SRL   6
#define ALU_OP_SLTU  7
#define ALU_OP_COUNT 8

struct AluResult {
  uint8_t result;
  bool carry;      // acarreo (add/sub) o bit desplazado (sll/srl)
  bool overflow;    // overflow con signo (solo add/sub)
  bool zero;
  bool negative;
};

// Ejecuta la ALU para a,b,op sin tocar el estado guardado (usado por "alu all").
AluResult aluCoreCompute(uint8_t a, uint8_t b, uint8_t op);

// Estado guardado (A, B, OP activos).
void      aluCoreSetA(uint8_t v);
void      aluCoreSetB(uint8_t v);
void      aluCoreSetOp(uint8_t op);
uint8_t   aluCoreGetA();
uint8_t   aluCoreGetB();
uint8_t   aluCoreGetOp();

AluResult aluCoreRun();           // ejecuta con el estado guardado
AluResult aluCoreLastResult();
bool      aluCoreHasResult();

bool        aluCoreOpFromName(const char *name, uint8_t *outOp);  // "add".."sltu"
const char *aluCoreMnemonic(uint8_t op);
const char *aluCoreFormula(uint8_t op);
const char *aluCoreAsmInstr(uint8_t op);
