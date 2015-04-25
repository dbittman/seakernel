[BITS 64]
[ORG 0x2100]

cli
mov rsp, 0x2500 ; we'll need a stack for the interrupt handler
mov rax, qword [0x1000] ; read the monitor's CR3 from somewhere in the trap code
lidt [idtr] ; load the interrupt table
pushfq ; get the flags
or qword [rsp], 100000000b ; set TF
popf ; set the flags
mov cr3, rax ; change address spaces
; <--- TF triggers interrupt here
loop:
jmp loop

; IDT table contains a null entry for interrupt 0, and the entry for interrupt 1
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
	dw 0x3100 ; offset low bits (some location to jump to at interrupt)
	dw 0x8    ; CS selector
	db 0      ; zero
	db 0x8e   ; type & attributes (interrupt, present)
	dw 0      ; offset middle bits
	dd 0      ; offset high bits
	dd 0      ; zero

idt_end:
