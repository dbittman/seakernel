[BITS 64]
[ORG 0x2100]

mov esp, 0x2500
xor rax, rax
mov rcx, 12345678abcdefh
lidt [idtr]
pushfq
pop rax
or rax, 100000000b
push rax
popf
mov cr3, rcx
hlt

ALIGN 16
idtr:
	dw 0x100 - 1
	dq idt_start

ALIGN 32
idt_start:
	dw 0
	dw 0
	db 0
	db 0
	dw 0
	dd 0
	dd 0

	; int 1
	dw 0x3100 ; offset low bits
	dw 0x8    ; CS selector
	db 0      ; zero
	db 0x8e   ; type & attributes (interrupt, present)
	dw 0      ; offset middle bits
	dd 0      ; offset high bits
	dd 0      ; zero

idt_end:

