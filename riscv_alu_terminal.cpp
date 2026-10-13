#include "Drivers/riscv_alu_terminal.h"
#include "Drivers/riscv_alu_core.h"
#include <string.h>

static void bin8(char *out, uint8_t v) {
  for (int i = 7, k = 0; i >= 0; i--) out[k++] = ((v >> i) & 1) ? '1' : '0';
  out[8] = 0;
}

static void hex2(Print &c, uint8_t v) {
  c.print("0x");
  if (v < 16) c.print('0');
  c.print(v, HEX);
}

static void printDiagram(Print &c, uint8_t a, uint8_t b, uint8_t op, const AluResult &r) {
  char ab[9], bb[9], rb[9];
  bin8(ab, a); bin8(bb, b); bin8(rb, r.result);

  c.println();
  c.print(F("   A = "));  c.print(a);  c.print(F("  ("));  c.print(ab);  c.println(F(")  -\\"));
  c.print(F("   OP  ")); c.println(aluCoreMnemonic(op));
  c.print(F("         ------->  [ ALU ]  ---> R = ")); c.print(r.result);
  c.print(F("  (")); c.print(rb); c.println(F(")"));
  c.print(F("   B = "));  c.print(b);  c.print(F("  ("));  c.print(bb);  c.println(F(")  -/"));
  c.print(F("   Z=")); c.print(r.zero ? 1 : 0);
  c.print(F("  N="));  c.print(r.negative ? 1 : 0);
  c.print(F("  OVF=")); c.print(r.overflow ? 1 : 0);
  c.print(F("  C=")); c.println(r.carry ? 1 : 0);
}


static void printAsmEcho(Print &c, uint8_t a, uint8_t b, uint8_t op, const AluResult &r) {
  c.println(F("   --- riscv_alu.S ---"));
  c.print(F("   entrada   a0="));  hex2(c, a);
  c.print(F("  a1="));  hex2(c, b);
  c.print(F("  a2="));  c.println(op);
  c.print(F("   instr     ")); c.println(aluCoreAsmInstr(op));
  c.print(F("   salida    a0="));  hex2(c, r.result);
  c.print(F(" (result)  a1="));
  c.print((r.carry ? 1 : 0) | ((r.overflow ? 1 : 0) << 1));
  c.print(F(" (bit0=C bit1=OVF)"));
  c.println();
}

static void printShow(Print &c) {
  uint8_t a = aluCoreGetA(), b = aluCoreGetB(), op = aluCoreGetOp();
  c.println();
  c.print(F("   A="));  c.print(a);
  c.print(F("  B="));  c.print(b);
  c.print(F("  OP="));  c.print(aluCoreMnemonic(op));
  c.print(F("  ("));  c.print(aluCoreFormula(op));  c.println(F(")"));
  if (aluCoreHasResult()) {
    AluResult r = aluCoreLastResult();
    c.print(F("   ultimo resultado: "));  c.println(r.result);
  }
}

static void printAll(Print &c) {
  uint8_t a = aluCoreGetA(), b = aluCoreGetB();
  c.println();
  c.print(F("   A="));  c.print(a);  c.print(F("  B="));  c.println(b);
  c.println(F("   OP    FORMULA   R     bin        Z N OVF C"));
  for (uint8_t op = 0; op < ALU_OP_COUNT; op++) {
    AluResult r = aluCoreCompute(a, b, op);
    char rb[9];
    bin8(rb, r.result);
    c.print(F("   "));
    c.print(aluCoreMnemonic(op));
    for (int i = strlen(aluCoreMnemonic(op)); i < 5; i++) c.print(' ');
    c.print(' ');
    const char *f = aluCoreFormula(op);
    c.print(f);
    for (int i = strlen(f); i < 7; i++) c.print(' ');
    c.print(' ');
    char num[4]; snprintf(num, sizeof(num), "%u", r.result);
    c.print(num);
    for (int i = strlen(num); i < 4; i++) c.print(' ');
    c.print(' '); c.print(rb); c.print(F("  "));
    c.print(r.zero ? 1 : 0); c.print(' ');
    c.print(r.negative ? 1 : 0); c.print(' ');
    c.print(r.overflow ? 1 : 0); c.print(F("   "));
    c.println(r.carry ? 1 : 0);
  }
}

static void printHelp(Print &c) {
  c.println();
  c.println(F("   == comandos ALU (RISC-V real, riscv_alu.S) =="));
  c.println(F("   alu a <0-255>     fija el operando A"));
  c.println(F("   alu b <0-255>     fija el operando B"));
  c.println(F("   alu op <nombre>   add|sub|and|or|xor|sll|srl|sltu"));
  c.println(F("   alu run           ejecuta la operacion actual (dibujo + eco asm)"));
  c.println(F("   alu all           corre las 8 operaciones con A y B actuales"));
  c.println(F("   alu show          muestra A, B, OP y el ultimo resultado"));
  c.println(F("   alu help          esta ayuda"));
}

bool aluTerminalHandle(Print &c, const String &cmd) {
  if (cmd == "alu" || cmd == "alu help") { printHelp(c); return true; }
  if (!cmd.startsWith("alu ")) return false;

  String rest = cmd.substring(4);
  rest.trim();

  if (rest.startsWith("a ")) {
    long v = rest.substring(2).toInt();
    if (v < 0) v = 0; if (v > 255) v = 255;
    aluCoreSetA((uint8_t)v);
    c.print(F("   ok: A -> ")); c.println(v);
  } else if (rest.startsWith("b ")) {
    long v = rest.substring(2).toInt();
    if (v < 0) v = 0; if (v > 255) v = 255;
    aluCoreSetB((uint8_t)v);
    c.print(F("   ok: B -> ")); c.println(v);
  } else if (rest.startsWith("op ")) {
    String name = rest.substring(3);
    name.trim();
    uint8_t op;
    if (aluCoreOpFromName(name.c_str(), &op)) {
      aluCoreSetOp(op);
      c.print(F("   ok: OP -> ")); c.println(aluCoreMnemonic(op));
    } else {
      c.println(F("   uso: alu op add|sub|and|or|xor|sll|srl|sltu"));
    }
  } else if (rest == "run") {
    AluResult r = aluCoreRun();
    printDiagram(c, aluCoreGetA(), aluCoreGetB(), aluCoreGetOp(), r);
    printAsmEcho(c, aluCoreGetA(), aluCoreGetB(), aluCoreGetOp(), r);
  } else if (rest == "all") {
    printAll(c);
  } else if (rest == "show") {
    printShow(c);
  } else {
    c.println(F("   uso: alu a|b|op|run|all|show   (alu help para detalle)"));
  }
  return true;
}
