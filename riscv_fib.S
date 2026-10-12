//   a0        = argumento de entrada / valor de retorno
//   ra  (x1)  = direccion de retorno (la pone el "call"/jal del que llama)
//   sp  (x2)  = puntero de pila
//   s0,s1     = registros 
//   t0,t1,t2  = temporales, cualquier subrutina los puede pisar

    .text
    .global riscv_fib_asm
    .align  2

riscv_fib_asm:
    addi    sp, sp, -16      // abre el stack (16 bytes)
    sw      ra, 12(sp)       // guardar direccion de retorno
    sw      s0,  8(sp)       // guardar s0 (n)
    sw      s1,  4(sp)       // guardar s1 (fib(n-1))

    mv      s0, a0           // s0 = n 

    mv      a1, sp           
    call    riscv_trace_enter  

    li      t0, 2
    blt     s0, t0, base_case    // if (n < 2) ir directo al caso base

    addi    a0, s0, -1
    call    riscv_fib_asm        // a0 = fib(n-1)   
    mv      s1, a0                // s1 = fib(n-1)

    addi    a0, s0, -2
    call    riscv_fib_asm         // a0 = fib(n-2)  

    add     a0, a0, s1            // a0 = fib(n-1) + fib(n-2)
    j       done

base_case:
    mv      a0, s0                // caso base fib(0)=0 y fib(1)=1

done:
    mv      s1, a0                
    mv      a0, s0                 
    mv      a1, s1                 // arg1 = resultado
    call    riscv_trace_exit       
    mv      a0, s1                 

    lw      s1, 4(sp)              
    lw      s0, 8(sp)
    lw      ra, 12(sp)
    addi    sp, sp, 16             
    ret                            
