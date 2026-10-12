//   ALU de 8 bits ejecutada con instrucciones RISC-V reales.
//   Entrada:  a0 = A, a1 = B, a2 = operacion
//   Salida:   a0 = resultado (8 bits)
//             a1 = bit0: acarreo/prestamo/bit desplazado, bit1: overflow con signo
//   (uint64_t en C: a0 = parte baja, a1 = parte alta)
//
//   op: 0 add, 1 sub, 2 and, 3 or, 4 xor, 5 sll, 6 srl, 7 sltu

    .text
    .global riscv_alu_asm
    .align  2

riscv_alu_asm:
    andi    a0, a0, 255          // A de 8 bits
    andi    a1, a1, 255          // B de 8 bits

    beqz    a2, alu_add
    addi    a2, a2, -1
    beqz    a2, alu_sub
    addi    a2, a2, -1
    beqz    a2, alu_and
    addi    a2, a2, -1
    beqz    a2, alu_or
    addi    a2, a2, -1
    beqz    a2, alu_xor
    addi    a2, a2, -1
    beqz    a2, alu_sll
    addi    a2, a2, -1
    beqz    a2, alu_srl
    addi    a2, a2, -1
    beqz    a2, alu_sltu
    j       alu_add              // op invalida -> add

alu_add:
    add     t1, a0, a1
    andi    t3, t1, 255          // resultado
    srli    t2, t1, 8
    andi    t2, t2, 1            // acarreo = bit 8
    xor     t4, a0, t3
    xor     t5, a1, t3
    and     t4, t4, t5           // overflow: A y B con igual signo, resultado distinto
    srli    t4, t4, 7
    andi    t4, t4, 1
    slli    t4, t4, 1
    or      a1, t2, t4
    mv      a0, t3
    ret

alu_sub:
    sltu    t2, a0, a1           // prestamo = A < B
    sub     t1, a0, a1
    andi    t3, t1, 255          // resultado
    xor     t4, a0, a1
    xor     t5, a0, t3
    and     t4, t4, t5           // overflow: A y B con distinto signo, resultado distinto de A
    srli    t4, t4, 7
    andi    t4, t4, 1
    slli    t4, t4, 1
    or      a1, t2, t4
    mv      a0, t3
    ret

alu_and:
    and     a0, a0, a1
    li      a1, 0
    ret

alu_or:
    or      a0, a0, a1
    li      a1, 0
    ret

alu_xor:
    xor     a0, a0, a1
    li      a1, 0
    ret

alu_sll:
    andi    t0, a1, 7            // desplazamiento 0..7
    sll     t1, a0, t0
    beqz    t0, alu_sll_nc
    li      t2, 8
    sub     t2, t2, t0
    srl     t2, a0, t2           // ultimo bit que salio por la izquierda
    andi    a1, t2, 1
    j       alu_sll_end
alu_sll_nc:
    li      a1, 0
alu_sll_end:
    andi    a0, t1, 255
    ret

alu_srl:
    andi    t0, a1, 7
    srl     t1, a0, t0
    beqz    t0, alu_srl_nc
    addi    t2, t0, -1
    srl     t2, a0, t2           // ultimo bit que salio por la derecha
    andi    a1, t2, 1
    j       alu_srl_end
alu_srl_nc:
    li      a1, 0
alu_srl_end:
    mv      a0, t1
    ret

alu_sltu:
    sltu    t1, a0, a1
    mv      a0, t1
    li      a1, 0
    ret
