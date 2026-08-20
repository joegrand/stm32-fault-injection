@ https://github.com/CTXz/stm32f1-picopwner
@ https://github.com/lolwheel/stm32f4-rdp-workaround
@ https://github.com/xobs/stimpik/tree/stm32f429-v2/payload

.global _start
.syntax divided
.thumb

.section .vector_table
vtable:
sp_addr:.word 0x20001000 	@ 0x00: Stack Pointer address
	.word _stage1+1			@ 0x04: Reset Handler (Enters payload start)
	.word isrNMI+1 			@ 0x08: NMI Handler
	.word isrHardfault+1	@ 0x0C: HardFault Handler
	.word isrMemMan+1 		@ 0x10: MemManage Handler
	.word 0					@ 0x14: BusFault not handled
	.word 0					@ 0x18: UsageFault not handled
	.word 0					@ 0x1C: Unused/Reserved
	.word _stage2+1			@ 0x20: FPB reset remap (Enters payload start)

.section .text
.thumb
_start:
_stage1:
	@ Set FPB to remap the reset vector to SRAM
	ldr r0, =0xe0002000   @ FP_CTRL, see https://developer.arm.com/documentation/ddi0337/e/System-Debug/FPB/FPB-programmer-s-model
    movs r1, #3           @ FP_CTRL: Enable FPB                                                                        
    movs r2, #0x20        @ FP_REMAP: Remap to stage 2 vtable entry (0x20)                              
    movs r3, #0x05        @ FP_COMP0: Set COMP0 to (rst_vector | EN) = 0x05
    stm r0!, {r1, r2, r3} @ Apply to FPB                                                                

	@ Set up the stack pointer
	ldr r0, =sp_addr
	ldr r0, [r0]
	mov sp, r0

	cpsie if @ Enable interrupts
	
	@ Configure VTOR
	ldr r0, =0xe000ED08 @ VTOR, see https://developer.arm.com/documentation/dui0552/a/cortex-m3-peripherals/system-control-block
	ldr r1, =vtable	    @ Address of the vector table
	str r1, [r0] 	    @ Set VTOR to point to our vector table

	@ Clear BSS
	ldr r0, =_bss_end
	ldr r1, =_bss_start

clrloop:
	cmp r0, r1
	beq clrloop_done
	mov r3, #0
	strd r3, r3, [r1]
	add r1, r1, #8
	b clrloop
	
clrloop_done:

	bl main_stage1

@ If main returns, we'll just loop forever
infloop:
	b infloop

_stage2:               
	@ Set up the stack pointer
	ldr r0, =sp_addr
	ldr r0, [r0]
	mov sp, r0

	cpsie if @ Enable interrupts
	
	@ Configure VTOR
	ldr r0, =0xe000ED08 @ VTOR, see https://developer.arm.com/documentation/dui0552/a/cortex-m3-peripherals/system-control-block
	ldr r1, =vtable	    @ Address of the vector table
	str r1, [r0] 	    @ Set VTOR to point to our vector table

	@ Clear BSS
	ldr r0, =_bss_end
	ldr r1, =_bss_start

clrloop2:
	cmp r0, r1
	beq clrloop_done2
	mov r3, #0
	strd r3, r3, [r1]
	add r1, r1, #8
	b clrloop2

clrloop_done2:
	bl main_stage2

@ -- Exception Handler --
.section .text
isrNMI:
isrHardfault:
isrMemMan:
	mrs r0, IPSR
	b alert_crash
