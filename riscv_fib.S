// int riscv_fib_asm(int n)
// 
//   a0        = argumento de entrada / valor de retorno
//   ra  (x1)  = direccion de retorno (la pone el "call"/jal del que llama)
//   sp  (x2)  = puntero de pila
//   s0,s1     = registros "callee-saved" (si los uso, los tengo que
//               guardar y restaurar yo mismo antes de salir)
//   t0,t1,t2  = temporales, cualquier subrutina los puede pisar

    .text
    .global riscv_fib_asm
    .align  2

riscv_fib_asm:
    addi    sp, sp, -16      // abrir marco de pila (16 bytes, alineado)
    sw      ra, 12(sp)       // guardar direccion de retorno
    sw      s0,  8(sp)       // guardar s0 (aqui conservamos "n")
    sw      s1,  4(sp)       // guardar s1 (aqui conservamos fib(n-1))

    mv      s0, a0           // s0 = n   (a0 se va a pisar en las llamadas)

    mv      a1, sp           // a1 = sp actual, para el log
    call    riscv_trace_enter   // riscv_trace_enter(n=a0, sp=a1)

    li      t0, 2
    blt     s0, t0, base_case    // if (n < 2) ir directo al caso base

    addi    a0, s0, -1
    call    riscv_fib_asm        // a0 = fib(n-1)   <-- llamada recursiva real
    mv      s1, a0                // s1 = fib(n-1)

    addi    a0, s0, -2
    call    riscv_fib_asm         // a0 = fib(n-2)   <-- otra llamada recursiva

    add     a0, a0, s1            // a0 = fib(n-1) + fib(n-2)
    j       done

base_case:
    mv      a0, s0                // caso base: fib(0)=0, fib(1)=1 -> retorna n

done:
    mv      s1, a0                 // guardar el resultado (sobrevive la llamada)
    mv      a0, s0                 // arg0 = n
    mv      a1, s1                 // arg1 = resultado
    call    riscv_trace_exit       // riscv_trace_exit(n, resultado)
    mv      a0, s1                 // restaurar el resultado real en a0

    lw      s1, 4(sp)              // restaurar registros guardados
    lw      s0, 8(sp)
    lw      ra, 12(sp)
    addi    sp, sp, 16             // cerrar el marco de pila
    ret                            // jr ra -- volver al que llamo
