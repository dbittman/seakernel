STACKSIZE equ 0x20000

global start64
extern kmain
extern boot_pml4
bits 64
section .text
start64:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov rsp, initial_kernel_stack + STACKSIZE

	; unmap lower physical memory from lower address space
	mov rax, boot_pml4
	mov qword [rax], 0
	invlpg [0]

	mov rsi, rsp
	mov rdi, 0xffff800000000000
	add rdi, rbx
	call kmain

	cli
	hlt

section .bss
align 0x20000
global initial_kernel_stack
initial_kernel_stack:
   resb STACKSIZE               ; reserve 16k stack on a quadword boundary

