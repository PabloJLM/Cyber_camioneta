#pragma once
#include <stdint.h>


#define ALU_OP_ADD   0
#define ALU_OP_SUB   1
#define ALU_OP_AND   2
#define ALU_OP_OR    3
#define ALU_OP_XOR   4
#define ALU_OP_SLL   5
#define ALU_OP_SRL   6
#define ALU_OP_SLTU  7
#define ALU_OP_COUNT 8

struct AluResult { //flags y resultado
  uint8_t result;
  bool carry;      
  bool overflow;    
  bool zero;
  bool negative;
};


AluResult aluCoreCompute(uint8_t a, uint8_t b, uint8_t op);


void      aluCoreSetA(uint8_t v);
void      aluCoreSetB(uint8_t v);
void      aluCoreSetOp(uint8_t op);
uint8_t   aluCoreGetA();
uint8_t   aluCoreGetB();
uint8_t   aluCoreGetOp();

AluResult aluCoreRun();           
AluResult aluCoreLastResult();
bool      aluCoreHasResult();

bool        aluCoreOpFromName(const char *name, uint8_t *outOp);  // "add".."sltu" tira la operacion a realizar 
const char *aluCoreMnemonic(uint8_t op);
const char *aluCoreFormula(uint8_t op);
const char *aluCoreAsmInstr(uint8_t op);
