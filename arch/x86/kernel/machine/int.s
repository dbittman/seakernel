; int.s : Defines assmebly entry routines for interrupt handling
; at the processor level. The source of these interrupts is irrelevant,
; at this point they are all handled the same way.

; THE C COUNTER PARTS OF THESE ARE DEFINED IN ISR.H
; !! If you edit these, make sure you also update the ones in isr.h
; !! or some interesting bugs may appear...
%define IPI_SCHED    0x90
%define IPI_SHUTDOWN 0xA0
%define IPI_TLB_ACK  0xB0
%define IPI_TLB      0xC0
%define IPI_DEBUG    0xD0
%define IPI_PANIC    0xE0

; isr's that don't push an error code need to have a dummy code
; pushed so that the structure aligns properly later...
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli                         ; Disable interrupts firstly.
    push byte 0                 ; Push a dummy error code.
    push byte %1                ; Push the interrupt number.
    jmp isr_entry_code         ; Go to our common handler code.
%endmacro

; these isr's already push an error code.
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli                         ; Disable interrupts.
    push byte %1                ; Push the interrupt number
    jmp isr_entry_code
%endmacro

; This macro creates functions for an IRQ - the first parameter is
; the IRQ number, the second is the ISR number it is remapped to.
%macro IRQ 2
  global irq%1
  irq%1:
    cli
    push byte 0 ; dummy error code
    push byte %2
    jmp irq_entry_code
%endmacro

; interprocessor interrupts (no error code here either)
%macro IPI 2
  global ipi_%1
  ipi_%1:
    cli
    push byte 0 ; dummy error code
    push %2
    jmp ipi_entry_code
%endmacro

; heres the actual common asm entry code for interrupts.
%macro INT_ENTRY_CODE 2
%1_entry_code:
    pusha                    ; Pushes processor registers

    mov ax, ds               ; Lower 16-bits of eax = ds.
    push eax                 ; save the data segment descriptor
    
    mov ax, 0x10  ; load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax

    call %2 ; calls the C-code handler
    pop ebx        ; reload the original data segment descriptor
    mov ds, bx
    mov es, bx
    mov fs, bx

    popa                     ; Pops edi,esi,ebp...
    add esp, 8     ; Cleans up the pushed error code and pushed ISR number
    iretd           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
%endmacro


align 4
; In isr.c
extern isr_handler
extern irq_handler
extern entry_syscall_handler
extern ipi_handler

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

IRQ   0,    32
IRQ   1,    33
IRQ   2,    34
IRQ   3,    35
IRQ   4,    36
IRQ   5,    37
IRQ   6,    38
IRQ   7,    39
IRQ   8,    40
IRQ   9,    41
IRQ  10,    42
IRQ  11,    43
IRQ  12,    44
IRQ  13,    45
IRQ  14,    46
IRQ  15,    47

; interprocessor interrupts (only matter in SMP)
IPI  panic    , IPI_PANIC
IPI  shutdown , IPI_SHUTDOWN
IPI  sched    , IPI_SCHED
IPI  tlb      , IPI_TLB
IPI  tlb_ack  , IPI_TLB_ACK
IPI  debug    , IPI_DEBUG

; syscall
global isr80
isr80:
    cli                         ; Disable interrupts.
    push byte 0
    push 0x80                ; Push the interrupt number
    jmp syscall_entry_code

global isr_ignore
isr_ignore:
	iretd

; the asm entry handlers
INT_ENTRY_CODE isr, isr_handler
INT_ENTRY_CODE irq, irq_handler
INT_ENTRY_CODE ipi, ipi_handler
INT_ENTRY_CODE syscall, entry_syscall_handler
