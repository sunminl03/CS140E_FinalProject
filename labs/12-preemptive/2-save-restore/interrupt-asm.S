#include "rpi-asm.h"

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 1: used by <2-simple-save-test.c>
@

@ terminal syscall: print the registers we have.
syscall_reg_save:
    @asm_todo("save all 17 registers onto stack: pass pointer to syscall_handler");
    add sp, sp, #4
    stm sp, {r0-r14}^
    mov r0, lr
    mrs r1, spsr
    mov r14, sp 
    add r14, r14, #60
    stm r14, {r0, r1}
    mov r0, sp
    b syscall_handler
    asm_not_reached();

@ use rfe to load <pc> and <cpsr> from the 2 entry
@ array passed in <r0>.
MK_FN(rfe_asm)
    mov sp, r0
    rfe sp
    asm_not_reached();

@ for test 1.
.align 5; 
.globl syscall_reg_save_vec
syscall_reg_save_vec:
    b unhandled_reset
    b unhandled_undefined_instruction
    b syscall_reg_save
    b unhandled_prefetch_abort
    b unhandled_data_abort
    b unhandled_interrupt
    asm_not_reached();

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ used by the rest of tests.

@ don't forget to fix lr!
prefetch_reg_save:
    @ at this stage, my r14 is set to be the code line that called interrupt 
    mov sp, #INT_STACK_ADDR

    sub r14, r14, #4
    stm sp, {r0-r14}^
    mov r0, r14
    mrs r1, spsr
    mov r12, sp 
    add r12, r12, #60
    stm r12, {r0, r1}
    mov r0, sp
    b prefetch_abort_handler
    asm_not_reached();

.align 5; 
.globl simple_reg_save_restore_vec
simple_reg_save_restore_vec:
    b unhandled_reset
    b unhandled_undefined_instruction
    b unhandled_swi
    b prefetch_reg_save
    b unhandled_data_abort
    b unhandled_interrupt
    asm_not_reached();

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ weak symbols used not all tests define all handlers.

@ so we can compile if they don't provide
.weak syscall_handler
MK_FN(syscall_handler)
    asm_not_reached();

@ so we can compile if they don't provide
.weak prefetch_abort_handler
MK_FN(prefetch_abort_handler)
    asm_not_reached();
