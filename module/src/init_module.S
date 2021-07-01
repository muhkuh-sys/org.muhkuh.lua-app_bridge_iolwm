	.section .init_code, "ax"

	.extern module

	.global start

#include "asic_types.h"

@--------------------------------------

@ The Cortex-M4 in the netX90 is a THUMB-only CPU.
@ All others start in 32 bit mode.
#if ASIC_TYP==ASIC_TYP_NETX90_MPW || ASIC_TYP==ASIC_TYP_NETX90 || ASIC_TYP_NETX90_MPW_APP || ASIC_TYP_NETX90_APP
	.syntax unified
	.thumb
#else
	.arm
#endif


    .func start, start
    .type start, STT_FUNC
start:
@----------------------------------------------------------------------------
@
@ Save the registers.
@
	@ Save all working register.
	push    {r4, r5, r6, lr}


@----------------------------------------------------------------------------
@
@ Only clear the .bss section if the command in r0 is "initialize" (0).
@
	cmp     r0, #0
	bne     clear_bss_end
	mov     r4, #0
	ldr     r5, =__bss_start__
	ldr     r6, =__bss_end__
clear_bss:
	str     r4, [r5], #4
	cmp     r5, r6
	bmi     clear_bss
clear_bss_end:


@----------------------------------------------------------------------------
@
@ Call the main routine.
@
	@ Call the main routine.
	bl      module

	@ Now the result value is in r0.


@----------------------------------------------------------------------------
@
@ Restore the registers.
@
	@ Restore the working register.
	pop     {r4, r5, r6, lr}
	bx      lr

    .endfunc

@--------------------------------------

  .end
