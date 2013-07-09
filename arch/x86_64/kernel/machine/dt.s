[GLOBAL gdt_flush]
gdt_flush:
	mov rax, rdi  ; Get the pointer to the GDT, passed as a parameter.
	lgdt [rax]        ; Load the new GDT pointer
	mov ax, 0x10      ; 0x10 is the offset in the GDT to our data segment
	mov ds, ax        ; Load all data segment selectors
	mov es, ax
	mov fs, ax
	mov ss, ax
	; flush the CS segment with iretq
    mov     rcx, qword .reloadcs
    mov     rsi, rsp
    
    push    rax             ; new SS
    push    rsi             ; new RSP
    push    2               ; new FLAGS
    push    0x8             ; new CS
    push    rcx             ; new RIP
    iretq
.reloadcs:
	ret

[GLOBAL idt_flush] 
idt_flush:
	mov rax, rdi  ; Get the pointer to the IDT, passed as a parameter. 
	lidt [rax]        ; Load the IDT pointer.
	ret
[GLOBAL tss_flush]
tss_flush:
	mov ax, 0x2B      ; Load the index of our TSS structure - The index is
	; 0x28, as it is the 5th selector and each is 8 bytes
	; long, but we set the bottom two bits (making 0x2B)
	; so that it has an RPL of 3, not zero.
	ltr ax            ; Load 0x2B into the task state register.
	ret 
