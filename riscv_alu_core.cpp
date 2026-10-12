#include "Drivers/riscv_alu_core.h"
#include <string.h>
#include <stdlib.h>

// Subrutina real en ensamblador RISC-V (riscv_alu.S).
// Entrada: a0=A, a1=B, a2=op. Salida (uint64_t): a0=resultado, a1=flags
// (bit0=acarreo/prestamo/bit desplazado, bit1=overflow con signo).
extern "C" uint64_t riscv_alu_asm(uint32_t a, uint32_t b, uint32_t op);

static const char *const MNEM[ALU_OP_COUNT]    = {"ADD","SUB","AND","OR","XOR","SLL","SRL","SLTU"};
static const char *const FORMULA[ALU_OP_COUNT] = {"A + B","A - B","A & B","A | B","A ^ B","A << B","A >> B","A < B"};
static const char *const ASMI[ALU_OP_COUNT]    = {
  "add a0,a1,a2", "sub a0,a1,a2", "and a0,a1,a2", "or a0,a1,a2",
  "xor a0,a1,a2", "sll a0,a1,a2", "srl a0,a1,a2", "sltu a0,a1,a2"
};

static uint8_t   curA = 0, curB = 0, curOp = ALU_OP_ADD;
static AluResult lastResult = {0, false, false, true, false};
static bool      hasResult = false;

AluResult aluCoreCompute(uint8_t a, uint8_t b, uint8_t op) {
  uint64_t r = riscv_alu_asm(a, b, op & 7);   // <- instrucciones RISC-V reales
  AluResult out;
  out.result   = (uint8_t)(r & 0xFF);
  out.carry    = (bool)((r >> 32) & 1);
  out.overflow = (bool)((r >> 33) & 1);
  out.zero     = (out.result == 0);
  out.negative = (bool)((out.result >> 7) & 1);
  return out;
}

void    aluCoreSetA(uint8_t v)   { curA = v; }
void    aluCoreSetB(uint8_t v)   { curB = v; }
void    aluCoreSetOp(uint8_t op) { curOp = op & 7; }
uint8_t aluCoreGetA()  { return curA; }
uint8_t aluCoreGetB()  { return curB; }
uint8_t aluCoreGetOp() { return curOp; }

AluResult aluCoreRun() {
  lastResult = aluCoreCompute(curA, curB, curOp);
  hasResult = true;
  return lastResult;
}
AluResult aluCoreLastResult() { return lastResult; }
bool      aluCoreHasResult()  { return hasResult; }

bool aluCoreOpFromName(const char *name, uint8_t *outOp) {
  for (int i = 0; i < ALU_OP_COUNT; i++) {
    if (strcasecmp(name, MNEM[i]) == 0) { *outOp = (uint8_t)i; return true; }
  }
  return false;
}

const char *aluCoreMnemonic(uint8_t op) { return MNEM[op & 7]; }
const char *aluCoreFormula(uint8_t op)  { return FORMULA[op & 7]; }
const char *aluCoreAsmInstr(uint8_t op) { return ASMI[op & 7]; }
